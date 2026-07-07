#pragma once

#include "../AppContext.h"

// Flashcards from plain text decks: /decks/*.txt, one card per line,
// "question|answer". DOWN flips / advances, UP goes back, BACK returns to the
// deck list (then the launcher).
//
// The deck list is selection-driven (UP/DOWN + CONFIRM handled in tick(), rows
// draw with selectedIndex/topIndex): unlike the UI focus system, this scrolls,
// so every deck stays reachable however many fit on screen.
class FlashcardsApp : public App {
 public:
  void begin(AppContext& ctx) override;
  void onEnter() override;
  void tick() override;
  const char* name() const override { return "Flashcards"; }

 private:
  enum class Mode : uint8_t { DeckList, Card };

  static constexpr size_t DECK_BYTES = 16 * 1024;
  static constexpr int MAX_CARDS = 200;
  static constexpr int MAX_DECKS = 24;

  void scanDecks();
  bool loadDeck(int index);

  static void drawScreen(UiApp::ScreenType& screen, void* self);
  void drawDeckList(UiApp::ScreenType& screen);
  void drawCard(UiApp::ScreenType& screen);

  AppContext* ctx_ = nullptr;
  Mode mode_ = Mode::DeckList;

  char deckNames_[MAX_DECKS][40];
  int deckCount_ = 0;
  int16_t deckSel_ = 0;   // selection, moved by UP/DOWN in tick()
  uint16_t deckTop_ = 0;  // first visible row (kept in range by the draw)

  char deckBuf_[DECK_BYTES];  // deck file, parsed in place
  const char* questions_[MAX_CARDS];
  const char* answers_[MAX_CARDS];
  int cardCount_ = 0;
  int cardIndex_ = 0;
  bool showingFront_ = true;
  char header_[48];
};
