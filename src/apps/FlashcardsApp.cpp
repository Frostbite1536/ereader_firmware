#include "FlashcardsApp.h"

#include <SDCardManager.h>

#include <cstdio>
#include <cstring>

#include "../fonts/WriterFonts.h"

namespace {
using namespace freeink::ui;
// ActionIds 30-39 are reserved for Flashcards (CLAUDE.md); none in use — the
// deck list is selection-driven, not action-dispatched.
}  // namespace

void FlashcardsApp::begin(AppContext& ctx) { ctx_ = &ctx; }

void FlashcardsApp::onEnter() {
  mode_ = Mode::DeckList;
  scanDecks();
  deckSel_ = 0;
  deckTop_ = 0;
  ctx_->ui.setScreen(&FlashcardsApp::drawScreen, this, RefreshHint::Full);
}

void FlashcardsApp::tick() {
  auto& in = ctx_->input;
  auto& ui = ctx_->ui;

  if (mode_ == Mode::DeckList) {
    if (in.wasPressed(InputManager::BTN_BACK)) {
      ctx_->switchTo(APP_LAUNCHER);
      return;
    }
    if (deckCount_ == 0) return;
    if (in.wasPressed(InputManager::BTN_DOWN) && deckSel_ + 1 < deckCount_) {
      deckSel_++;
      ui.invalidate(RefreshHint::Fast);
    }
    if (in.wasPressed(InputManager::BTN_UP) && deckSel_ > 0) {
      deckSel_--;
      ui.invalidate(RefreshHint::Fast);
    }
    if (in.wasPressed(InputManager::BTN_CONFIRM) && loadDeck(deckSel_)) {
      mode_ = Mode::Card;
      ui.invalidate(RefreshHint::Full);
    }
    return;
  }

  // Card mode. DOWN: flip, then next. UP: previous card. BACK: deck list.
  if (in.wasPressed(InputManager::BTN_DOWN) || in.wasPressed(InputManager::BTN_CONFIRM) ||
      in.wasPressed(InputManager::BTN_RIGHT)) {
    if (showingFront_) {
      showingFront_ = false;
    } else {
      cardIndex_ = (cardIndex_ + 1) % cardCount_;
      showingFront_ = true;
    }
    ui.invalidate(RefreshHint::Fast);
  }
  if (in.wasPressed(InputManager::BTN_UP) || in.wasPressed(InputManager::BTN_LEFT)) {
    cardIndex_ = (cardIndex_ + cardCount_ - 1) % cardCount_;
    showingFront_ = true;
    ui.invalidate(RefreshHint::Fast);
  }
  if (in.wasPressed(InputManager::BTN_BACK)) {
    mode_ = Mode::DeckList;
    ui.invalidate(RefreshHint::Full);
  }
}

void FlashcardsApp::scanDecks() {
  deckCount_ = 0;
  SdMan.ensureDirectoryExists("/decks");
  for (const String& name : SdMan.listFiles("/decks", MAX_DECKS * 2)) {
    if (deckCount_ >= MAX_DECKS) break;
    if (!name.endsWith(".txt")) continue;
    strlcpy(deckNames_[deckCount_], name.c_str(), sizeof(deckNames_[0]));
    deckCount_++;
  }
}

bool FlashcardsApp::loadDeck(int index) {
  if (index < 0 || index >= deckCount_) return false;
  char path[64];
  snprintf(path, sizeof(path), "/decks/%s", deckNames_[index]);
  const size_t n = SdMan.readFileToBuffer(path, deckBuf_, DECK_BYTES);
  if (n == 0) return false;

  // Parse in place: one "question|answer" per line.
  cardCount_ = 0;
  char* p = deckBuf_;
  char* end = deckBuf_ + n;
  while (p < end && cardCount_ < MAX_CARDS) {
    char* line = p;
    while (p < end && *p != '\n') p++;
    char* lineEnd = p;
    if (p < end) p++;                                  // skip '\n'
    if (lineEnd > line && lineEnd[-1] == '\r') lineEnd--;  // CRLF decks
    *lineEnd = 0;
    char* sep = strchr(line, '|');
    if (!sep || sep == line) continue;
    *sep = 0;
    questions_[cardCount_] = line;
    answers_[cardCount_] = sep + 1;
    cardCount_++;
  }
  cardIndex_ = 0;
  showingFront_ = true;
  return cardCount_ > 0;
}

void FlashcardsApp::drawScreen(UiApp::ScreenType& screen, void* self) {
  auto& a = *static_cast<FlashcardsApp*>(self);
  screen.target().fill(a.ctx_->ui.device().screen(), Paint::solid(Color::White));
  if (a.mode_ == Mode::DeckList) a.drawDeckList(screen);
  else a.drawCard(screen);
}

void FlashcardsApp::drawDeckList(UiApp::ScreenType& screen) {
  screen.header("Flashcards", "UP/DOWN pick a deck, CONFIRM opens");
  if (deckCount_ == 0) {
    screen.spacer(24);
    screen.popup("No decks found.\nAdd /decks/<name>.txt with one\nquestion|answer per line.");
    return;
  }
  ListItem items[MAX_DECKS] = {};
  for (int i = 0; i < deckCount_; i++) {
    items[i].label = deckNames_[i];
    items[i].actionValue = i;
  }

  // Selection-driven: rows are not interactive (NO_ACTION registers nothing);
  // tick() owns UP/DOWN/CONFIRM and this keeps the selected row scrolled into
  // view — the focus system can't reach rows past the first screenful.
  const uint16_t visible = listVisibleRows(screen.body(), screen.theme().rowHeight, 0);
  deckTop_ = listTopIndexFor(deckSel_, deckTop_, visible, static_cast<uint16_t>(deckCount_));

  ListProps lp;
  lp.items = items;
  lp.count = static_cast<uint16_t>(deckCount_);
  lp.selectedIndex = deckSel_;
  lp.topIndex = deckTop_;
  lp.action = NO_ACTION;
  lp.rowHeight = 0;  // inherit theme metric
  screen.list(lp);
}

void FlashcardsApp::drawCard(UiApp::ScreenType& screen) {
  snprintf(header_, sizeof(header_), "Card %d/%d", cardIndex_ + 1, cardCount_);
  screen.header("Flashcards", header_, showingFront_ ? "Q" : "A");

  StatusBarProps bar;
  bar.leading = showingFront_ ? "DOWN: show answer" : "DOWN: next card";
  bar.trailing = "UP: prev  BACK: decks";
  bar.text.font = fonts::UI_SMALL;
  bar.fillBackground = true;
  screen.status(bar, LayoutAnchor::Bottom);

  screen.insetContent(Insets{16, 14, 12, 14});
  const Rect body = screen.body();

  TextStyle qStyle;
  qStyle.font = fonts::WRITER_BASE + 1;  // medium writer font for card text
  qStyle.align = TextAlign::Left;
  qStyle.maxLines = 10;

  const char* q = questions_[cardIndex_];
  const Size qSize = measureWrappedText(screen.target(), q, qStyle, body.width);
  Rect qRect{body.x, body.y, body.width, qSize.height < body.height ? qSize.height : body.height};
  screen.target().text(qRect, q, qStyle);

  if (!showingFront_) {
    const int16_t ay = static_cast<int16_t>(qRect.bottom() + 24);
    if (ay < body.bottom() - 8) {
      screen.target().line(Point{body.x, static_cast<int16_t>(ay - 12)},
                           Point{body.right(), static_cast<int16_t>(ay - 12)}, 1, Paint::solid(Color::Black));
      TextStyle aStyle = qStyle;
      aStyle.bold = true;
      Rect aRect{body.x, ay, body.width, static_cast<int16_t>(body.bottom() - ay)};
      const Size aSize = measureWrappedText(screen.target(), answers_[cardIndex_], aStyle, body.width);
      if (aSize.height < aRect.height) aRect.height = aSize.height;
      screen.target().text(aRect, answers_[cardIndex_], aStyle);
    }
  }
}
