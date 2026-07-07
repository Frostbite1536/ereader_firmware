#pragma once

#include <FreeInkUI.h>

// Dark-mode wrapper over the SDK DisplayTarget, replacing the SDK's
// InvertedDrawTarget (which is `final`, and freeink-sdk/ is never edited —
// INVARIANTS.md #10).
//
// Why it exists: DisplayTarget renders a gray dither by inking only the
// pattern's BLACK dots — "off" pixels leave the background untouched. That is
// what lets a dither band sit behind text in light mode, but it means an
// inverted gray fill on dark mode's black page draws black-on-black: the
// focus highlight was invisible in dark mode on real hardware (first X4
// test). Fix: when inversion is on, under-fill dither FILLS with explicit
// white first, so e.g. the light-mode highlight (25% black dots on white)
// becomes its exact visual inverse (25% white dots on black).
//
// Trade-off: a dither fill is no longer translucent in dark mode (the
// under-fill erases what's beneath), so a dialog scrim covers instead of
// dimming. Backgrounds are drawn before content everywhere in the SDK's
// components, so nothing else changes. Upstream candidate — docs/ROADMAP.md.
class DarkModeTarget final : public freeink::ui::DrawTarget {
 public:
  explicit DarkModeTarget(freeink::ui::DrawTarget& inner, bool enabled = true)
      : inner_(inner), enabled_(enabled) {}

  void setEnabled(bool enabled) { enabled_ = enabled; }
  bool enabled() const { return enabled_; }

  freeink::ui::Size measureText(freeink::ui::FontId font, const char* text,
                                freeink::ui::TextStyle style) const override {
    return inner_.measureText(font, text, style);
  }
  int16_t lineHeight(freeink::ui::FontId font) const override { return inner_.lineHeight(font); }

  void fill(freeink::ui::Rect rect, freeink::ui::Paint paint, uint8_t radius = 0,
            uint8_t corners = freeink::ui::CornersAll) override {
    using namespace freeink::ui;
    if (enabled_ && paint.kind == PaintKind::Dither) {
      inner_.fill(rect, Paint::solid(Color::White), radius, corners);
      inner_.fill(rect, invertedPaint(paint), radius, corners);
      return;
    }
    inner_.fill(rect, enabled_ ? invertedPaint(paint) : paint, radius, corners);
  }
  void stroke(freeink::ui::Rect rect, freeink::ui::Paint paint, uint8_t width, uint8_t radius = 0,
              uint8_t corners = freeink::ui::CornersAll) override {
    inner_.stroke(rect, enabled_ ? freeink::ui::invertedPaint(paint) : paint, width, radius, corners);
  }
  void line(freeink::ui::Point from, freeink::ui::Point to, uint8_t width, freeink::ui::Paint paint) override {
    inner_.line(from, to, width, enabled_ ? freeink::ui::invertedPaint(paint) : paint);
  }
  void triangle(freeink::ui::Point a, freeink::ui::Point b, freeink::ui::Point c,
                freeink::ui::Paint paint) override {
    inner_.triangle(a, b, c, enabled_ ? freeink::ui::invertedPaint(paint) : paint);
  }
  void text(freeink::ui::Rect rect, const char* text, freeink::ui::TextStyle style) override {
    using namespace freeink::ui;
    if (enabled_) {
      style.color = invertedColor(style.color);
      style.inverted = style.color == Color::White;
    }
    inner_.text(rect, text, style);
  }
  void bitmap(freeink::ui::Rect rect, freeink::ui::BitmapRef bitmap, freeink::ui::BitmapMode mode,
              freeink::ui::Paint foreground = freeink::ui::Paint::solid(freeink::ui::Color::Black),
              freeink::ui::Rotation rotation = freeink::ui::Rotation::None) override {
    inner_.bitmap(rect, bitmap, mode, enabled_ ? freeink::ui::invertedPaint(foreground) : foreground, rotation);
  }

 private:
  freeink::ui::DrawTarget& inner_;
  bool enabled_;
};
