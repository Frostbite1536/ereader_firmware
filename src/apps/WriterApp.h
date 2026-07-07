#pragma once

#include <BleKeyboardHost.h>

#include "../AppContext.h"

// Distraction-free word processor. BLE keyboard in, plain .txt out.
//
// Refresh contract (docs/INVARIANTS.md): plain keystrokes never touch the
// panel; the screen updates only on Enter / Tab / '.' (fast), Esc (forced
// fast), Ctrl+L (full, ghost clear), and menu transitions. Every Nth fast
// refresh is promoted to a full refresh to keep the panel DC-balanced.
//
// Shortcuts: Ctrl+S save · Ctrl+N new file · Ctrl+L full refresh ·
// Ctrl+D dark mode · Esc redraw. BACK button opens the menu.
class WriterApp : public App {
 public:
  void begin(AppContext& ctx) override;
  void onEnter() override;
  void tick() override;
  void onExit() override;
  const char* name() const override { return "Writer"; }

  // Called by main on the way into deep sleep (autosave-on-sleep invariant).
  void saveIfDirty();

 private:
  enum class Mode : uint8_t { Editing, Menu, Pairing };

  static constexpr size_t CAP = 32 * 1024;  // per-document limit (~5k words)

  // --- editing ---------------------------------------------------------------
  void handleKey(const freeink::KeyEvent& ev, bool& fast, bool& full);
  bool insertChar(char c);
  void backspace();
  void deleteForward();
  void triggerRefresh(bool& fast, bool& full);

  // --- files -----------------------------------------------------------------
  bool save();
  void newDocument();
  void loadLastDocument();
  void allocDocPath();

  // --- drawing ---------------------------------------------------------------
  static void drawScreen(UiApp::ScreenType& screen, void* self);
  void drawEditor(UiApp::ScreenType& screen);
  void drawMenu(UiApp::ScreenType& screen);
  void drawPairing(UiApp::ScreenType& screen);
  void buildRenderBuffer();
  size_t wordCount() const;

  AppContext* ctx_ = nullptr;
  Mode mode_ = Mode::Editing;

  char buf_[CAP];
  size_t len_ = 0;
  size_t cursor_ = 0;
  char renderBuf_[CAP + 8];
  size_t viewStart_ = 0;  // first buffer offset drawn (tail-follow scrolling)

  char docPath_[64] = {0};
  bool dirty_ = false;
  uint16_t fastRefreshes_ = 0;  // since the last full refresh
  char toast_[24] = {0};        // one-shot status-bar note ("Saved", ...)

  // menu / pairing UI state
  int16_t menuSel_ = 0;
  int16_t pairSel_ = 0;
  bool scanKicked_ = false;

  // status-bar scratch (rebuilt each draw; fixed so drawing never allocates)
  char stLeft_[80];
  char stMid_[48];
  char stRight_[48];
};
