#pragma once

#include <BleKeyboardHost.h>

#include "../AppContext.h"

// Distraction-free word processor. BLE keyboard in, plain .txt out.
//
// Refresh contract (docs/INVARIANTS.md): plain keystrokes never touch the
// panel; the screen updates only on Enter / Tab / '.' (fast), Esc (forced
// fast), Ctrl+L (full, ghost clear), the typed-char budget, menu transitions —
// and one idle catch-up ~2 s after the last key, which also carries the
// autosave so SD I/O rides a pause, never a keystroke. Every Nth fast
// refresh is promoted to a full refresh to keep the panel DC-balanced.
//
// Shortcuts: Ctrl+S save · Ctrl+N new file · Ctrl+L full refresh ·
// Ctrl+D dark mode · Esc redraw. BACK button opens the menu.
//
// Documents live in /docs; the menu's "Save folder" row moves the current one
// to /decks (and back), where the Flashcards app reads it — decks are the same
// plain text, one "question|answer" per line. "Open document" lists both
// folders. The menu and the picker are selection-driven lists (see drawMenu):
// focus can't scroll, and both can outgrow one landscape screenful.
//
// The canvas is the SDK's textArea component: it word-wraps the whole buffer
// with no line cap, draws the caret natively, and scrolls by visual line
// (textAreaMeasure + textAreaTopLineFor). The buffer is kept NUL-terminated
// (len_ < CAP always) because textArea renders it in place — no render copy.
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
  enum class Mode : uint8_t { Editing, Menu, Pairing, FilePicker, Forget };

  static constexpr size_t CAP = 32 * 1024;  // per-document limit (~5k words)
  static constexpr int MAX_FILES = 32;      // picker cap across /docs + /decks
  static constexpr uint32_t IDLE_CATCHUP_MS = 2000;  // quiet time before catch-up refresh + autosave

  // --- editing ---------------------------------------------------------------
  void handleKey(const freeink::KeyEvent& ev, bool& fast, bool& full);
  void handleMenuKey(const freeink::KeyEvent& ev);  // arrows/Enter/Esc in non-editing modes
  bool insertChar(char c);
  void backspace();
  void deleteForward();
  void triggerRefresh(bool& fast, bool& full);
  // Arm the idle catch-up: called on every buffer or caret change the strict
  // refresh contract leaves off-screen.
  void noteEdit() {
    lastEditMs_ = millis();
    screenStale_ = true;
  }

  // --- files -----------------------------------------------------------------
  bool save();
  void newDocument();
  bool loadDocument(const char* path);
  void loadLastDocument();
  bool allocDocPath();
  bool moveToFolder(const char* dir);
  void scanFiles();
  void scanFolder(const char* dir, bool deck);
  void openPicked();

  // --- menu / drawing ----------------------------------------------------------
  void menuActivate(int16_t row);
  void tryPendingConnect();  // issue a picked pairing once the BLE conn task is free
  void forgetSelected();     // drop the bond under the Forget-screen cursor
  static void drawScreen(UiApp::ScreenType& screen, void* self);
  void drawEditor(UiApp::ScreenType& screen);
  void drawMenu(UiApp::ScreenType& screen);
  void drawPairing(UiApp::ScreenType& screen);
  void drawFilePicker(UiApp::ScreenType& screen);
  void drawForget(UiApp::ScreenType& screen);
  size_t wordCount() const;

  AppContext* ctx_ = nullptr;
  Mode mode_ = Mode::Editing;

  char buf_[CAP];         // document text, always NUL-terminated at buf_[len_]
  size_t len_ = 0;
  size_t cursor_ = 0;
  uint32_t topLine_ = 0;  // first visual line drawn (textArea scroll window)

  char docPath_[64] = {0};
  bool dirty_ = false;
  uint16_t fastRefreshes_ = 0;  // since the last full refresh
  uint16_t charsSinceRefresh_ = 0;  // typed-char refresh budget (Settings.refreshEveryChars)
  uint32_t lastEditMs_ = 0;     // idle catch-up timer; 0 = disarmed (fired or nothing pending)
  bool screenStale_ = false;    // buffer/caret changed since the last panel push
  char toast_[24] = {0};        // one-shot status-bar note ("Saved", ...)

  // menu + file-picker selection (selection-driven lists, moved in tick())
  int16_t menuSel_ = 0;
  uint16_t menuTop_ = 0;
  int16_t fileSel_ = 0;
  uint16_t fileTop_ = 0;
  char fileNames_[MAX_FILES][40];
  bool fileIsDeck_[MAX_FILES];  // /decks entry (shown tagged; opens the same)
  int fileCount_ = 0;

  // pairing UI state
  bool scanKicked_ = false;
  bool lastConnected_ = false;
  uint32_t lastScanDraw_ = 0;
  char pairMsg_[64] = {0};       // passkey prompt / connect-failure note
  char pendingAddr_[18] = {0};   // picked keyboard waiting for the SDK conn task
  bool userConnect_ = false;     // the in-flight attempt is the user's pick (show its failure)
  uint32_t scanDrawSig_ = 0;     // last drawn scan-list content (skip no-change redraws)
  int16_t forgetSel_ = 0;        // Forget-keyboards screen selection

  // status-bar scratch (rebuilt each draw; fixed so drawing never allocates)
  char stLeft_[80];
  char stMid_[48];
  char stRight_[48];
};
