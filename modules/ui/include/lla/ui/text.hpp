#pragma once

#include "lla/contracts.hpp"

#include <algorithm>
#include <string_view>
#include <vector>

namespace lla::ui {

inline constexpr int kGlyphWidth = 5;
inline constexpr int kGlyphHeight = 7;
inline constexpr int kGlyphSpacing = 1;

enum class TextAlign {
    Left,
    Center,
};

int text_width(std::string_view text, int scale);
int text_height(int scale);

// Space between wrapped lines. One line has no extra gap.
inline int wrapped_line_gap(int scale) {
    if (scale <= 0) {
        return 0;
    }
    return std::max(2, scale);
}

inline int text_block_height(int lines, int scale) {
    if (lines <= 0 || scale <= 0) {
        return 0;
    }
    return lines * text_height(scale) + (lines - 1) * wrapped_line_gap(scale);
}

inline Rect intersect_rect(Rect a, Rect b) {
    const float x0 = std::max(a.x, b.x);
    const float y0 = std::max(a.y, b.y);
    const float x1 = std::min(a.x + a.w, b.x + b.w);
    const float y1 = std::min(a.y + a.h, b.y + b.h);
    if (x1 <= x0 || y1 <= y0) {
        return {};
    }
    return {x0, y0, x1 - x0, y1 - y0};
}

// Break on spaces so each line fits max_width. A single glyph wider than max_width stays one line.
void wrap_lines(std::string_view text, int scale, int max_width, std::vector<std::string_view>& lines);
int wrapped_line_count(std::string_view text, int scale, int max_width);

void draw_text(ICanvas& canvas, float x, float y, std::string_view text, Color color, int scale);
void draw_text_clipped(ICanvas& canvas, float x, float y, std::string_view text, Color color, int scale, Rect clip);

// Draw text inside area. Glyphs that would leave the area are clipped.
void draw_text_box(
    ICanvas& canvas, Rect area, std::string_view text, Color color, int scale, TextAlign align, bool center_y);
void stroke_rect(ICanvas& canvas, Rect rect, Color color, int thickness);

}  // namespace lla::ui
