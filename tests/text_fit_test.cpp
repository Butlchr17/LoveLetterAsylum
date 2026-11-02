#include "lla/ui/menu_layout.hpp"
#include "lla/ui/text.hpp"

#include <cmath>
#include <iostream>
#include <string>
#include <string_view>
#include <vector>

namespace {

int fail(const char* message) {
    std::cerr << message << '\n';
    return 1;
}

bool inside(lla::Rect outer, lla::Rect inner) {
    if (inner.w < 0.f || inner.h < 0.f) {
        return false;
    }
    return inner.x >= outer.x - 0.05f && inner.y >= outer.y - 0.05f && inner.x + inner.w <= outer.x + outer.w + 0.05f &&
           inner.y + inner.h <= outer.y + outer.h + 0.05f;
}

bool overlaps(lla::Rect a, lla::Rect b) {
    return a.w > 0.5f && a.h > 0.5f && b.w > 0.5f && b.h > 0.5f && a.x < b.x + b.w - 0.05f && b.x < a.x + a.w - 0.05f &&
           a.y < b.y + b.h - 0.05f && b.y < a.y + a.h - 0.05f;
}

bool lines_fit(std::string_view text, int scale, lla::Rect area) {
    if (text.empty() || area.h <= 0.f) {
        return true;
    }
    const int width = std::max(1, static_cast<int>(std::floor(area.w)));
    std::vector<std::string_view> lines;
    lla::ui::wrap_lines(text, scale, width, lines);
    const int block = lla::ui::text_block_height(static_cast<int>(lines.size()), scale);
    if (static_cast<float>(block) > area.h + 0.05f) {
        return false;
    }
    for (std::string_view line : lines) {
        const int line_w = lla::ui::text_width(line, scale);
        if (line_w > width && line.size() > 1) {
            return false;
        }
    }
    return true;
}

bool stack_fits(const lla::ui::MenuStack& stack, const lla::ui::MenuSpec& spec, int window_w, int window_h, bool require_full_text) {
    const lla::Rect window{0.f, 0.f, static_cast<float>(window_w), static_cast<float>(window_h)};
    if (!inside(window, stack.panel)) {
        return false;
    }
    const lla::Rect panel_inside{
        stack.panel.x + 2.f,
        stack.panel.y + 2.f,
        std::max(0.f, stack.panel.w - 4.f),
        std::max(0.f, stack.panel.h - 4.f),
    };
    std::vector<lla::Rect> boxes;
    boxes.push_back(stack.title);
    boxes.insert(boxes.end(), stack.preface_lines.begin(), stack.preface_lines.end());
    boxes.insert(boxes.end(), stack.rows.begin(), stack.rows.end());
    boxes.push_back(stack.status);
    boxes.push_back(stack.hint);
    for (const lla::Rect& box : boxes) {
        if (!inside(panel_inside, box) && box.w > 0.f && box.h > 0.f) {
            return false;
        }
    }
    for (std::size_t i = 0; i < boxes.size(); ++i) {
        for (std::size_t j = i + 1; j < boxes.size(); ++j) {
            if (overlaps(boxes[i], boxes[j])) {
                return false;
            }
        }
    }
    if (!require_full_text) {
        return true;
    }
    if (!lines_fit(spec.title, spec.title_scale, stack.title)) {
        return false;
    }
    if (!lines_fit(spec.hint, spec.text_scale, stack.hint)) {
        return false;
    }
    if (!lines_fit(spec.status, spec.text_scale, stack.status)) {
        return false;
    }
    const int row_count = std::min(static_cast<int>(spec.rows.size()), static_cast<int>(stack.rows.size()));
    for (int index = 0; index < row_count; ++index) {
        const lla::Rect text = lla::ui::text_area(stack.rows[static_cast<std::size_t>(index)], stack.row_pad_x, stack.row_pad_right);
        if (!lines_fit(spec.rows[static_cast<std::size_t>(index)], spec.text_scale, text)) {
            return false;
        }
    }
    return true;
}

struct ListCanvas final : lla::ICanvas {
    int width() const override { return 1280; }
    int height() const override { return 720; }
    void clear(lla::Color) override {}
    void fill_rect(lla::Rect rect, lla::Color) override { rects.push_back(rect); }
    std::vector<lla::Rect> rects;
};

bool pixels_inside(const ListCanvas& canvas, lla::Rect area) {
    for (const lla::Rect& rect : canvas.rects) {
        if (!inside(area, rect)) {
            return false;
        }
    }
    return true;
}

lla::ui::MenuSpec main_spec(int width, int height, int scale) {
    lla::ui::MenuSpec spec;
    spec.window_w = width;
    spec.window_h = height;
    spec.text_scale = scale;
    spec.title_scale = scale + 1;
    spec.title = "Love Letter Asylum";
    spec.hint = "Up/Down select    Enter or click confirm";
    spec.status = "Credits has no handler yet.";
    spec.preferred_w = std::min(560.f, std::max(320.f, static_cast<float>(width) - 48.f));
    spec.preferred_h = std::min(540.f, std::max(420.f, static_cast<float>(height) - 48.f));
    return spec;
}

}  // namespace

int main() {
    const std::string_view entries[] = {"New Game", "Continue", "Options", "Credits", "Quit"};
    {
        lla::ui::MenuSpec spec = main_spec(1280, 720, 2);
        spec.rows = entries;
        const lla::ui::MenuStack stack = lla::ui::layout_menu(spec);
        if (std::abs(stack.panel.w - 560.f) > 0.1f || std::abs(stack.panel.h - 540.f) > 0.1f) {
            return fail("small text should keep the main menu panel");
        }
        if (stack.rows.size() != 5 || std::abs(stack.rows[0].y - (stack.panel.y + 108.f)) > 0.1f || std::abs(stack.rows[0].h - 48.f) > 0.1f) {
            return fail("small text should keep the main menu rows");
        }
        if (std::abs(stack.hint.y - (stack.panel.y + stack.panel.h - 34.f)) > 0.1f) {
            return fail("small text should keep the main menu hint");
        }
        if (!stack_fits(stack, spec, 1280, 720, true)) {
            return fail("small main menu text leaves its boxes");
        }
    }

    const int scales[] = {2, 3, 4};
    const int windows[][2] = {{1280, 720}, {2560, 1440}, {320, 240}};
    for (int scale : scales) {
        for (const auto& window : windows) {
            lla::ui::MenuSpec spec = main_spec(window[0], window[1], scale);
            spec.rows = entries;
            const lla::ui::MenuStack stack = lla::ui::layout_menu(spec);
            if (!stack_fits(stack, spec, window[0], window[1], window[1] >= 720)) {
                std::cerr << "main menu overflow at scale " << scale << " " << window[0] << "x" << window[1] << '\n';
                return 1;
            }
        }
    }

    const std::string_view controls[] = {
        "Move left Backspace", "Move left 2 Backspace", "Move right Backspace", "Move right 2 Backspace", "Menu up Backspace",
        "Menu up 2 Backspace", "Menu down Backspace", "Menu down 2 Backspace", "Confirm Backspace", "Confirm 2 Backspace",
        "Pause Backspace", "Restore defaults", "Back",
    };
    for (int scale : scales) {
        for (const auto& window : windows) {
            lla::ui::MenuSpec spec;
            spec.window_w = window[0];
            spec.window_h = window[1];
            spec.text_scale = scale;
            spec.title_scale = scale + 1;
            spec.title = "Keep this display";
            spec.hint = "Up/Down select    Enter or click confirm";
            spec.status = "Key in use";
            spec.preferred_w = std::min(760.f, std::max(560.f, static_cast<float>(window[0]) - 48.f));
            spec.preferred_h = std::min(680.f, std::max(520.f, static_cast<float>(window[1]) - 32.f));
            spec.min_row_h = 40.f;
            spec.row_gap = 6.f;
            spec.side_pad = 28.f;
            spec.row_pad_x = 18.f;
            spec.row_pad_right = 16.f;
            spec.title_top = 28.f;
            spec.title_gap = 35.f;
            spec.footer_gap = 12.f;
            spec.status_gap = 10.f;
            spec.bottom_pad = 14.f;
            spec.rows = controls;
            const lla::ui::MenuStack stack = lla::ui::layout_menu(spec);
            if (!stack_fits(stack, spec, window[0], window[1], window[1] >= 720)) {
                std::cerr << "settings overflow at scale " << scale << " " << window[0] << "x" << window[1] << '\n';
                return 1;
            }
            if (scale == 2 && window[0] == 1280 && (std::abs(stack.panel.h - 680.f) > 0.1f || stack.rows.empty() || std::abs(stack.rows[0].h - 35.f) > 0.1f)) {
                std::cerr << "controls panel " << stack.panel.h << " row " << (stack.rows.empty() ? -1.f : stack.rows[0].h) << '\n';
                return fail("small text should keep the controls panel");
            }
            if (scale == 4 && window[0] == 2560) {
                const int text_h = lla::ui::text_height(4);
                if (stack.rows.empty() || stack.rows[0].h + 0.1f < static_cast<float>(text_h + 8)) {
                    return fail("large text rows should hold the glyphs");
                }
            }
        }
    }

    {
        std::vector<std::string_view> lines;
        lla::ui::wrap_lines("Up/Down select    Enter or click confirm", 4, 400, lines);
        if (lines.size() < 2) {
            return fail("wide hint should wrap");
        }
        for (std::string_view line : lines) {
            if (lla::ui::text_width(line, 4) > 400) {
                return fail("wrapped hint line is too wide");
            }
        }
    }

    {
        ListCanvas canvas;
        const lla::Rect clip{40.f, 50.f, 36.f, 18.f};
        lla::ui::draw_text_box(canvas, clip, "Up/Down select    Enter or click confirm", {255, 255, 255, 255}, 4, lla::ui::TextAlign::Left, false);
        if (canvas.rects.empty() || !pixels_inside(canvas, clip)) {
            return fail("clipped text drew outside its box");
        }
    }

    {
        ListCanvas canvas;
        const lla::Rect row{10.f, 10.f, 80.f, 36.f};
        const lla::ui::Theme theme = lla::ui::theme_from(4, false);
        lla::ui::draw_menu_button(canvas, row, "Move left 2 Backspace", true, theme, 18.f, 16.f);
        if (!pixels_inside(canvas, row)) {
            return fail("button text drew outside the row");
        }
    }

    std::cout << "ALL_OK\n";
    return 0;
}
