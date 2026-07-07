#pragma once

#include "../AppContext.h"

// Home menu: pick an app with UP/DOWN + CONFIRM. Battery and device model in
// the header. "Sleep" is also here so the device can be put down deliberately.
class LauncherApp : public App {
 public:
  void begin(AppContext& ctx) override;
  void onEnter() override;
  void tick() override;
  const char* name() const override { return "Launcher"; }

  // Set by main() after device detection, shown in the header.
  void setModelName(const char* model) { model_ = model; }
  bool sleepRequested() const { return sleepRequested_; }
  void clearSleepRequest() { sleepRequested_ = false; }

 private:
  static void drawScreen(UiApp::ScreenType& screen, void* self);

  AppContext* ctx_ = nullptr;
  const char* model_ = "Xteink";
  bool sleepRequested_ = false;
  char battery_[16];
};
