#include "FlasherApp.h"

#include <SDCardManager.h>

#include <cstdio>
#include <cstring>

#include "../flash/FirmwareFlasher.h"
#include "../fonts/WriterFonts.h"

namespace {
using namespace freeink::ui;

// ActionIds 40-49 (Flasher range, CLAUDE.md).
enum : ActionId {
  ACT_GO = 41,
  ACT_CANCEL = 42,
};
}  // namespace

void FlasherApp::begin(AppContext& ctx) {
  ctx_ = &ctx;

  // Handlers only set flags — the actual flash runs from tick(), outside the
  // UI render pass (handlers are dispatched inside FreeInkApp::render()).
  ctx.ui.on(ACT_GO, [](const ActionEvent&, void* self) {
    auto& f = *static_cast<FlasherApp*>(self);
    f.mode_ = Mode::Flashing;
    f.flashQueued_ = true;
    f.progressPct_ = 0;
    f.ctx_->ui.invalidate(RefreshHint::Fast);
  }, this);

  ctx.ui.on(ACT_CANCEL, [](const ActionEvent&, void* self) {
    auto& f = *static_cast<FlasherApp*>(self);
    f.mode_ = Mode::List;
    f.chosen_ = -1;
    f.ctx_->ui.invalidate(RefreshHint::Fast);
  }, this);
}

void FlasherApp::onEnter() {
  mode_ = Mode::List;
  chosen_ = -1;
  binSel_ = 0;
  binTop_ = 0;
  flashQueued_ = false;
  scanBins();
  ctx_->ui.setScreen(&FlasherApp::drawScreen, this, RefreshHint::Full);
}

void FlasherApp::tick() {
  auto& in = ctx_->input;
  auto& ui = ctx_->ui;

  if (flashQueued_) {
    flashQueued_ = false;
    runFlash();  // blocking; repaints progress via the callback. Does not
                 // return on success (device reboots into the new firmware).
    return;
  }

  switch (mode_) {
    case Mode::List:
      if (in.wasPressed(InputManager::BTN_BACK)) {
        ctx_->switchTo(APP_LAUNCHER);
        return;
      }
      if (binCount_ == 0) return;
      if (in.wasPressed(InputManager::BTN_DOWN) && binSel_ + 1 < binCount_) {
        binSel_++;
        ui.invalidate(RefreshHint::Fast);
      }
      if (in.wasPressed(InputManager::BTN_UP) && binSel_ > 0) {
        binSel_--;
        ui.invalidate(RefreshHint::Fast);
      }
      if (in.wasPressed(InputManager::BTN_CONFIRM)) {
        chosen_ = binSel_;
        mode_ = Mode::Confirm;
        ui.invalidate(RefreshHint::Fast);
      }
      break;
    case Mode::Confirm:
    case Mode::Failed:
      // The dialog's two options are the only interactions on screen; UP/DOWN
      // focuses them and CONFIRM dispatches (route() ignores confirm with no
      // focus, so entering the dialog can't instantly trigger an option).
      if (in.wasPressed(InputManager::BTN_BACK)) {
        mode_ = Mode::List;
        chosen_ = -1;
        ui.invalidate(RefreshHint::Fast);
      }
      break;
    case Mode::Flashing:
      break;  // no backing out mid-flash
  }
}

void FlasherApp::scanBins() {
  binCount_ = 0;
  SdMan.ensureDirectoryExists("/firmware");
  for (const String& name : SdMan.listFiles("/firmware", MAX_BINS * 2)) {
    if (binCount_ >= MAX_BINS) break;
    if (!name.endsWith(".bin")) continue;
    strlcpy(binNames_[binCount_], name.c_str(), sizeof(binNames_[0]));
    binCount_++;
  }
}

void FlasherApp::runFlash() {
  if (chosen_ < 0 || chosen_ >= binCount_) {  // belt: never index binNames_ out of range
    mode_ = Mode::List;
    ctx_->ui.invalidate(RefreshHint::Fast);
    return;
  }
  snprintf(pathBuf_, sizeof(pathBuf_), "/firmware/%s", binNames_[chosen_]);

  // Repaint helper: render the current screen state and push a fast refresh.
  auto pump = [this]() {
    ctx_->ui.invalidate(RefreshHint::Fast);
    ctx_->ui.render(InputSnapshot{});
    ctx_->refresh(EInkDisplay::FAST_REFRESH);
  };
  pump();  // show "Flashing... 0%"

  auto progress = [](size_t written, size_t total, void* vctx) {
    auto* self = static_cast<FlasherApp*>(vctx);
    const uint8_t pct = total ? static_cast<uint8_t>((written * 100) / total) : 0;
    self->progressPct_ = pct;
    // E-paper can't animate: repaint only every 10 points of progress.
    if (pct >= self->drawnPct_ + 10 || (pct == 100 && self->drawnPct_ != 100)) {
      self->drawnPct_ = pct;
      self->ctx_->ui.invalidate(RefreshHint::Fast);
      self->ctx_->ui.render(InputSnapshot{});
      self->ctx_->refresh(EInkDisplay::FAST_REFRESH);
    }
  };

  drawnPct_ = 0;
  const auto res = firmware_flash::flashFromSdPath(pathBuf_, progress, this);
  if (res == firmware_flash::Result::OK) {
    progressPct_ = 100;
    strlcpy(failMsg_, "", sizeof(failMsg_));
    ctx_->ui.invalidate(RefreshHint::Full);
    ctx_->ui.render(InputSnapshot{});
    ctx_->refresh(EInkDisplay::FULL_REFRESH);
    delay(300);
    ESP.restart();  // boots the freshly flashed firmware
  }

  // Validation or write failed: otadata is untouched, we are still the boot
  // target (INVARIANTS.md #7). Report and go back to the list.
  snprintf(failMsg_, sizeof(failMsg_), "Flash failed: %s", firmware_flash::resultName(res));
  mode_ = Mode::Failed;
  ctx_->ui.invalidate(RefreshHint::Full);
}

void FlasherApp::drawScreen(UiApp::ScreenType& screen, void* self) {
  auto& f = *static_cast<FlasherApp*>(self);
  screen.target().fill(f.ctx_->ui.device().screen(), Paint::solid(Color::White));

  screen.header("Swap firmware", "/firmware on the SD card");

  if (f.mode_ == Mode::Flashing) {
    static char msg[96];
    snprintf(msg, sizeof(msg), "Flashing %s\n%u%%\n\nDo not power off.", f.binNames_[f.chosen_],
             static_cast<unsigned>(f.progressPct_));
    screen.popup(msg);
    return;
  }

  if (f.mode_ == Mode::Confirm && f.chosen_ >= 0) {
    // Dialog only — the list stays un-registered so its rows can't take focus
    // behind the dim, and LEFT/RIGHT can't reach the options (InputDefault).
    static const DialogOption options[2] = {
        {"Cancel", ACT_CANCEL},
        {"Flash + reboot", ACT_GO},
    };
    OptionDialogProps dlg;
    dlg.title = "Flash this firmware?";
    dlg.headline = f.binNames_[f.chosen_];
    dlg.message = "It boots from the spare slot; this firmware stays installed. DOWN selects, CONFIRM activates.";
    dlg.options = options;
    dlg.optionCount = 2;
    dlg.inputMask = InputDefault;
    dlg.dimBackground = true;
    screen.dialog(dlg);
    return;
  }

  if (f.binCount_ == 0) {
    screen.spacer(24);
    screen.popup("No .bin files found.\nCopy a firmware image (e.g.\nCrossPoint's firmware.bin) into\n/firmware on the SD card.");
    return;
  }

  ListItem items[MAX_BINS] = {};
  for (int i = 0; i < f.binCount_; i++) {
    items[i].label = f.binNames_[i];
    items[i].actionValue = i;
  }

  // Selection-driven, same pattern as the Flashcards deck list.
  const uint16_t visible = listVisibleRows(screen.body(), screen.theme().rowHeight, 0);
  f.binTop_ = listTopIndexFor(f.binSel_, f.binTop_, visible, static_cast<uint16_t>(f.binCount_));

  ListProps lp;
  lp.items = items;
  lp.count = static_cast<uint16_t>(f.binCount_);
  lp.selectedIndex = f.binSel_;
  lp.topIndex = f.binTop_;
  lp.action = NO_ACTION;
  lp.rowHeight = 0;  // inherit theme metric
  screen.list(lp);

  if (f.mode_ == Mode::Failed) {
    screen.popup(f.failMsg_);
  }
}
