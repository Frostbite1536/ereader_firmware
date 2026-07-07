// InkPad — Xteink X3/X4 word processor + flashcards + firmware-swap launcher.
// Boot order matters: X3/X4 fingerprint BEFORE SD and display bring-up, so
// both read the right board profile (see freeink-sdk XteinkDetect docs).

#include <Arduino.h>
#include <BatteryMonitor.h>
#include <BoardConfig.h>
#include <EInkDisplay.h>
#include <FreeInkUIDisplayTarget.h>
#include <FreeInkUIInputManager.h>
#include <InputManager.h>
#include <PowerManager.h>
#include <SDCardManager.h>
#include <XteinkDetect.h>

#include "AppContext.h"
#include "Settings.h"
#include "apps/FlashcardsApp.h"
#include "apps/FlasherApp.h"
#include "apps/LauncherApp.h"
#include "apps/WriterApp.h"
#include "fonts/WriterFonts.h"

namespace {

using freeink::ui::InputSnapshot;
using freeink::ui::RefreshHint;

constexpr uint32_t IDLE_SLEEP_MS = 15UL * 60UL * 1000UL;
constexpr uint32_t POWER_BTN_GUARD_MS = 2000;  // ignore the wake press at boot

// X3 and X4 share a pinout; ACTIVE defaults to the X4 profile until detection.
EInkDisplay display(BoardConfig::XTEINK_X4.display.sclk, BoardConfig::XTEINK_X4.display.mosi,
                    BoardConfig::XTEINK_X4.display.cs, BoardConfig::XTEINK_X4.display.dc,
                    BoardConfig::XTEINK_X4.display.rst, BoardConfig::XTEINK_X4.display.busy);
InputManager input;

LauncherApp launcher;
WriterApp writer;
FlashcardsApp flashcards;
FlasherApp flasher;
App* apps[APP_COUNT] = {&launcher, &writer, &flashcards, &flasher};
App* current = nullptr;

// Constructed in setup() once the display geometry is known.
freeink::ui::DisplayTarget* rawTarget = nullptr;
freeink::ui::InvertedDrawTarget* target = nullptr;
UiApp* ui = nullptr;
BatteryMonitor* battery = nullptr;
AppContext* ctx = nullptr;

uint32_t lastActivityMs = 0;

void pushFrame(RefreshHint hint) {
  switch (hint) {
    case RefreshHint::Fast:
      ctx->refresh(EInkDisplay::FAST_REFRESH);
      break;
    case RefreshHint::Full:
    case RefreshHint::Clean:
      ctx->refresh(EInkDisplay::FULL_REFRESH);
      break;
    case RefreshHint::None:
      break;
  }
}

void switchApp(AppId next) {
  if (current) current->onExit();
  current = apps[next];
  SETTINGS.lastApp = next;
  SETTINGS.save();
  current->onEnter();  // installs the screen + invalidates Full
}

// Honor a switch requested during tick() or from an action handler.
bool serviceAppSwitch() {
  if (ctx->pendingApp == APP_COUNT) return false;
  const AppId next = ctx->pendingApp;
  ctx->pendingApp = APP_COUNT;
  switchApp(next < APP_COUNT ? next : APP_LAUNCHER);
  return true;
}

[[noreturn]] void goToSleep() {
  Serial.println("[MAIN] sleeping");
  if (current) current->onExit();  // Writer saves here — autosave-on-sleep invariant
  SETTINGS.save();
  // The panel retains the last frame through deep sleep, so the user's text
  // stays visible on the "off" device.
  display.deepSleep();
  freeink::PowerManager::deepSleepUntilPowerButton();
}

}  // namespace

void setup() {
  Serial.begin(115200);
  SETTINGS.load();

  // 1. Fingerprint X3 vs X4 (I2C probe) before any profile-dependent init.
  const bool isX3 = freeink::selectXteinkDevice();
  if (isX3) display.setDisplayX3();
  launcher.setModelName(isX3 ? "Xteink X3" : "Xteink X4");

  // 2. Hardware bring-up.
  display.begin();
  input.begin();
  if (!SdMan.begin()) {
    Serial.println("[MAIN] SD mount failed");
  } else {
    SdMan.ensureDirectoryExists("/docs");
    SdMan.ensureDirectoryExists("/decks");
    SdMan.ensureDirectoryExists("/firmware");
  }
  battery = new BatteryMonitor();

  // 3. UI runtime over the display's framebuffer (logical portrait).
  rawTarget = new freeink::ui::DisplayTarget(display.getFrameBuffer(), display.getDisplayWidth(),
                                             display.getDisplayHeight(), display.getDisplayWidthBytes());
  fonts::installFonts(*rawTarget);
  target = new freeink::ui::InvertedDrawTarget(*rawTarget, SETTINGS.darkMode);
  ui = new UiApp(*target, rawTarget->deviceContext());
  // Metric tokens derived from the body font's line height — the static
  // defaultThemeTokens() metrics assume ~18px fonts and clip subtitle rows.
  ui->setTheme(freeink::ui::themeTokensForLineHeight(rawTarget->lineHeight(fonts::UI_BODY), fonts::UI_SMALL,
                                                     fonts::UI_BODY, fonts::UI_TITLE));

  ctx = new AppContext{display, *target, *ui, input, *battery};

  for (App* app : apps) app->begin(*ctx);

  // 4. Resume where the user left off (never straight into the flasher).
  AppId start = static_cast<AppId>(SETTINGS.lastApp);
  if (start >= APP_COUNT || start == APP_FLASHER) start = APP_LAUNCHER;
  switchApp(start);

  lastActivityMs = millis();
  Serial.printf("[MAIN] InkPad up on %s (%ux%u)\n", isX3 ? "X3" : "X4", display.getDisplayWidth(),
                display.getDisplayHeight());
}

void loop() {
  input.update();
  const InputSnapshot snap = freeink::ui::snapshotFrom(input);
  const bool anyButton = snap.confirm || snap.back || snap.focusNext || snap.focusPrev || snap.prev || snap.next;
  if (anyButton) lastActivityMs = millis();

  current->tick();  // BLE keys, app logic; may invalidate the UI or request a switch
  if (ctx->lastActivityMs > lastActivityMs) lastActivityMs = ctx->lastActivityMs;

  bool switched = serviceAppSwitch();

  if (ui->invalidated() || anyButton) {
    ui->render(snap);
    RefreshHint hint = ui->lastRenderRefreshHint();

    // Focus moves happen inside render() AFTER the frame was drawn, and the UI
    // does not self-invalidate for them — without this the highlight would
    // move invisibly (the re-render below draws it, then the panel updates).
    if (snap.focusNext || snap.focusPrev) ui->invalidate(RefreshHint::Fast);

    // An action handler may have changed state (or requested an app switch)
    // during dispatch: redraw before pushing so the panel never shows a stale
    // intermediate frame — e-paper refreshes are too expensive to waste.
    switched = serviceAppSwitch() || switched;
    if (ui->invalidated()) {
      ui->render(InputSnapshot{});
      const RefreshHint second = ui->lastRenderRefreshHint();
      if (static_cast<uint8_t>(second) > static_cast<uint8_t>(hint)) hint = second;
    }
    pushFrame(hint);
  } else if (switched) {
    ui->render(InputSnapshot{});
    pushFrame(ui->lastRenderRefreshHint());
  }

  // Sleep: launcher menu action, power button (guarded against the wake
  // press), or idle timeout. goToSleep() saves dirty work first.
  const bool powerPressed = input.wasPressed(InputManager::BTN_POWER) && millis() > POWER_BTN_GUARD_MS;
  if (launcher.sleepRequested() || powerPressed || millis() - lastActivityMs > IDLE_SLEEP_MS) {
    launcher.clearSleepRequest();
    goToSleep();
  }

  delay(5);  // input poll cadence; keeps the idle loop cool
}
