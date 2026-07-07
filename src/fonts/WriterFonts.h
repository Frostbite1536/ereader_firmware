#pragma once

// Writer font sizes, generated from Noto Sans (OFL) with the SDK tool:
//   python freeink-sdk/libs/ui/FreeInkUI/tools/gen_font.py \
//       --ttf NotoSans-Regular.ttf --size <px> --name NotoSans<px> \
//       --out src/fonts/NotoSans<px>Font.h
// Regenerate at any size — or swap in your own TTF the same way — and rebuild.
// Missing headers fall back to the DisplayTarget's bundled Noto Sans so the
// firmware always compiles.

#include <FreeInkUIDisplayTarget.h>
#include <FreeInkUIFont.h>

#if __has_include("NotoSans16Font.h")
#include "NotoSans16Font.h"
#define WRITER_FONT_S ::freeink::ui::kNotoSans16Font
#else
#define WRITER_FONT_S ::freeink::ui::kNotoSansFont
#endif

#if __has_include("NotoSans20Font.h")
#include "NotoSans20Font.h"
#define WRITER_FONT_M ::freeink::ui::kNotoSans20Font
#else
#define WRITER_FONT_M ::freeink::ui::kNotoSansFont
#endif

#if __has_include("NotoSans25Font.h")
#include "NotoSans25Font.h"
#define WRITER_FONT_L ::freeink::ui::kNotoSans25Font
#else
#define WRITER_FONT_L ::freeink::ui::kNotoSansFont
#endif

#if __has_include("NotoSans31Font.h")
#include "NotoSans31Font.h"
#define WRITER_FONT_XL ::freeink::ui::kNotoSans31Font
#else
#define WRITER_FONT_XL ::freeink::ui::kNotoSansFont
#endif

// DisplayTarget font-slot assignments (CLAUDE.md): 0-2 UI, 3-6 writer sizes.
namespace fonts {
constexpr freeink::ui::FontId UI_SMALL = 0;
constexpr freeink::ui::FontId UI_BODY = 1;
constexpr freeink::ui::FontId UI_TITLE = 2;
constexpr freeink::ui::FontId WRITER_BASE = 3;  // +0..3 = S/M/L/XL

// UI chrome reuses the writer faces (16/20/25) instead of the bundled 34px
// Noto Sans: subtitle rows and headers fit, and the theme metrics are derived
// from the real body-font line height (themeTokensForLineHeight in main.cpp).
inline void installFonts(freeink::ui::DisplayTarget& target) {
  target.setFont(UI_SMALL, WRITER_FONT_S);
  target.setFont(UI_BODY, WRITER_FONT_M);
  target.setFont(UI_TITLE, WRITER_FONT_L);
  target.setFont(WRITER_BASE + 0, WRITER_FONT_S);
  target.setFont(WRITER_BASE + 1, WRITER_FONT_M);
  target.setFont(WRITER_BASE + 2, WRITER_FONT_L);
  target.setFont(WRITER_BASE + 3, WRITER_FONT_XL);
}

inline const char* sizeName(uint8_t idx) {
  switch (idx) {
    case 0: return "Small";
    case 1: return "Medium";
    case 2: return "Large";
    default: return "XL";
  }
}
}  // namespace fonts
