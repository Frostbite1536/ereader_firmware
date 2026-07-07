#include "LauncherApp.h"

#include <cstdio>

namespace {
using namespace freeink::ui;

// ActionIds 1-9 (Launcher range, CLAUDE.md).
enum : ActionId {
  ACT_LAUNCH = 1,
};

enum Row : int16_t {
  ROW_WRITER = 0,
  ROW_CARDS,
  ROW_FLASHER,
  ROW_SLEEP,
  ROW_COUNT,
};
}  // namespace

void LauncherApp::begin(AppContext& ctx) {
  ctx_ = &ctx;
  ctx.ui.on(ACT_LAUNCH, [](const ActionEvent& ev, void* self) {
    auto& l = *static_cast<LauncherApp*>(self);
    switch (ev.value) {
      case ROW_WRITER: l.ctx_->switchTo(APP_WRITER); break;
      case ROW_CARDS: l.ctx_->switchTo(APP_FLASHCARDS); break;
      case ROW_FLASHER: l.ctx_->switchTo(APP_FLASHER); break;
      case ROW_SLEEP: l.sleepRequested_ = true; break;
    }
  }, this);
}

void LauncherApp::onEnter() {
  ctx_->ui.setScreen(&LauncherApp::drawScreen, this, RefreshHint::Full);
}

void LauncherApp::tick() {
  // Nothing continuous; everything routes through the action handler.
}

void LauncherApp::drawScreen(UiApp::ScreenType& screen, void* self) {
  auto& l = *static_cast<LauncherApp*>(self);
  screen.target().fill(l.ctx_->ui.device().screen(), Paint::solid(Color::White));

  snprintf(l.battery_, sizeof(l.battery_), "bat %u%%", l.ctx_->battery.readPercentage());
  screen.header("InkPad", l.model_, l.battery_);

  ListItem items[ROW_COUNT] = {};
  items[ROW_WRITER].label = "Writer";
  items[ROW_WRITER].subtitle = "distraction-free word processor (BLE keyboard)";
  items[ROW_CARDS].label = "Flashcards";
  items[ROW_CARDS].subtitle = "study decks from /decks";
  items[ROW_FLASHER].label = "Swap firmware";
  items[ROW_FLASHER].subtitle = "flash CrossPoint or another .bin from /firmware";
  items[ROW_SLEEP].label = "Sleep";
  items[ROW_SLEEP].subtitle = "deep sleep now (power button wakes)";
  for (int i = 0; i < ROW_COUNT; i++) items[i].actionValue = i;

  ListProps lp;
  lp.items = items;
  lp.count = ROW_COUNT;
  lp.selectedIndex = -1;
  lp.action = ACT_LAUNCH;
  // No InputPrev/InputNext: with the default mask a stray LEFT/RIGHT press
  // dispatches the FIRST row (launching the Writer unasked).
  lp.inputMask = InputDefault;
  lp.rowHeight = 0;  // inherit theme metric
  screen.list(lp);
}
