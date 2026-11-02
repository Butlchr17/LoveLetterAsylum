#pragma once

#include "lla/ui/text.hpp"
#include "lla/ui/theme.hpp"

#include <algorithm>
#include <cmath>
#include <span>
#include <string_view>
#include <vector>

namespace lla::ui {

// A menu panel whose text boxes stay inside the panel. At the small text size the
// preferred panel is kept. Larger text grows the panel up to the window, then wraps.
struct MenuSpec {
    int window_w = 0;
    int window_h = 0;
    int text_scale = 2;
    int title_scale = 3;
    std::string_view title;
    std::string_view status;
    std::string_view hint;
    std::span<const std::string_view> rows;
    std::span<const std::string_view> preface;
    float preferred_w = 560.f;
    float preferred_h = 540.f;
    float min_row_h = 48.f;
    float row_gap = 12.f;
    float side_pad = 36.f;
    float row_pad_x = 22.f;
    float row_pad_right = 16.f;
    float title_top = 36.f;
    float title_gap = 51.f;
    float footer_gap = 12.f;
    float status_gap = 16.f;
    float bottom_pad = 20.f;
    float window_margin = 8.f;
};

struct MenuStack {
    Rect panel{};
    Rect title{};
    Rect preface{};
    std::vector<Rect> preface_lines;
    std::vector<Rect> rows;
    Rect status{};
    Rect hint{};
    float row_pad_x = 22.f;
    float row_pad_right = 16.f;
};

inline int box_width(Rect rect) {
    return std::max(1, static_cast<int>(std::floor(rect.w)));
}

inline MenuStack layout_menu(const MenuSpec& spec) {
    MenuStack stack;
    stack.row_pad_x = spec.row_pad_x;
    stack.row_pad_right = spec.row_pad_right;

    const float window_w = static_cast<float>(std::max(spec.window_w, 1));
    const float window_h = static_cast<float>(std::max(spec.window_h, 1));
    float margin = spec.window_margin;
    if (window_w <= margin * 2.f || window_h <= margin * 2.f) {
        margin = 0.f;
    }
    const float limit_w = std::max(1.f, window_w - margin * 2.f);
    const float limit_h = std::max(1.f, window_h - margin * 2.f);

    const int text_scale = std::max(spec.text_scale, 1);
    const int title_scale = std::max(spec.title_scale, 1);

    int content = text_width(spec.title, title_scale);
    content = std::max(content, text_width(spec.hint, text_scale));
    content = std::max(content, text_width(spec.status, text_scale));
    for (std::string_view line : spec.preface) {
        content = std::max(content, text_width(line, text_scale));
    }
    const int row_pads = static_cast<int>(std::ceil(spec.row_pad_x + spec.row_pad_right));
    for (std::string_view label : spec.rows) {
        content = std::max(content, text_width(label, text_scale) + row_pads);
    }

    float panel_w = std::min(limit_w, std::max(std::min(spec.preferred_w, limit_w), static_cast<float>(content) + spec.side_pad * 2.f));
    panel_w = std::round(std::max(panel_w, 1.f));
    const float side = std::min(spec.side_pad, std::max(2.f, (panel_w - 4.f) * 0.5f));
    const float inner_w = std::max(1.f, panel_w - side * 2.f);
    const int inner_px = box_width(Rect{0.f, 0.f, inner_w, 1.f});
    const int row_text_px = std::max(1, inner_px - row_pads);

    const int title_lines = std::max(1, wrapped_line_count(spec.title, title_scale, inner_px));
    const int hint_lines = spec.hint.empty() ? 1 : std::max(1, wrapped_line_count(spec.hint, text_scale, inner_px));
    int status_lines = 1;
    if (!spec.status.empty()) {
        status_lines = std::max(1, wrapped_line_count(spec.status, text_scale, inner_px));
    }
    const float title_block = static_cast<float>(text_block_height(title_lines, title_scale));
    const float hint_block = static_cast<float>(text_block_height(hint_lines, text_scale));
    const float status_block = static_cast<float>(text_block_height(status_lines, text_scale));

    int row_lines = 1;
    for (std::string_view label : spec.rows) {
        row_lines = std::max(row_lines, std::max(1, wrapped_line_count(label, text_scale, row_text_px)));
    }
    const float row_text = static_cast<float>(text_block_height(row_lines, text_scale));
    // 12px keeps glyphs off the button border. 4px is the last pad before clipping.
    const float comfort = row_text + 12.f;
    const float design = std::max(spec.min_row_h, comfort);

    float preface_h = 0.f;
    std::vector<float> preface_heights;
    preface_heights.reserve(spec.preface.size());
    for (std::size_t index = 0; index < spec.preface.size(); ++index) {
        const int lines = std::max(1, wrapped_line_count(spec.preface[index], text_scale, inner_px));
        const float height = static_cast<float>(text_block_height(lines, text_scale));
        preface_heights.push_back(height);
        preface_h += height;
        if (index + 1 < spec.preface.size()) {
            preface_h += static_cast<float>(wrapped_line_gap(text_scale));
        }
    }

    MenuSpec gap = spec;
    const int row_count = static_cast<int>(spec.rows.size());
    auto rows_start_of = [&]() {
        float start = gap.title_top + title_block + gap.title_gap + preface_h;
        if (!spec.preface.empty()) {
            start += gap.title_gap;
        }
        return start;
    };
    auto footer_of = [&]() {
        return gap.bottom_pad + hint_block + gap.status_gap + status_block + gap.footer_gap;
    };
    auto row_gaps = [&]() {
        return row_count > 1 ? static_cast<float>(row_count - 1) * gap.row_gap : 0.f;
    };
    auto space_for_rows = [&](float panel_h) { return panel_h - rows_start_of() - footer_of(); };
    auto fitted_row_h = [&](float panel_h) {
        if (row_count <= 0) {
            return design;
        }
        const float raw = (space_for_rows(panel_h) - row_gaps()) / static_cast<float>(row_count);
        if (raw < 1.f) {
            return raw;
        }
        return std::floor(raw);
    };

    float panel_h = std::min(std::max(spec.preferred_h, 1.f), limit_h);
    float row_h = design;
    const float fitted_preferred = fitted_row_h(panel_h);
    if (row_count > 0 && fitted_preferred < design) {
        if (fitted_preferred >= comfort) {
            row_h = fitted_preferred;
        } else {
            const float need = rows_start_of() + static_cast<float>(row_count) * comfort + row_gaps() + footer_of();
            if (need <= limit_h) {
                panel_h = std::max(panel_h, need);
                row_h = comfort;
            } else {
                panel_h = limit_h;
                auto shrink = [&](float& value, float floor_value, float per_unit) {
                    if (per_unit <= 0.f || fitted_row_h(panel_h) >= comfort) {
                        return;
                    }
                    const float have = fitted_row_h(panel_h);
                    if (have >= comfort) {
                        return;
                    }
                    const float deficit = (comfort - std::max(have, 0.f)) * static_cast<float>(row_count);
                    const float can = value - floor_value;
                    if (can <= 0.f || deficit <= 0.f) {
                        return;
                    }
                    value -= std::min(can, deficit / per_unit);
                };
                shrink(gap.title_gap, 8.f, 1.f);
                shrink(gap.footer_gap, 8.f, 1.f);
                shrink(gap.status_gap, 4.f, 1.f);
                shrink(gap.row_gap, 4.f, row_count > 1 ? static_cast<float>(row_count - 1) : 1.f);
                shrink(gap.title_top, 12.f, 1.f);
                shrink(gap.bottom_pad, 8.f, 1.f);
                const float fitted = fitted_row_h(panel_h);
                if (fitted >= comfort) {
                    row_h = comfort;
                } else {
                    row_h = std::max(1.f, fitted);
                }
            }
        }
    }

    panel_h = std::round(std::min(limit_h, std::max(panel_h, 1.f)));
    // Rounding can steal the last pixel the row fit was counting on.
    if (row_count > 0) {
        const float fitted = fitted_row_h(panel_h);
        if (fitted + 0.01f < row_h) {
            row_h = std::max(1.f, fitted);
        }
    }

    stack.panel = {
        std::round((window_w - panel_w) * 0.5f),
        std::round((window_h - panel_h) * 0.5f),
        panel_w,
        panel_h,
    };

    const float content_x = stack.panel.x + side;
    stack.title = {content_x, stack.panel.y + gap.title_top, inner_w, title_block};

    float preface_y = stack.title.y + stack.title.h + gap.title_gap;
    stack.preface = {content_x, preface_y, inner_w, preface_h};
    for (float height : preface_heights) {
        stack.preface_lines.push_back({content_x, preface_y, inner_w, height});
        preface_y += height + static_cast<float>(wrapped_line_gap(text_scale));
    }

    const float rows_y = stack.panel.y + rows_start_of();
    stack.rows.reserve(static_cast<std::size_t>(row_count));
    float row_y = rows_y;
    for (int index = 0; index < row_count; ++index) {
        stack.rows.push_back({content_x, row_y, inner_w, std::max(row_h, 1.f)});
        row_y += row_h + gap.row_gap;
    }

    stack.hint = {content_x, stack.panel.y + panel_h - gap.bottom_pad - hint_block, inner_w, hint_block};
    stack.status = {
        content_x, stack.hint.y - gap.status_gap - status_block, inner_w, status_block,
    };

    // Keep every text box inside the panel and off the box above it when the window is too short.
    std::vector<Rect*> boxes;
    boxes.reserve(2 + stack.preface_lines.size() + stack.rows.size());
    boxes.push_back(&stack.title);
    for (Rect& line : stack.preface_lines) {
        boxes.push_back(&line);
    }
    for (Rect& row : stack.rows) {
        boxes.push_back(&row);
    }
    boxes.push_back(&stack.status);
    boxes.push_back(&stack.hint);
    const float band_top = stack.panel.y + 2.f;
    const float band_bottom = stack.panel.y + stack.panel.h - 2.f;
    bool crowded = false;
    float previous_bottom = band_top;
    for (const Rect* box : boxes) {
        if (box->h <= 0.f) {
            continue;
        }
        if (box->y < previous_bottom - 0.01f || box->y + box->h > band_bottom + 0.01f) {
            crowded = true;
            break;
        }
        previous_bottom = box->y + box->h;
    }
    if (crowded) {
        float sum = 0.f;
        int active = 0;
        for (const Rect* box : boxes) {
            if (box->h > 0.f) {
                sum += box->h;
                ++active;
            }
        }
        const float gap_px = 2.f;
        const float gaps = gap_px * static_cast<float>(std::max(active - 1, 0));
        const float budget = std::max(0.f, band_bottom - band_top - gaps);
        const float fit = sum > 0.f ? std::min(1.f, budget / sum) : 1.f;
        float y = band_top;
        for (Rect* box : boxes) {
            if (box->h <= 0.f || y >= band_bottom) {
                box->y = band_bottom;
                box->h = 0.f;
                continue;
            }
            float height = box->h * fit;
            if (y + height > band_bottom) {
                height = band_bottom - y;
            }
            box->y = y;
            box->h = height;
            y += height + gap_px;
        }
        if (!stack.preface_lines.empty()) {
            stack.preface.y = stack.preface_lines.front().y;
            stack.preface.h = stack.preface_lines.back().y + stack.preface_lines.back().h - stack.preface.y;
        }
    }

    const Rect inside = intersect_rect(
        stack.panel, Rect{stack.panel.x + 2.f, stack.panel.y + 2.f, std::max(0.f, stack.panel.w - 4.f), std::max(0.f, stack.panel.h - 4.f)});
    auto clamp_box = [&](Rect rect) { return intersect_rect(rect, inside); };
    stack.title = clamp_box(stack.title);
    stack.preface = clamp_box(stack.preface);
    for (Rect& line : stack.preface_lines) {
        line = clamp_box(line);
    }
    for (Rect& row : stack.rows) {
        row = clamp_box(row);
    }
    stack.status = clamp_box(stack.status);
    stack.hint = clamp_box(stack.hint);
    return stack;
}

inline Rect text_area(Rect row, float pad_x, float pad_right) {
    const Rect inner{row.x + 2.f, row.y + 2.f, std::max(0.f, row.w - 4.f), std::max(0.f, row.h - 4.f)};
    const Rect padded{row.x + pad_x, row.y, std::max(0.f, row.w - pad_x - pad_right), row.h};
    return intersect_rect(inner, padded);
}

inline void draw_menu_button(
    ICanvas& canvas, Rect row, std::string_view label, bool selected, const Theme& theme, float pad_x, float pad_right) {
    canvas.fill_rect(row, selected ? theme.selected : theme.button);
    stroke_rect(canvas, row, selected ? theme.selected_edge : theme.button_edge, 2);
    if (selected) {
        canvas.fill_rect({row.x, row.y, std::min(6.f, row.w), row.h}, theme.selected_edge);
    }
    draw_text_box(canvas, text_area(row, pad_x, pad_right), label, theme.label, theme.text_scale, TextAlign::Left, true);
}

inline void draw_menu_panel(ICanvas& canvas, const MenuStack& stack, const Theme& theme, bool clear_first) {
    if (clear_first) {
        canvas.clear(theme.background);
    }
    canvas.fill_rect(stack.panel, theme.panel);
    stroke_rect(canvas, stack.panel, theme.panel_edge, 2);
}

inline void draw_menu_text(ICanvas& canvas, const MenuStack& stack, const MenuSpec& spec, const Theme& theme) {
    draw_text_box(canvas, stack.title, spec.title, theme.title, theme.title_scale, TextAlign::Center, false);
    for (std::size_t index = 0; index < stack.preface_lines.size() && index < spec.preface.size(); ++index) {
        draw_text_box(
            canvas, stack.preface_lines[index], spec.preface[index], theme.label, theme.text_scale, TextAlign::Center, true);
    }
    if (!spec.status.empty()) {
        draw_text_box(canvas, stack.status, spec.status, theme.status, theme.text_scale, TextAlign::Left, false);
    }
    draw_text_box(canvas, stack.hint, spec.hint, theme.hint, theme.text_scale, TextAlign::Left, false);
}

}  // namespace lla::ui
