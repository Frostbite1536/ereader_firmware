#include "WriterApp.h"

#include <SDCardManager.h>

#include <cctype>
#include <cstdio>
#include <cstring>

#include "../fonts/WriterFonts.h"

namespace {
using freeink::SpecialKey;
using namespace freeink::ui;

// ActionIds 10-29 (Writer range, CLAUDE.md).
enum : ActionId {
  ACT_MENU_PICK = 10,
  ACT_PAIR_PICK = 11,
  ACT_PAIR_RESCAN = 12,
  ACT_PAIR_BACK = 13,
};

enum MenuRow : int16_t {
  ROW_RESUME = 0,
  ROW_SAVE,
  ROW_NEW,
  ROW_PAIR,
  ROW_FONT,
  ROW_DARK,
  ROW_AUTOSAVE,
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
  ctx.ui.on(ACT_MENU_PICK, [](const ActionEvent& ev, void* self) {
    auto& w = *static_cast<WriterApp*>(self);
    auto& ui = w.ctx_->ui;
    switch (ev.value) {
      case ROW_RESUME:
        w.mode_ = Mode::Editing;
        ui.invalidate(RefreshHint::Fast);
        break;
      case ROW_SAVE:
        strlcpy(w.toast_, w.save() ? "Saved" : "Save FAILED", sizeof(w.toast_));
        w.mode_ = Mode::Editing;
        ui.invalidate(RefreshHint::Fast);
        break;
      case ROW_NEW:
        w.newDocument();
        w.mode_ = Mode::Editing;
        ui.invalidate(RefreshHint::Full);
        break;
      case ROW_PAIR:
        w.mode_ = Mode::Pairing;
        w.scanKicked_ = false;
        w.pairMsg_[0] = 0;
        ui.invalidate(RefreshHint::Fast);
        break;
      case ROW_FONT:
        SETTINGS.fontSize = (SETTINGS.fontSize + 1) % 4;
        SETTINGS.save();
        ui.invalidate(RefreshHint::Fast);
        break;
      case ROW_DARK:
        SETTINGS.darkMode = !SETTINGS.darkMode;
        w.ctx_->target.setEnabled(SETTINGS.darkMode);
        SETTINGS.save();
        ui.invalidate(RefreshHint::Full);
        break;
      case ROW_AUTOSAVE:
        SETTINGS.autosaveOnRefresh = !SETTINGS.autosaveOnRefresh;
        SETTINGS.save();
        ui.invalidate(RefreshHint::Fast);
        break;
      case ROW_EXIT:
        w.ctx_->switchTo(APP_LAUNCHER);
        break;
    }
  }, this);

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
  if (docPath_[0] == 0) loadLastDocument();
  // NimBLE bring-up requires full CPU frequency (INVARIANTS.md #12); we never
  // downclock, so this holds by construction.
  BleHid.begin("InkPad");
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

  bool fast = false;
  bool full = false;
  freeink::KeyEvent ev;
  while (BleHid.popKey(ev)) {
    ctx_->noteActivity();
    if (mode_ == Mode::Editing) handleKey(ev, fast, full);
  }

  switch (mode_) {
    case Mode::Editing:
      if (in.wasPressed(InputManager::BTN_BACK)) {
        mode_ = Mode::Menu;
        ui.invalidate(RefreshHint::Fast);
      } else if (in.wasPressed(InputManager::BTN_CONFIRM)) {
        fast = true;  // no-keyboard way to force a redraw
      }
      break;
    case Mode::Menu:
      if (in.wasPressed(InputManager::BTN_BACK)) {
        mode_ = Mode::Editing;
        ui.invalidate(RefreshHint::Fast);
      }
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
    if (insertChar(ev.ch) && ev.ch == '.') triggerRefresh(fast, full);
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

// --- files ---------------------------------------------------------------------

void WriterApp::allocDocPath() {
  SdMan.ensureDirectoryExists("/docs");
  for (int i = 1; i <= 999; i++) {
    snprintf(docPath_, sizeof(docPath_), "/docs/draft-%03d.txt", i);
    if (!SdMan.exists(docPath_)) break;
  }
  strlcpy(SETTINGS.lastDoc, docPath_, sizeof(SETTINGS.lastDoc));
  SETTINGS.save();
}

bool WriterApp::save() {
  if (docPath_[0] == 0) allocDocPath();
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
  saveIfDirty();
  len_ = cursor_ = 0;
  topLine_ = 0;
  buf_[0] = 0;
  fastRefreshes_ = 0;
  docPath_[0] = 0;
  allocDocPath();
  save();  // reserve the name on card immediately
  strlcpy(toast_, "New file", sizeof(toast_));
}

void WriterApp::loadLastDocument() {
  if (SETTINGS.lastDoc[0] && SdMan.exists(SETTINGS.lastDoc)) {
    strlcpy(docPath_, SETTINGS.lastDoc, sizeof(docPath_));
    // readFileToBuffer reads at most CAP-1 bytes and NUL-terminates.
    len_ = SdMan.readFileToBuffer(docPath_, buf_, CAP);
    cursor_ = len_;
    topLine_ = 0;  // first draw scrolls the window to the caret
    dirty_ = false;
  } else {
    newDocument();
  }
}

// --- drawing ---------------------------------------------------------------------

void WriterApp::drawScreen(UiApp::ScreenType& screen, void* self) {
  auto& w = *static_cast<WriterApp*>(self);
  screen.target().fill(w.ctx_->ui.device().screen(), Paint::solid(Color::White));
  switch (w.mode_) {
    case Mode::Editing: w.drawEditor(screen); break;
    case Mode::Menu: w.drawMenu(screen); break;
    case Mode::Pairing: w.drawPairing(screen); break;
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

  static char pairSub[48];
  if (BleHid.isConnected()) snprintf(pairSub, sizeof(pairSub), "connected: %s", BleHid.connectedName());
  else strlcpy(pairSub, "not connected", sizeof(pairSub));

  ListItem items[ROW_COUNT] = {};
  items[ROW_RESUME].label = "Resume writing";
  items[ROW_SAVE].label = "Save now";
  items[ROW_NEW].label = "New document";
  items[ROW_PAIR].label = "Keyboard pairing";
  items[ROW_PAIR].subtitle = pairSub;
  items[ROW_FONT].label = "Font size";
  items[ROW_FONT].value = fonts::sizeName(SETTINGS.fontSize);
  items[ROW_DARK].label = "Dark mode";
  items[ROW_DARK].value = SETTINGS.darkMode ? "On" : "Off";
  items[ROW_AUTOSAVE].label = "Autosave on refresh";
  items[ROW_AUTOSAVE].value = SETTINGS.autosaveOnRefresh ? "On" : "Off";
  items[ROW_EXIT].label = "Exit to launcher";
  for (int i = 0; i < ROW_COUNT; i++) items[i].actionValue = i;

  ListProps lp;
  lp.items = items;
  lp.count = ROW_COUNT;
  lp.selectedIndex = -1;
  lp.action = ACT_MENU_PICK;
  // No InputPrev/InputNext: route() would fire the FIRST such row on a stray
  // LEFT/RIGHT press (the default list mask accepts them).
  lp.inputMask = InputDefault;
  lp.rowHeight = 0;  // 0 = inherit the theme metric (the default 36 clips subtitles)
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
    lp.inputMask = InputDefault;  // see drawMenu — LEFT/RIGHT must stay inert
    lp.rowHeight = 0;
    screen.list(lp);
  } else if (!pairMsg_[0]) {
    screen.spacer(24);
    screen.popup(BleHid.isScanning() ? "Scanning for keyboards..." : "No devices found. Rescan?");
  }
  if (pairMsg_[0]) screen.popup(pairMsg_);
}
