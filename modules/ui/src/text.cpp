#include "lla/ui/text.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <stdexcept>
#include <string_view>

namespace lla::ui {
namespace {

struct GlyphDef {
    char ch;
    const char* rows[7];
};

constexpr GlyphDef kGlyphs[] = {
    {'A', {"01110", "10001", "10001", "11111", "10001", "10001", "10001"}},
    {'B', {"11110", "10001", "10001", "11110", "10001", "10001", "11110"}},
    {'C', {"01111", "10000", "10000", "10000", "10000", "10000", "01111"}},
    {'D', {"11110", "10001", "10001", "10001", "10001", "10001", "11110"}},
    {'E', {"11111", "10000", "10000", "11110", "10000", "10000", "11111"}},
    {'F', {"11111", "10000", "10000", "11110", "10000", "10000", "10000"}},
    {'G', {"01111", "10000", "10000", "10111", "10001", "10001", "01111"}},
    {'H', {"10001", "10001", "10001", "11111", "10001", "10001", "10001"}},
    {'I', {"01110", "00100", "00100", "00100", "00100", "00100", "01110"}},
    {'J', {"00111", "00010", "00010", "00010", "00010", "10010", "01100"}},
    {'K', {"10001", "10010", "10100", "11000", "10100", "10010", "10001"}},
    {'L', {"10000", "10000", "10000", "10000", "10000", "10000", "11111"}},
    {'M', {"10001", "11011", "10101", "10101", "10001", "10001", "10001"}},
    {'N', {"10001", "11001", "10101", "10011", "10001", "10001", "10001"}},
    {'O', {"01110", "10001", "10001", "10001", "10001", "10001", "01110"}},
    {'P', {"11110", "10001", "10001", "11110", "10000", "10000", "10000"}},
    {'Q', {"01110", "10001", "10001", "10001", "10101", "10010", "01101"}},
    {'R', {"11110", "10001", "10001", "11110", "10100", "10010", "10001"}},
    {'S', {"01111", "10000", "10000", "01110", "00001", "00001", "11110"}},
    {'T', {"11111", "00100", "00100", "00100", "00100", "00100", "00100"}},
    {'U', {"10001", "10001", "10001", "10001", "10001", "10001", "01110"}},
    {'V', {"10001", "10001", "10001", "10001", "10001", "01010", "00100"}},
    {'W', {"10001", "10001", "10001", "10101", "10101", "10101", "01010"}},
    {'X', {"10001", "10001", "01010", "00100", "01010", "10001", "10001"}},
    {'Y', {"10001", "10001", "01010", "00100", "00100", "00100", "00100"}},
    {'Z', {"11111", "00001", "00010", "00100", "01000", "10000", "11111"}},
    {'a', {"00000", "00000", "01110", "00001", "01111", "10001", "01111"}},
    {'b', {"10000", "10000", "11110", "10001", "10001", "10001", "11110"}},
    {'c', {"00000", "00000", "01110", "10000", "10000", "10000", "01110"}},
    {'d', {"00001", "00001", "01111", "10001", "10001", "10001", "01111"}},
    {'e', {"00000", "00000", "01110", "10001", "11111", "10000", "01110"}},
    {'f', {"00110", "01001", "01000", "11100", "01000", "01000", "01000"}},
    {'g', {"00000", "01111", "10001", "10001", "01111", "00001", "01110"}},
    {'h', {"10000", "10000", "11110", "10001", "10001", "10001", "10001"}},
    {'i', {"00100", "00000", "01100", "00100", "00100", "00100", "01110"}},
    {'j', {"00010", "00000", "00110", "00010", "00010", "10010", "01100"}},
    {'k', {"10000", "10000", "10010", "10100", "11000", "10100", "10010"}},
    {'l', {"01100", "00100", "00100", "00100", "00100", "00100", "01110"}},
    {'m', {"00000", "00000", "11010", "10101", "10101", "10001", "10001"}},
    {'n', {"00000", "00000", "11110", "10001", "10001", "10001", "10001"}},
    {'o', {"00000", "00000", "01110", "10001", "10001", "10001", "01110"}},
    {'p', {"00000", "00000", "11110", "10001", "11110", "10000", "10000"}},
    {'q', {"00000", "00000", "01111", "10001", "01111", "00001", "00001"}},
    {'r', {"00000", "00000", "10110", "11001", "10000", "10000", "10000"}},
    {'s', {"00000", "00000", "01111", "10000", "01110", "00001", "11110"}},
    {'t', {"01000", "01000", "11100", "01000", "01000", "01001", "00110"}},
    {'u', {"00000", "00000", "10001", "10001", "10001", "10011", "01101"}},
    {'v', {"00000", "00000", "10001", "10001", "10001", "01010", "00100"}},
    {'w', {"00000", "00000", "10001", "10001", "10101", "10101", "01010"}},
    {'x', {"00000", "00000", "10001", "01010", "00100", "01010", "10001"}},
    {'y', {"00000", "00000", "10001", "10001", "01111", "00001", "01110"}},
    {'z', {"00000", "00000", "11111", "00010", "00100", "01000", "11111"}},
    {'0', {"01110", "10001", "10011", "10101", "11001", "10001", "01110"}},
    {'1', {"00100", "01100", "00100", "00100", "00100", "00100", "01110"}},
    {'2', {"01110", "10001", "00001", "00010", "00100", "01000", "11111"}},
    {'3', {"11110", "00001", "00001", "01110", "00001", "00001", "11110"}},
    {'4', {"00010", "00110", "01010", "10010", "11111", "00010", "00010"}},
    {'5', {"11111", "10000", "11110", "00001", "00001", "10001", "01110"}},
    {'6', {"00110", "01000", "10000", "11110", "10001", "10001", "01110"}},
    {'7', {"11111", "00001", "00010", "00100", "01000", "01000", "01000"}},
    {'8', {"01110", "10001", "10001", "01110", "10001", "10001", "01110"}},
    {'9', {"01110", "10001", "10001", "01111", "00001", "00010", "01100"}},
    {'.', {"00000", "00000", "00000", "00000", "00000", "01100", "01100"}},
    {'/', {"00001", "00010", "00010", "00100", "01000", "01000", "10000"}},
};

constexpr std::uint8_t kBox[7] = {0b11111, 0b10001, 0b10001, 0b10001, 0b10001, 0b10001, 0b11111};

std::uint8_t pack_row(const char* row) {
    if (std::strlen(row) != 5) {
        throw std::logic_error("glyph row must be 5 characters");
    }
    std::uint8_t bits = 0;
    for (int column = 0; column < 5; ++column) {
        if (row[column] == '1') {
            bits = static_cast<std::uint8_t>(bits | (1u << (4 - column)));
        }
    }
    return bits;
}

const std::uint8_t* glyph(char ch) {
    static const auto table = [] {
        std::array<std::array<std::uint8_t, 7>, 128> built{};
        for (const GlyphDef& def : kGlyphs) {
            const auto index = static_cast<unsigned char>(def.ch);
            for (int row = 0; row < 7; ++row) {
                built[index][static_cast<std::size_t>(row)] = pack_row(def.rows[row]);
            }
        }
        return built;
    }();

    const auto index = static_cast<unsigned char>(ch);
    if (index >= table.size()) {
        return kBox;
    }
    bool any = false;
    for (std::uint8_t bits : table[index]) {
        any = any || bits != 0;
    }
    if (!any && ch != ' ') {
        return kBox;
    }
    return table[index].data();
}

}  // namespace

int text_width(std::string_view text, int scale) {
    if (text.empty() || scale <= 0) {
        return 0;
    }
    const int advance = (kGlyphWidth + kGlyphSpacing) * scale;
    return static_cast<int>(text.size()) * advance - kGlyphSpacing * scale;
}

int text_height(int scale) {
    return scale > 0 ? kGlyphHeight * scale : 0;
}

namespace {

std::string_view trim_right(std::string_view value) {
    while (!value.empty() && value.back() == ' ') {
        value.remove_suffix(1);
    }
    return value;
}

void draw_text_inner(ICanvas& canvas, float x, float y, std::string_view text, Color color, int scale, const Rect* clip) {
    if (scale <= 0) {
        return;
    }
    const float advance = static_cast<float>((kGlyphWidth + kGlyphSpacing) * scale);
    const float pixel = static_cast<float>(scale);
    float cursor = x;
    for (char ch : text) {
        if (ch != ' ') {
            const std::uint8_t* rows = glyph(ch);
            for (int row = 0; row < kGlyphHeight; ++row) {
                int run_start = -1;
                for (int column = 0; column <= kGlyphWidth; ++column) {
                    const bool on = column < kGlyphWidth && ((rows[row] >> (4 - column)) & 1u) != 0;
                    if (on && run_start < 0) {
                        run_start = column;
                    }
                    if (!on && run_start >= 0) {
                        Rect glyph{
                            cursor + static_cast<float>(run_start) * pixel,
                            y + static_cast<float>(row) * pixel,
                            static_cast<float>(column - run_start) * pixel,
                            pixel,
                        };
                        if (clip != nullptr) {
                            glyph = intersect_rect(glyph, *clip);
                        }
                        if (glyph.w > 0.f && glyph.h > 0.f) {
                            canvas.fill_rect(glyph, color);
                        }
                        run_start = -1;
                    }
                }
            }
        }
        cursor += advance;
    }
}

}  // namespace

void wrap_lines(std::string_view text, int scale, int max_width, std::vector<std::string_view>& lines) {
    lines.clear();
    if (text.empty() || scale <= 0) {
        return;
    }
    if (max_width < 1) {
        max_width = 1;
    }
    std::size_t index = 0;
    while (index < text.size()) {
        if (!lines.empty()) {
            while (index < text.size() && text[index] == ' ') {
                ++index;
            }
            if (index >= text.size()) {
                break;
            }
        }
        std::size_t end = index;
        std::size_t break_at = std::string_view::npos;
        while (end < text.size()) {
            const int width = text_width(text.substr(index, end + 1 - index), scale);
            if (width > max_width) {
                break;
            }
            if (text[end] == ' ') {
                break_at = end;
            }
            ++end;
        }
        if (end >= text.size()) {
            const std::string_view line = trim_right(text.substr(index, text.size() - index));
            if (!line.empty()) {
                lines.push_back(line);
            }
            break;
        }
        if (break_at != std::string_view::npos && break_at > index) {
            const std::string_view line = trim_right(text.substr(index, break_at - index));
            if (!line.empty()) {
                lines.push_back(line);
            }
            index = break_at + 1;
            continue;
        }
        if (end == index) {
            lines.push_back(text.substr(index, 1));
            ++index;
            continue;
        }
        lines.push_back(text.substr(index, end - index));
        index = end;
    }
}

int wrapped_line_count(std::string_view text, int scale, int max_width) {
    std::vector<std::string_view> lines;
    wrap_lines(text, scale, max_width, lines);
    return static_cast<int>(lines.size());
}

void draw_text(ICanvas& canvas, float x, float y, std::string_view text, Color color, int scale) {
    draw_text_inner(canvas, x, y, text, color, scale, nullptr);
}

void draw_text_clipped(ICanvas& canvas, float x, float y, std::string_view text, Color color, int scale, Rect clip) {
    draw_text_inner(canvas, x, y, text, color, scale, &clip);
}

void draw_text_box(
    ICanvas& canvas, Rect area, std::string_view text, Color color, int scale, TextAlign align, bool center_y) {
    if (scale <= 0 || area.w <= 0.f || area.h <= 0.f || text.empty()) {
        return;
    }
    std::vector<std::string_view> lines;
    const int max_width = std::max(1, static_cast<int>(std::floor(area.w)));
    wrap_lines(text, scale, max_width, lines);
    if (lines.empty()) {
        return;
    }
    const int line_gap = wrapped_line_gap(scale);
    const int line_h = text_height(scale);
    const int block = text_block_height(static_cast<int>(lines.size()), scale);
    float y = area.y;
    if (center_y) {
        y = area.y + (area.h - static_cast<float>(block)) * 0.5f;
        if (y < area.y) {
            y = area.y;
        }
    }
    for (std::string_view line : lines) {
        if (y >= area.y + area.h) {
            break;
        }
        float x = area.x;
        if (align == TextAlign::Center) {
            const float width = static_cast<float>(text_width(line, scale));
            x = area.x + (area.w - width) * 0.5f;
            if (x < area.x) {
                x = area.x;
            }
        }
        draw_text_clipped(canvas, std::round(x), std::round(y), line, color, scale, area);
        y += static_cast<float>(line_h + line_gap);
    }
}

void stroke_rect(ICanvas& canvas, Rect rect, Color color, int thickness) {
    if (thickness <= 0 || rect.w <= 0 || rect.h <= 0) {
        return;
    }
    const float t = static_cast<float>(thickness);
    canvas.fill_rect({rect.x, rect.y, rect.w, t}, color);
    canvas.fill_rect({rect.x, rect.y + rect.h - t, rect.w, t}, color);
    canvas.fill_rect({rect.x, rect.y, t, rect.h}, color);
    canvas.fill_rect({rect.x + rect.w - t, rect.y, t, rect.h}, color);
}

}  // namespace lla::ui
