#pragma once

#include <BatteryMonitor.h>
#include <EInkDisplay.h>
#include <FreeInkApp.h>
#include <InputManager.h>

#include "DarkModeTarget.h"
#include "Settings.h"

// Shared UI runtime type. 48 interactions / 32 handlers covers the largest
// focus-driven screen (keyboard pairing: 8 rows + footer) plus every app's
// handlers registered at boot. The bigger lists (writer menu, file picker,
// decks, bins) are selection-driven and register nothing.
using UiApp = freeink::ui::FreeInkApp<48, 32>;

// AppId doubles as the launcher menu order and the NVS resume value.
enum AppId : uint8_t {
  APP_LAUNCHER = 0,
  APP_WRITER = 1,
  APP_FLASHCARDS = 2,
  APP_FLASHER = 3,
  APP_COUNT,
};

// Everything an app is allowed to touch. Apps hold this by reference and
// never reach for globals, so app code stays testable and boundaries stay
// visible (the highest-value bug-hunt surface).
struct AppContext {
  EInkDisplay& display;
  DarkModeTarget& target;  // dark-mode wrapper over DisplayTarget (dither-safe)
  UiApp& ui;
  InputManager& input;
  BatteryMonitor& battery;

  // Push the framebuffer to the panel. Apps use this instead of touching the
  // display directly so the refresh policy stays in one place.
  void refresh(EInkDisplay::RefreshMode mode) {
    // Dark mode inverts drawing; the panel is pushed as-is.
    display.displayBuffer(mode, /*turnOffScreen=*/true);
  }

  // Request a switch to another app; honored by main loop after this tick.
  void switchTo(AppId id) { pendingApp = id; }

  // Request a clean reboot (orientation change rebuilds the UI stack at
  // boot). Main loop runs onExit() first, so dirty Writer text is saved.
  void requestRestart() { restartPending = true; }

  // Apps bump this on non-button activity (BLE keystrokes) so the idle-sleep
  // timer in main sees typing as activity.
  void noteActivity() { lastActivityMs = millis(); }

  AppId pendingApp = APP_COUNT;  // APP_COUNT = no switch requested
  bool restartPending = false;
  uint32_t lastActivityMs = 0;
};

// One app = screen drawing + input handling + lifecycle. Handlers are
// registered once in begin() (ActionId ranges per app — see CLAUDE.md);
// onEnter() installs the app's screen function; tick() runs every loop.
class App {
 public:
  virtual ~App() = default;
  virtual void begin(AppContext& ctx) = 0;  // called once at boot
  virtual void onEnter() = 0;               // app becomes active
  virtual void tick() = 0;                  // every main-loop pass
  virtual void onExit() {}                  // leaving app OR device going to sleep
  virtual const char* name() const = 0;
};
