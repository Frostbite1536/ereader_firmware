#pragma once

#include "../AppContext.h"

// SD-card firmware swap: lists /firmware/*.bin, validates the chosen image,
// flashes it into the inactive OTA partition and reboots into it. This is the
// "game console cartridge" flow — drop CrossPoint's firmware.bin (or any
// other Xteink firmware built on this partition table) in /firmware and swap
// back and forth; CrossPoint's own SD-update screen flashes InkPad back.
//
// Interaction model: the bin list is selection-driven (UP/DOWN + CONFIRM in
// tick(), scrolls past a screenful). The confirm step is a real dialog with
// ONLY its two options interactive — Cancel listed first, so the first press
// of DOWN can never land on "Flash + reboot".
class FlasherApp : public App {
 public:
  void begin(AppContext& ctx) override;
  void onEnter() override;
  void tick() override;
  const char* name() const override { return "Flasher"; }

 private:
  enum class Mode : uint8_t { List, Confirm, Flashing, Failed };

  static constexpr int MAX_BINS = 16;

  void scanBins();
  void runFlash();
  static void drawScreen(UiApp::ScreenType& screen, void* self);

  AppContext* ctx_ = nullptr;
  Mode mode_ = Mode::List;
  char binNames_[MAX_BINS][48];
  int binCount_ = 0;
  int16_t binSel_ = 0;   // selection, moved by UP/DOWN in tick()
  uint16_t binTop_ = 0;  // first visible row
  int16_t chosen_ = -1;  // bin confirmed into the dialog
  char failMsg_[64];
  char pathBuf_[64];
  volatile uint8_t progressPct_ = 0;
  uint8_t drawnPct_ = 0;
  bool flashQueued_ = false;
};
