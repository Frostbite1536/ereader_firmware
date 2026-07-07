#pragma once

#include "AppContext.h"
#include "fonts/WriterFonts.h"

// Bottom-bezel button legend, CrossPoint style: four fixed slots aligned with
// the bottom row's physical order, left to right: BACK, CONFIRM, LEFT, RIGHT
// (HARDWARE.md §Buttons). Pure chrome — text only, no interactions registered,
// so it never joins the focus cycle. Pass nullptr to leave a slot blank.
// Call it after header() and before the screen's body content so takeBottom()
// reserves the strip. In landscape the buttons sit on a side bezel; the bar
// still reads as a plain legend there.
inline void drawButtonHints(UiApp::ScreenType& screen, const char* back, const char* confirm,
                            const char* left, const char* right) {
  using namespace freeink::ui;
  auto& t = screen.target();
  const int16_t lh = t.lineHeight(fonts::UI_SMALL);
  const Rect bar = screen.takeBottom(static_cast<int16_t>(lh + 8));
  if (bar.empty()) return;
  t.fill(Rect{bar.x, bar.y, bar.width, 1}, Paint::solid(Color::Black));

  TextStyle st;
  st.font = fonts::UI_SMALL;
  st.align = TextAlign::Center;
  const char* labels[4] = {back, confirm, left, right};
  const int16_t slotW = static_cast<int16_t>(bar.width / 4);
  for (int i = 0; i < 4; i++) {
    if (!labels[i] || !labels[i][0]) continue;
    const int16_t x = static_cast<int16_t>(bar.x + slotW * i);
    const int16_t w = i == 3 ? static_cast<int16_t>(bar.right() - x) : slotW;
    t.text(Rect{x, static_cast<int16_t>(bar.y + 5), w, lh}, labels[i], st);
  }
}
