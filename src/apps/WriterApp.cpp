#include "WriterApp.h"

#include <SDCardManager.h>

#include <cctype>
#include <cstdio>
#include <cstring>

#include "../BleCompat.h"
#include "../BleKeyFilter.h"
#include "../ButtonHints.h"
#include "../SdMount.h"
#include "../fonts/WriterFonts.h"

namespace {
using freeink::SpecialKey;
using namespace freeink::ui;

// ActionIds 10-29 (Writer range, CLAUDE.md). The menu and file picker are
// selection-driven (tick() owns the buttons), so only pairing dispatches.
enum : ActionId {
  ACT_PAIR_PICK = 11,
  ACT_PAIR_RESCAN = 12,
  ACT_PAIR_BACK = 13,
};

enum MenuRow : int16_t {
  ROW_RESUME = 0,
  ROW_SAVE,
  ROW_OPEN,
  ROW_NEW,
  ROW_FOLDER,
  ROW_PAIR,
  ROW_FONT,
  ROW_DARK,
  ROW_ROTATE,
  ROW_AUTOSAVE,
  ROW_REFRESH,
  ROW_EXIT,
  ROW_COUNT,
};

constexpr uint8_t HID_MOD_CTRL = 0x11;  // left | right ctrl (HID modifier byte)
// Focus-driven lists can only reach rows that fit on screen (the SDK list
// virtualizes but focus does not scroll it), so cap the pairing list at what
// fits above the footer on both panels.
constexpr size_t MAX_PAIR_ROWS = 8;
}  // namespace

void WriterApp::begin(AppContext& ctx) {
  ctx_ = &ctx;
  ctx.ui.on(ACT_PAIR_PICK, [](const ActionEvent& ev, void* self) {
    auto& w = *static_cast<WriterApp*>(self);
    if (ev.value >= 0 && ev.value < BleHid.deviceCount()) {
      BleHid.stopScan();
      w.pairMsg_[0] = 0;
      BleHid.connect(BleHid.device(ev.value).addr);
    }
    w.ctx_->ui.invalidate(RefreshHint::Fast);
  }, this);

  ctx.ui.on(ACT_PAIR_RESCAN, [](const ActionEvent&, void* self) {
    auto& w = *static_cast<WriterApp*>(self);
    w.pairMsg_[0] = 0;
    BleHid.startScan(5000);
    w.ctx_->ui.invalidate(RefreshHint::Fast);
  }, this);

  ctx.ui.on(ACT_PAIR_BACK, [](const ActionEvent&, void* self) {
    auto& w = *static_cast<WriterApp*>(self);
    w.mode_ = Mode::Menu;
    w.ctx_->ui.invalidate(RefreshHint::Fast);
  }, this);
}

void WriterApp::onEnter() {
  mode_ = Mode::Editing;
  ensureSdMounted();  // card may have been inserted after boot
  if (docPath_[0] == 0) loadLastDocument();
  // NimBLE bring-up requires full CPU frequency (INVARIANTS.md #12); we never
  // downclock, so this holds by construction.
  BleHid.begin("Cherith's InkPad");
  applyBleKeyboardCompat();  // after begin(): begin() resets the security globals
  lastConnected_ = BleHid.isConnected();
  ctx_->ui.setScreen(&WriterApp::drawScreen, this, RefreshHint::Full);
}

void WriterApp::onExit() {
  saveIfDirty();
  BleHid.end();  // return NimBLE's RAM to the heap for other apps
  SETTINGS.save();
}

void WriterApp::saveIfDirty() {
  if (dirty_) save();
}

// --- input -------------------------------------------------------------------

void WriterApp::tick() {
  auto& in = ctx_->input;
  auto& ui = ctx_->ui;

  BleHid.poll();
  bleKeyFilterPoll();  // swap in the rollover de-doubling filter on each link-up

  bool fast = false;
  bool full = false;
  freeink::KeyEvent ev;
  while (BleHid.popKey(ev)) {
    ctx_->noteActivity();
    if (mode_ == Mode::Editing) handleKey(ev, fast, full);
    else handleMenuKey(ev);
  }

  switch (mode_) {
    case Mode::Editing:
      if (in.wasPressed(InputManager::BTN_BACK)) {
        mode_ = Mode::Menu;
        menuSel_ = 0;
        menuTop_ = 0;
        ui.invalidate(RefreshHint::Fast);
      } else if (in.wasPressed(InputManager::BTN_CONFIRM)) {
        fast = true;  // no-keyboard way to force a redraw
      }
      break;
    case Mode::Menu:
      // Selection-driven like the deck list (focus can't scroll, and 11 rows
      // overflow a landscape page). LEFT/RIGHT mirror the side buttons.
      if (in.wasPressed(InputManager::BTN_BACK)) {
        mode_ = Mode::Editing;
        ui.invalidate(RefreshHint::Fast);
        break;
      }
      if ((in.wasPressed(InputManager::BTN_DOWN) || in.wasPressed(InputManager::BTN_RIGHT)) &&
          menuSel_ + 1 < ROW_COUNT) {
        menuSel_++;
        ui.invalidate(RefreshHint::Fast);
      }
      if ((in.wasPressed(InputManager::BTN_UP) || in.wasPressed(InputManager::BTN_LEFT)) && menuSel_ > 0) {
        menuSel_--;
        ui.invalidate(RefreshHint::Fast);
      }
      if (in.wasPressed(InputManager::BTN_CONFIRM)) menuActivate(menuSel_);
      break;
    case Mode::FilePicker:
      if (in.wasPressed(InputManager::BTN_BACK)) {
        mode_ = Mode::Menu;
        ui.invalidate(RefreshHint::Fast);
        break;
      }
      if ((in.wasPressed(InputManager::BTN_DOWN) || in.wasPressed(InputManager::BTN_RIGHT)) &&
          fileSel_ + 1 < fileCount_) {
        fileSel_++;
        ui.invalidate(RefreshHint::Fast);
      }
      if ((in.wasPressed(InputManager::BTN_UP) || in.wasPressed(InputManager::BTN_LEFT)) && fileSel_ > 0) {
        fileSel_--;
        ui.invalidate(RefreshHint::Fast);
      }
      if (in.wasPressed(InputManager::BTN_CONFIRM)) openPicked();
      break;
    case Mode::Pairing: {
      if (in.wasPressed(InputManager::BTN_BACK)) {
        mode_ = Mode::Menu;
        ui.invalidate(RefreshHint::Fast);
      }
      if (!scanKicked_ && !BleHid.isConnected() && !BleHid.isConnecting()) {
        BleHid.startScan(5000);
        scanKicked_ = true;
      }
      // Some keyboards demand a passkey instead of Just Works bonding; without
      // showing it, pairing stalls silently.
      uint32_t passkey = 0;
      if (BleHid.takePairingPasskey(passkey)) {
        snprintf(pairMsg_, sizeof(pairMsg_), "Type %06u on the keyboard,\nthen press Enter.",
                 static_cast<unsigned>(passkey));
        ui.invalidate(RefreshHint::Fast);
      }
      char fail[40];
      if (BleHid.takeConnectFailure(fail, sizeof(fail))) {
        snprintf(pairMsg_, sizeof(pairMsg_), "Connect failed:\n%s", fail);
        ui.invalidate(RefreshHint::Fast);
      }
      // Live-update the list while scanning.
      if (BleHid.isScanning() && millis() - lastScanDraw_ > 1500) {
        lastScanDraw_ = millis();
        ui.invalidate(RefreshHint::Fast);
      }
      if (BleHid.isConnected() != lastConnected_) {
        lastConnected_ = BleHid.isConnected();
        if (lastConnected_) {  // pairing done — drop the user back into the text
          mode_ = Mode::Editing;
          pairMsg_[0] = 0;
          strlcpy(toast_, "Keyboard connected", sizeof(toast_));
        }
        ui.invalidate(RefreshHint::Fast);
      }
      break;
    }
  }

  if (full) ui.invalidate(RefreshHint::Full);
  else if (fast) ui.invalidate(RefreshHint::Fast);
}

void WriterApp::handleKey(const freeink::KeyEvent& ev, bool& fast, bool& full) {
  const bool ctrl = (ev.mods & HID_MOD_CTRL) != 0;

  if (ctrl && ev.ch) {
    // Normalize: with Shift also held the keymap reports 'S', not 's'.
    switch (tolower(static_cast<unsigned char>(ev.ch))) {
      case 's':
        strlcpy(toast_, save() ? "Saved" : "Save FAILED", sizeof(toast_));
        fast = true;
        return;
      case 'n':
        newDocument();
        full = true;
        return;
      case 'l':  // ghost clear
        full = true;
        return;
      case 'd':
        SETTINGS.darkMode = !SETTINGS.darkMode;
        ctx_->target.setEnabled(SETTINGS.darkMode);
        SETTINGS.save();
        full = true;
        return;
      default:
        return;  // unbound ctrl combo: ignore, never insert
    }
  }

  switch (ev.special) {
    case SpecialKey::Enter:
      if (insertChar('\n')) triggerRefresh(fast, full);
      return;
    case SpecialKey::Tab: {
      // Two spaces, not '\t': buffer, saved file, and rendered canvas stay
      // byte-identical (the panel fonts have no tab glyph).
      bool ok = insertChar(' ');
      if (insertChar(' ')) ok = true;
      if (ok) triggerRefresh(fast, full);
      return;
    }
    case SpecialKey::Backspace:
      backspace();
      return;  // no refresh (INVARIANTS.md #3) — Esc redraws on demand
    case SpecialKey::Delete:
      deleteForward();
      return;
    case SpecialKey::Escape:
      fast = true;
      return;
    case SpecialKey::Left:
      if (cursor_ > 0) cursor_--;
      return;
    case SpecialKey::Right:
      if (cursor_ < len_) cursor_++;
      return;
    case SpecialKey::Home:
      while (cursor_ > 0 && buf_[cursor_ - 1] != '\n') cursor_--;
      return;
    case SpecialKey::End:
      while (cursor_ < len_ && buf_[cursor_] != '\n') cursor_++;
      return;
    default:
      break;
  }

  if (ev.ch >= 0x20 && ev.ch < 0x7F) {
    if (insertChar(ev.ch)) {
      // Trigger keys refresh immediately; other characters draw down the
      // typed-char budget ("Refresh every" menu row, 0 = off) so long
      // trigger-less stretches still reach the panel (INVARIANTS.md #1).
      if (ev.ch == '.') {
        triggerRefresh(fast, full);
      } else if (SETTINGS.refreshEveryChars != 0 && ++charsSinceRefresh_ >= SETTINGS.refreshEveryChars) {
        triggerRefresh(fast, full);
      }
    }
  }
}

// Keyboard navigation for the non-editing modes: arrows move the selection or
// focus, Enter activates, Esc/Backspace goes back — mirroring the physical
// buttons, which lose presses during panel refreshes (BLE keys are queued).
void WriterApp::handleMenuKey(const freeink::KeyEvent& ev) {
  using SK = freeink::SpecialKey;
  auto& ui = ctx_->ui;
  const bool down = ev.special == SK::Down || ev.special == SK::Right;
  const bool up = ev.special == SK::Up || ev.special == SK::Left;
  const bool enter = ev.special == SK::Enter;
  const bool back = ev.special == SK::Escape || ev.special == SK::Backspace;
  switch (mode_) {
    case Mode::Menu:
      if (down && menuSel_ + 1 < ROW_COUNT) {
        menuSel_++;
        ui.invalidate(RefreshHint::Fast);
      } else if (up && menuSel_ > 0) {
        menuSel_--;
        ui.invalidate(RefreshHint::Fast);
      } else if (enter) {
        menuActivate(menuSel_);
      } else if (back) {
        mode_ = Mode::Editing;
        ui.invalidate(RefreshHint::Fast);
      }
      break;
    case Mode::FilePicker:
      if (down && fileSel_ + 1 < fileCount_) {
        fileSel_++;
        ui.invalidate(RefreshHint::Fast);
      } else if (up && fileSel_ > 0) {
        fileSel_--;
        ui.invalidate(RefreshHint::Fast);
      } else if (enter) {
        openPicked();
      } else if (back) {
        mode_ = Mode::Menu;
        ui.invalidate(RefreshHint::Fast);
      }
      break;
    case Mode::Pairing:
      // Focus-driven screen: feed the shared snapshot flags so the row and
      // footer actions dispatch exactly like button presses.
      if (down) ctx_->kbFocusNext = true;
      else if (up) ctx_->kbFocusPrev = true;
      else if (enter) ctx_->kbConfirm = true;
      else if (back) ctx_->kbBack = true;
      break;
    default:
      break;
  }
}

bool WriterApp::insertChar(char c) {
  if (len_ + 1 >= CAP) return false;  // explicit FULL state; input rejected
  memmove(buf_ + cursor_ + 1, buf_ + cursor_, len_ - cursor_);
  buf_[cursor_++] = c;
  len_++;
  buf_[len_] = 0;
  dirty_ = true;
  return true;
}

void WriterApp::backspace() {
  if (cursor_ == 0) return;
  memmove(buf_ + cursor_ - 1, buf_ + cursor_, len_ - cursor_);
  cursor_--;
  len_--;
  buf_[len_] = 0;
  dirty_ = true;
}

void WriterApp::deleteForward() {
  if (cursor_ >= len_) return;
  memmove(buf_ + cursor_, buf_ + cursor_ + 1, len_ - cursor_ - 1);
  len_--;
  buf_[len_] = 0;
  dirty_ = true;
}

void WriterApp::triggerRefresh(bool& fast, bool& full) {
  toast_[0] = 0;
  charsSinceRefresh_ = 0;  // any trigger restarts the typed-char budget
  fastRefreshes_++;
  if (fastRefreshes_ >= SETTINGS.fullRefreshEvery) {
    fastRefreshes_ = 0;
    full = true;  // periodic promotion (INVARIANTS.md #2)
  } else {
    fast = true;
  }
  if (SETTINGS.autosaveOnRefresh && dirty_) {
    if (!save()) strlcpy(toast_, "Save FAILED", sizeof(toast_));
  }
}

// --- menu ------------------------------------------------------------------------

void WriterApp::menuActivate(int16_t row) {
  auto& ui = ctx_->ui;
  switch (row) {
    case ROW_RESUME:
      mode_ = Mode::Editing;
      ui.invalidate(RefreshHint::Fast);
      break;
    case ROW_SAVE:
      strlcpy(toast_, save() ? "Saved" : "Save FAILED", sizeof(toast_));
      mode_ = Mode::Editing;
      ui.invalidate(RefreshHint::Fast);
      break;
    case ROW_OPEN:
      scanFiles();
      fileSel_ = 0;
      fileTop_ = 0;
      mode_ = Mode::FilePicker;
      ui.invalidate(RefreshHint::Fast);
      break;
    case ROW_NEW:
      newDocument();
      mode_ = Mode::Editing;
      ui.invalidate(RefreshHint::Full);
      break;
    case ROW_FOLDER: {
      // "Type the cards in the Writer, then choose the folder": moving to
      // /decks is what publishes a document to the Flashcards app.
      const bool inDecks = strncmp(docPath_, "/decks/", 7) == 0;
      const char* dest = inDecks ? "/docs" : "/decks";
      if (moveToFolder(dest)) snprintf(toast_, sizeof(toast_), "Moved to %s", dest);
      else strlcpy(toast_, "Move FAILED", sizeof(toast_));
      // Stay in the menu: the row's value and the header path show the result.
      ui.invalidate(RefreshHint::Fast);
      break;
    }
    case ROW_PAIR:
      mode_ = Mode::Pairing;
      scanKicked_ = false;
      pairMsg_[0] = 0;
      ui.invalidate(RefreshHint::Fast);
      break;
    case ROW_FONT:
      SETTINGS.fontSize = (SETTINGS.fontSize + 1) % 4;
      SETTINGS.save();
      ui.invalidate(RefreshHint::Fast);
      break;
    case ROW_DARK:
      SETTINGS.darkMode = !SETTINGS.darkMode;
      ctx_->target.setEnabled(SETTINGS.darkMode);
      SETTINGS.save();
      ui.invalidate(RefreshHint::Full);
      break;
    case ROW_ROTATE:
      // Orientation is baked into the draw target at construction, so this
      // reboots — which wipes the RAM buffer. Refuse until the text is safely
      // on card (INVARIANTS.md #4): onExit()'s save-if-dirty runs before the
      // restart, but a failure there could not stop the reboot.
      if (dirty_ && !save()) {
        strlcpy(toast_, "Save FAILED - kept text", sizeof(toast_));
        mode_ = Mode::Editing;
        ui.invalidate(RefreshHint::Full);
        break;
      }
      SETTINGS.landscape = !SETTINGS.landscape;
      SETTINGS.save();
      ctx_->requestRestart();
      break;
    case ROW_AUTOSAVE:
      SETTINGS.autosaveOnRefresh = !SETTINGS.autosaveOnRefresh;
      SETTINGS.save();
      ui.invalidate(RefreshHint::Fast);
      break;
    case ROW_REFRESH:
      // Typed-char refresh budget: Off -> 25 -> 50 -> 100 -> Off. Budget
      // refreshes go through triggerRefresh(), so they count toward the same
      // every-Nth FULL promotion as the trigger keys (INVARIANTS.md #2).
      SETTINGS.refreshEveryChars = SETTINGS.refreshEveryChars == 0    ? 25
                                   : SETTINGS.refreshEveryChars == 25 ? 50
                                   : SETTINGS.refreshEveryChars == 50 ? 100
                                                                      : 0;
      SETTINGS.save();
      ui.invalidate(RefreshHint::Fast);
      break;
    case ROW_EXIT:
      ctx_->switchTo(APP_LAUNCHER);
      break;
  }
}

// --- files ---------------------------------------------------------------------

bool WriterApp::allocDocPath() {
  SdMan.ensureDirectoryExists("/docs");
  for (int i = 1; i <= 999; i++) {
    snprintf(docPath_, sizeof(docPath_), "/docs/draft-%03d.txt", i);
    if (!SdMan.exists(docPath_)) {
      strlcpy(SETTINGS.lastDoc, docPath_, sizeof(SETTINGS.lastDoc));
      SETTINGS.save();
      return true;
    }
  }
  // All 999 slots taken: refuse rather than silently overwrite draft-999
  // (INVARIANTS.md #4).
  docPath_[0] = 0;
  return false;
}

bool WriterApp::save() {
  // Late-inserted card: try to mount before declaring failure. Throttled
  // inside ensureSdMounted(), so a missing card can't stall the typing path.
  if (!ensureSdMounted()) return false;
  if (docPath_[0] == 0 && !allocDocPath()) return false;
  // Atomic-enough save (INVARIANTS.md #5): full write to .tmp, then swap.
  char tmp[72];
  snprintf(tmp, sizeof(tmp), "%s.tmp", docPath_);
  FsFile f = SdMan.open(tmp, O_WRONLY | O_CREAT | O_TRUNC);
  if (!f) return false;
  const size_t written = f.write(buf_, len_);
  f.close();
  if (written != len_) {
    SdMan.remove(tmp);
    return false;
  }
  if (SdMan.exists(docPath_)) SdMan.remove(docPath_);
  if (!SdMan.rename(tmp, docPath_)) return false;
  dirty_ = false;
  return true;
}

void WriterApp::newDocument() {
  // Never wipe unsaved text (INVARIANTS.md #4): if the flush fails (no card),
  // keep the buffer and the old document exactly as they were.
  if (dirty_ && !save()) {
    strlcpy(toast_, "Save FAILED - kept text", sizeof(toast_));
    return;
  }
  // Allocate the new name BEFORE wiping, so a full /docs leaves the current
  // document untouched.
  char oldPath[sizeof(docPath_)];
  strlcpy(oldPath, docPath_, sizeof(oldPath));
  docPath_[0] = 0;
  if (!allocDocPath()) {
    strlcpy(docPath_, oldPath, sizeof(docPath_));
    strlcpy(toast_, "No free draft name", sizeof(toast_));
    return;
  }
  len_ = cursor_ = 0;
  topLine_ = 0;
  buf_[0] = 0;
  fastRefreshes_ = 0;
  save();  // reserve the name on card immediately
  strlcpy(toast_, "New file", sizeof(toast_));
}

bool WriterApp::loadDocument(const char* path) {
  if (!ensureSdMounted()) return false;
  FsFile f = SdMan.open(path);
  if (!f) return false;
  const size_t size = static_cast<size_t>(f.fileSize());
  f.close();
  // readFileToBuffer reads at most CAP-1 bytes and NUL-terminates. A short
  // read (SD hiccup, file vanished under us) must not masquerade as a clean
  // document: a later autosave would replace the real file with the stub. So
  // only adopt the path once the buffer holds everything we expected.
  const size_t want = size < CAP - 1 ? size : CAP - 1;
  len_ = SdMan.readFileToBuffer(path, buf_, CAP);
  if (len_ != want) {
    len_ = cursor_ = 0;  // buffer is clobbered either way; leave it empty+clean
    buf_[0] = 0;
    dirty_ = false;
    return false;
  }
  strlcpy(docPath_, path, sizeof(docPath_));
  cursor_ = len_;
  topLine_ = 0;  // first draw scrolls the window to the caret
  dirty_ = false;
  fastRefreshes_ = 0;
  strlcpy(SETTINGS.lastDoc, docPath_, sizeof(SETTINGS.lastDoc));
  SETTINGS.save();
  return true;
}

void WriterApp::loadLastDocument() {
  if (!(SETTINGS.lastDoc[0] && loadDocument(SETTINGS.lastDoc))) newDocument();
}

// Move the current document between /docs and /decks — the same plain text
// either way; /decks is simply where the Flashcards app looks. Write to the
// new home first, delete the old copy only after that succeeded, so a failed
// move can never lose the only copy (INVARIANTS.md #4/#5).
bool WriterApp::moveToFolder(const char* dir) {
  if (!ensureSdMounted()) return false;
  if (docPath_[0] == 0) allocDocPath();
  const char* base = strrchr(docPath_, '/');
  base = base ? base + 1 : docPath_;
  char newPath[sizeof(docPath_)];
  snprintf(newPath, sizeof(newPath), "%s/%s", dir, base);
  if (strcmp(newPath, docPath_) == 0) return true;  // already there
  SdMan.ensureDirectoryExists(dir);
  if (SdMan.exists(newPath)) {
    // Name taken in the destination: fall back to the first free draft slot.
    bool found = false;
    for (int i = 1; i <= 999; i++) {
      snprintf(newPath, sizeof(newPath), "%s/draft-%03d.txt", dir, i);
      if (!SdMan.exists(newPath)) {
        found = true;
        break;
      }
    }
    if (!found) return false;  // destination full — never overwrite (INVARIANTS.md #4)
  }
  char oldPath[sizeof(docPath_)];
  strlcpy(oldPath, docPath_, sizeof(oldPath));
  strlcpy(docPath_, newPath, sizeof(docPath_));
  if (!save()) {  // full atomic write at the new location
    strlcpy(docPath_, oldPath, sizeof(docPath_));
    return false;
  }
  if (SdMan.exists(oldPath)) SdMan.remove(oldPath);
  strlcpy(SETTINGS.lastDoc, docPath_, sizeof(SETTINGS.lastDoc));
  SETTINGS.save();
  return true;
}

void WriterApp::scanFiles() {
  fileCount_ = 0;
  if (!ensureSdMounted()) return;
  scanFolder("/docs", false);
  scanFolder("/decks", true);
}

void WriterApp::scanFolder(const char* dir, bool deck) {
  const int start = fileCount_;
  SdMan.ensureDirectoryExists(dir);
  for (const String& name : SdMan.listFiles(dir, MAX_FILES * 2)) {
    if (fileCount_ >= MAX_FILES) break;
    if (!hasExtension(name, ".txt")) continue;  // also skips leftover .tmp files
    // A name that would truncate can't round-trip through openPicked() — the
    // rebuilt path would point at a different (or no) file. Leave it out.
    if (name.length() >= sizeof(fileNames_[0])) continue;
    strlcpy(fileNames_[fileCount_], name.c_str(), sizeof(fileNames_[0]));
    fileIsDeck_[fileCount_] = deck;
    fileCount_++;
  }
  // Alphabetical within the folder (draft-NNN is zero-padded, so this is also
  // creation order); FAT directory order is whatever the card felt like.
  for (int i = start + 1; i < fileCount_; i++) {
    char key[sizeof(fileNames_[0])];
    strlcpy(key, fileNames_[i], sizeof(key));
    int j = i;
    while (j > start && strcmp(fileNames_[j - 1], key) > 0) {
      memcpy(fileNames_[j], fileNames_[j - 1], sizeof(fileNames_[0]));
      j--;
    }
    strlcpy(fileNames_[j], key, sizeof(fileNames_[0]));
  }
}

void WriterApp::openPicked() {
  if (fileSel_ < 0 || fileSel_ >= fileCount_) return;
  // Flush the current document before its buffer is replaced (INVARIANTS.md
  // #4); on failure stay on the old text rather than dropping it.
  if (dirty_ && !save()) {
    strlcpy(toast_, "Save FAILED - kept text", sizeof(toast_));
    mode_ = Mode::Editing;
    ctx_->ui.invalidate(RefreshHint::Full);
    return;
  }
  char prev[sizeof(docPath_)];
  strlcpy(prev, docPath_, sizeof(prev));
  char path[sizeof(docPath_)];
  snprintf(path, sizeof(path), "%s/%s", fileIsDeck_[fileSel_] ? "/decks" : "/docs", fileNames_[fileSel_]);
  if (loadDocument(path)) {
    strlcpy(toast_, "Opened", sizeof(toast_));
  } else {
    // Target unreadable (and the buffer clobbered by the attempt): fall back
    // to the document we just flushed, or a fresh one if even that is gone.
    if (!loadDocument(prev)) newDocument();
    strlcpy(toast_, "Open FAILED", sizeof(toast_));
  }
  mode_ = Mode::Editing;
  ctx_->ui.invalidate(RefreshHint::Full);
}

// --- drawing ---------------------------------------------------------------------

void WriterApp::drawScreen(UiApp::ScreenType& screen, void* self) {
  auto& w = *static_cast<WriterApp*>(self);
  screen.target().fill(w.ctx_->ui.device().screen(), Paint::solid(Color::White));
  switch (w.mode_) {
    case Mode::Editing: w.drawEditor(screen); break;
    case Mode::Menu: w.drawMenu(screen); break;
    case Mode::Pairing: w.drawPairing(screen); break;
    case Mode::FilePicker: w.drawFilePicker(screen); break;
  }
}

size_t WriterApp::wordCount() const {
  size_t words = 0;
  bool inWord = false;
  for (size_t i = 0; i < len_; i++) {
    const bool ws = buf_[i] == ' ' || buf_[i] == '\n' || buf_[i] == '\t';
    if (!ws && !inWord) words++;
    inWord = !ws;
  }
  return words;
}

void WriterApp::drawEditor(UiApp::ScreenType& screen) {
  const char* base = strrchr(docPath_, '/');
  base = base ? base + 1 : docPath_;
  snprintf(stLeft_, sizeof(stLeft_), "%s%s", base, dirty_ ? "*" : "");

  if (toast_[0]) {
    strlcpy(stMid_, toast_, sizeof(stMid_));
  } else if (BleHid.isConnected()) {
    snprintf(stMid_, sizeof(stMid_), "BT %s", BleHid.connectedName());
  } else {
    strlcpy(stMid_, BleHid.isConnecting() ? "BT connecting" : "no keyboard", sizeof(stMid_));
  }

  const unsigned pct = static_cast<unsigned>((len_ * 100) / CAP);
  if (len_ + 1 >= CAP) {
    snprintf(stRight_, sizeof(stRight_), "FULL  bat %u%%", ctx_->battery.readPercentage());
  } else if (pct >= 90) {
    snprintf(stRight_, sizeof(stRight_), "%uw %u%%full bat %u%%", static_cast<unsigned>(wordCount()), pct,
             ctx_->battery.readPercentage());
  } else {
    snprintf(stRight_, sizeof(stRight_), "%uw  bat %u%%", static_cast<unsigned>(wordCount()),
             ctx_->battery.readPercentage());
  }

  StatusBarProps bar;
  bar.leading = stLeft_;
  bar.title = stMid_;
  bar.trailing = stRight_;
  bar.text.font = fonts::UI_SMALL;
  bar.fillBackground = true;
  screen.status(bar, LayoutAnchor::Bottom);

  screen.insetContent(Insets{8, 10, 4, 10});  // writing margins
  const Rect canvas = screen.body();

  TextStyle st;
  st.font = fonts::WRITER_BASE + SETTINGS.fontSize;

  // Scroll by visual line so the caret stays on screen. textAreaMeasure walks
  // the same wrap the textArea draws with — no iterative re-measuring, and no
  // layoutText 16-line cap (the reason a plain text() cannot render the doc).
  auto& target = screen.target();
  const TextAreaMetrics m = textAreaMeasure(target, canvas.width, buf_, st, static_cast<uint32_t>(cursor_));
  const uint16_t visible = textAreaVisibleLines(canvas, target.lineHeight(st.font));
  topLine_ = textAreaTopLineFor(m.caretLine, topLine_, visible, m.lineCount);

  TextAreaProps props;
  props.text = buf_;
  props.cursor = static_cast<uint32_t>(cursor_);
  props.topLine = topLine_;
  props.style = st;
  props.showCaret = true;
  screen.textArea(props);
}

void WriterApp::drawMenu(UiApp::ScreenType& screen) {
  screen.header("Writer", docPath_);
  drawButtonHints(screen, "Close", "Select", "Up", "Down");

  static char pairSub[48];
  if (BleHid.isConnected()) snprintf(pairSub, sizeof(pairSub), "connected: %s", BleHid.connectedName());
  else strlcpy(pairSub, "not connected", sizeof(pairSub));

  const bool inDecks = strncmp(docPath_, "/decks/", 7) == 0;

  ListItem items[ROW_COUNT] = {};
  items[ROW_RESUME].label = "Resume writing";
  items[ROW_SAVE].label = "Save now";
  items[ROW_OPEN].label = "Open document";
  items[ROW_NEW].label = "New document";
  items[ROW_FOLDER].label = "Save folder";
  items[ROW_FOLDER].value = inDecks ? "/decks" : "/docs";
  items[ROW_PAIR].label = "Keyboard pairing";
  items[ROW_FONT].label = "Font size";
  items[ROW_FONT].value = fonts::sizeName(SETTINGS.fontSize);
  items[ROW_DARK].label = "Dark mode";
  items[ROW_DARK].value = SETTINGS.darkMode ? "On" : "Off";
  items[ROW_ROTATE].label = "Screen rotation";
  items[ROW_ROTATE].value = SETTINGS.landscape ? "Landscape" : "Portrait";
  items[ROW_AUTOSAVE].label = "Autosave on refresh";
  items[ROW_AUTOSAVE].value = SETTINGS.autosaveOnRefresh ? "On" : "Off";
  static char refreshVal[16];
  if (SETTINGS.refreshEveryChars == 0) strlcpy(refreshVal, "Off", sizeof(refreshVal));
  else snprintf(refreshVal, sizeof(refreshVal), "%u chars", SETTINGS.refreshEveryChars);
  items[ROW_REFRESH].label = "Refresh every";
  items[ROW_REFRESH].value = refreshVal;
  items[ROW_EXIT].label = "Exit to launcher";
  for (int i = 0; i < ROW_COUNT; i++) items[i].actionValue = i;

  // Selection-driven (tick() owns the buttons, NO_ACTION registers nothing):
  // unlike the focus system this scrolls, so the menu can hold 11 rows even on
  // a landscape screen that shows ~9.
  int16_t rowH;
  if (SETTINGS.landscape) {
    // Compact single-line rows, no subtitles: more of the menu per screenful.
    rowH = static_cast<int16_t>(screen.target().lineHeight(fonts::UI_BODY) + 12);
  } else {
    items[ROW_FOLDER].subtitle = "/decks files show up in Flashcards";
    items[ROW_PAIR].subtitle = pairSub;
    items[ROW_ROTATE].subtitle = "applies after a quick restart";
    rowH = screen.theme().rowHeight;  // the subtitle-friendly theme metric
  }
  const uint16_t visible = listVisibleRows(screen.body(), rowH, 0);
  menuTop_ = listTopIndexFor(menuSel_, menuTop_, visible, ROW_COUNT);

  ListProps lp;
  lp.items = items;
  lp.count = ROW_COUNT;
  lp.selectedIndex = menuSel_;
  lp.topIndex = menuTop_;
  lp.action = NO_ACTION;
  lp.rowHeight = SETTINGS.landscape ? rowH : 0;  // 0 = inherit the theme metric
  screen.list(lp);
}

void WriterApp::drawFilePicker(UiApp::ScreenType& screen) {
  screen.header("Open document", "/docs and /decks on the SD card");
  drawButtonHints(screen, "Back", "Open", "Up", "Down");

  if (fileCount_ == 0) {
    screen.spacer(24);
    // Same honesty rule as the deck list: say WHY the list is empty.
    if (!SdMan.ready()) {
      screen.popup("No SD card detected.\nInsert a card, then re-open\nthis menu.");
    } else {
      screen.popup("No documents yet.\n\"New document\" starts one.");
    }
    return;
  }

  ListItem items[MAX_FILES] = {};
  for (int i = 0; i < fileCount_; i++) {
    items[i].label = fileNames_[i];
    items[i].value = fileIsDeck_[i] ? "deck" : "";
    items[i].actionValue = i;
  }

  // Selection-driven, same as the menu and the Flashcards deck list.
  const uint16_t visible = listVisibleRows(screen.body(), screen.theme().rowHeight, 0);
  fileTop_ = listTopIndexFor(fileSel_, fileTop_, visible, static_cast<uint16_t>(fileCount_));

  ListProps lp;
  lp.items = items;
  lp.count = static_cast<uint16_t>(fileCount_);
  lp.selectedIndex = fileSel_;
  lp.topIndex = fileTop_;
  lp.action = NO_ACTION;
  lp.rowHeight = 0;  // inherit theme metric
  screen.list(lp);
}

void WriterApp::drawPairing(UiApp::ScreenType& screen) {
  static char sub[48];
  if (BleHid.isConnected()) snprintf(sub, sizeof(sub), "connected: %s", BleHid.connectedName());
  else if (BleHid.isConnecting()) strlcpy(sub, "connecting...", sizeof(sub));
  else if (BleHid.isScanning()) strlcpy(sub, "scanning...", sizeof(sub));
  else strlcpy(sub, "select your keyboard", sizeof(sub));
  screen.header("Keyboard", sub);

  const FooterAction footer[] = {
      {.label = "Rescan", .action = ACT_PAIR_RESCAN},
      {.label = "Back", .action = ACT_PAIR_BACK},
  };
  screen.footer(footer, 2);

  const uint8_t total = BleHid.deviceCount();
  const uint8_t count = total < MAX_PAIR_ROWS ? total : MAX_PAIR_ROWS;
  ListItem items[MAX_PAIR_ROWS] = {};
  for (uint8_t i = 0; i < count; i++) {
    const auto& d = BleHid.device(i);
    items[i].label = d.name;
    items[i].subtitle = d.addr;
    items[i].value = d.hid ? "HID" : "";
    items[i].actionValue = i;
  }
  if (count > 0) {
    ListProps lp;
    lp.items = items;
    lp.count = count;
    lp.selectedIndex = -1;
    lp.action = ACT_PAIR_PICK;
    // No InputPrev/InputNext: route() would fire the FIRST such row on a
    // stray LEFT/RIGHT press (the default list mask accepts them).
    lp.inputMask = InputDefault;
    lp.rowHeight = 0;
    screen.list(lp);
  } else if (!pairMsg_[0]) {
    screen.spacer(24);
    // Bluetooth-Classic-only keyboards never advertise on BLE, so they can't
    // appear here (HARDWARE.md: the radio has no Classic support) — say so.
    screen.popup(BleHid.isScanning() ? "Scanning for keyboards..."
                                     : "No devices found. Rescan?\nBLE keyboards only (no BT Classic).");
  }
  if (pairMsg_[0]) screen.popup(pairMsg_);
}
