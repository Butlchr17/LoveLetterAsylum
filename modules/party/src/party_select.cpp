#include "lla/party/register.hpp"

#include "lla/party/party.hpp"
#include "lla/ui/menu_layout.hpp"
#include "lla/ui/text.hpp"
#include "lla/ui/theme.hpp"

#include <algorithm>
#include <string>
#include <string_view>
#include <vector>

namespace lla {
namespace {

constexpr int kRankRows = kPartySlotCount;
constexpr int kEditRows = kRankRows + 1;
constexpr int kPauseRows = 3;

constexpr const char* kTitle = "Party";
constexpr const char* kProceed = "Proceed";
constexpr const char* kConfirmTitle = "Keep this party";
constexpr const char* kBack = "Back";
constexpr const char* kNeedTwo = "Pick 2 colors";
constexpr const char* kWriteFailed = "Could not write the save.";
constexpr const char* kPlaceHint = "Rank 1 is left. Rank 4 is front.";
constexpr const char* kChangeHint = "Up/Down select    Left/Right change";
constexpr const char* kConfirmHint = "Up/Down select    Enter or click confirm";
constexpr const char* kPauseTitle = "Paused";
constexpr const char* kContinue = "Continue";
constexpr const char* kOptions = "Options";
constexpr const char* kExit = "Exit to main menu";
constexpr const char* kPauseLabels[kPauseRows] = {kContinue, kOptions, kExit};
const std::string_view kPauseRowText[] = {kContinue, kOptions, kExit};
const std::string_view kConfirmRows[] = {kProceed, kBack};

constexpr Color kEmptySlot{28, 30, 36, 255};

std::string rank_label(int slot, int class_id) {
    const ClassDef* def = find_class(class_id);
    const char* name = def == nullptr ? "Empty" : def->name;
    return "Rank " + std::to_string(slot + 1) + " " + name;
}

ui::MenuSpec edit_spec(int width, int height, std::string_view status, const ui::Theme& theme) {
    ui::MenuSpec spec;
    spec.window_w = width;
    spec.window_h = height;
    spec.text_scale = theme.text_scale;
    spec.title_scale = theme.title_scale;
    spec.title = kTitle;
    spec.status = status;
    spec.hint = kConfirmHint;
    spec.preferred_w = std::min(640.f, std::max(420.f, static_cast<float>(width) - 48.f));
    spec.preferred_h = std::min(620.f, std::max(460.f, static_cast<float>(height) - 48.f));
    spec.min_row_h = 44.f;
    spec.row_gap = 8.f;
    spec.title_gap = 28.f;
    return spec;
}

int navigate_rows(const InputFrame& input, const Rect* rows, int count, int& index) {
    if (input.mouse_moved) {
        for (int row = 0; row < count; ++row) {
            if (rows[row].contains(input.mouse_x, input.mouse_y)) {
                index = row;
                break;
            }
        }
    }
    if (input.mouse_pressed) {
        for (int row = 0; row < count; ++row) {
            if (rows[row].contains(input.mouse_x, input.mouse_y)) {
                index = row;
                return row;
            }
        }
    }
    if (count <= 0) {
        return -1;
    }
    if (input.up && !input.down) {
        index = (index + count - 1) % count;
    } else if (input.down && !input.up) {
        index = (index + 1) % count;
    }
    if (input.confirm) {
        return index;
    }
    return -1;
}

ui::MenuStack pause_layout(int width, int height, const ui::Theme& theme) {
    ui::MenuSpec spec;
    spec.window_w = width;
    spec.window_h = height;
    spec.text_scale = theme.text_scale;
    spec.title_scale = theme.title_scale;
    spec.title = kPauseTitle;
    spec.hint = kConfirmHint;
    spec.preferred_w = std::min(560.f, std::max(320.f, static_cast<float>(width) - 48.f));
    spec.preferred_h = std::min(400.f, std::max(360.f, static_cast<float>(height) - 80.f));
    spec.rows = kPauseRowText;
    return ui::layout_menu(spec);
}

void draw_pause(ICanvas& canvas, int index, const ui::Theme& theme) {
    ui::MenuSpec spec;
    spec.window_w = canvas.width();
    spec.window_h = canvas.height();
    spec.text_scale = theme.text_scale;
    spec.title_scale = theme.title_scale;
    spec.title = kPauseTitle;
    spec.hint = kConfirmHint;
    spec.preferred_w = std::min(560.f, std::max(320.f, static_cast<float>(canvas.width()) - 48.f));
    spec.preferred_h = std::min(400.f, std::max(360.f, static_cast<float>(canvas.height()) - 80.f));
    spec.rows = kPauseRowText;
    const ui::MenuStack layout = ui::layout_menu(spec);
    ui::draw_menu_panel(canvas, layout, theme, false);
    for (int row = 0; row < kPauseRows && row < static_cast<int>(layout.rows.size()); ++row) {
        ui::draw_menu_button(
            canvas, layout.rows[static_cast<std::size_t>(row)], kPauseLabels[row], row == index, theme, layout.row_pad_x, layout.row_pad_right);
    }
    ui::draw_menu_text(canvas, layout, spec, theme);
}

Color ink_for(Color color) {
    const int light = static_cast<int>(color.r) * 3 + static_cast<int>(color.g) * 6 + static_cast<int>(color.b);
    if (light > 1400) {
        return {16, 16, 20, 255};
    }
    return {236, 236, 240, 255};
}

void draw_strip(ICanvas& canvas, Rect area, const int* ranks, const ui::Theme& theme) {
    if (area.w < 40.f || area.h < 24.f) {
        return;
    }
    const float gap = std::max(8.f, area.w * 0.03f);
    const float box_w = (area.w - gap * 3.f) / 4.f;
    const float box_h = std::min(area.h, box_w * 1.35f);
    const float y = area.y + (area.h - box_h) * 0.5f;
    float x = area.x;
    for (int slot = 0; slot < kPartySlotCount; ++slot) {
        const Rect box{x, y, box_w, box_h};
        const ClassDef* def = find_class(ranks[slot]);
        canvas.fill_rect(box, def == nullptr ? kEmptySlot : def->color);
        ui::stroke_rect(canvas, box, theme.panel_edge, 2);
        const std::string number = std::to_string(slot + 1);
        const Color ink = def == nullptr ? theme.hint : ink_for(def->color);
        ui::draw_text_box(canvas, box, number, ink, theme.text_scale, ui::TextAlign::Center, true);
        x += box_w + gap;
    }
}

class PartyModule final : public IModule {
public:
    PartyModule(ISaveSlots* saves, IPlayPhase* phase, ISettingsScreen** settings, IDeviceSettings* devices)
        : saves_(saves), phase_(phase), settings_(settings), devices_(devices) {}

    std::string_view name() const override { return "party.select"; }

    void update(const InputFrame& input) override {
        if (phase_ == nullptr || phase_->phase() != PlayPhase::Party) {
            active_ = false;
            return;
        }
        if (!active_) {
            active_ = true;
            confirming_ = false;
            paused_ = false;
            index_ = 0;
            pause_index_ = 0;
            status_.clear();
            const int* stored = phase_->party_ranks();
            for (int slot = 0; slot < kPartySlotCount; ++slot) {
                ranks_[slot] = stored == nullptr ? 0 : stored[slot];
                if (!is_starter_class(ranks_[slot])) {
                    ranks_[slot] = 0;
                }
            }
        }
        if (input.window_width <= 0 || input.window_height <= 0) {
            return;
        }
        if (settings_open()) {
            return;
        }
        if (input.pause) {
            paused_ = !paused_;
            if (paused_) {
                pause_index_ = 0;
            }
            return;
        }
        if (paused_) {
            const ui::MenuStack layout = pause_layout(input.window_width, input.window_height, theme());
            const int activated = navigate_rows(input, layout.rows.data(), static_cast<int>(layout.rows.size()), pause_index_);
            if (activated == 0) {
                paused_ = false;
            } else if (activated == 1) {
                if (settings_ != nullptr && *settings_ != nullptr) {
                    (*settings_)->open();
                }
            } else if (activated == 2) {
                active_ = false;
                paused_ = false;
                confirming_ = false;
                phase_->return_to_menu();
            }
            return;
        }
        if (confirming_) {
            update_confirm(input);
            return;
        }
        update_edit(input);
    }

    void draw(ICanvas& canvas) override {
        if (!active_ || phase_ == nullptr || phase_->phase() != PlayPhase::Party) {
            return;
        }
        if (canvas.width() <= 0 || canvas.height() <= 0) {
            return;
        }
        const ui::Theme colors = theme();
        if (confirming_) {
            draw_confirm(canvas, colors);
        } else {
            draw_edit(canvas, colors);
        }
        if (paused_) {
            draw_pause(canvas, pause_index_, colors);
        }
    }

private:
    ui::Theme theme() const { return ui::theme_from(devices_); }

    bool settings_open() const {
        return settings_ != nullptr && *settings_ != nullptr && (*settings_)->is_open();
    }

    void update_edit(const InputFrame& input) {
        std::vector<std::string> labels;
        std::vector<std::string_view> views;
        collect_edit(labels, views);
        const std::string shown = status_.empty() ? std::string(kPlaceHint) : status_;
        ui::MenuSpec spec = edit_spec(input.window_width, input.window_height, shown, theme());
        spec.rows = views;
        const ui::MenuStack layout = ui::layout_menu(spec);
        const int previous = index_;
        const int activated = navigate_rows(input, layout.rows.data(), static_cast<int>(layout.rows.size()), index_);
        if (activated >= 0 && activated < kRankRows) {
            ranks_[activated] = step_rank(ranks_, activated, 1);
            status_.clear();
            return;
        }
        if (activated == kRankRows) {
            try_confirm();
            return;
        }
        if (index_ != previous) {
            return;
        }
        if (index_ >= 0 && index_ < kRankRows) {
            int direction = 0;
            if (input.left_pressed && !input.right_pressed) {
                direction = -1;
            } else if (input.right_pressed && !input.left_pressed) {
                direction = 1;
            }
            if (direction != 0) {
                ranks_[index_] = step_rank(ranks_, index_, direction);
                status_.clear();
            }
        }
    }

    void update_confirm(const InputFrame& input) {
        ui::MenuSpec spec = edit_spec(input.window_width, input.window_height, kPlaceHint, theme());
        spec.title = kConfirmTitle;
        spec.hint = kConfirmHint;
        spec.rows = kConfirmRows;
        const ui::MenuStack layout = ui::layout_menu(spec);
        const int activated = navigate_rows(input, layout.rows.data(), static_cast<int>(layout.rows.size()), index_);
        if (activated == 0) {
            commit_party();
        } else if (activated == 1) {
            confirming_ = false;
            index_ = kRankRows;
            status_.clear();
        }
    }

    void try_confirm() {
        if (!starting_party_ready(ranks_)) {
            status_ = kNeedTwo;
            return;
        }
        confirming_ = true;
        index_ = 0;
        status_.clear();
    }

    void commit_party() {
        if (saves_ == nullptr || phase_ == nullptr || !saves_->store_party(phase_->active_slot(), ranks_)) {
            status_ = kWriteFailed;
            confirming_ = false;
            index_ = kRankRows;
            return;
        }
        phase_->set_party_ranks(ranks_);
        active_ = false;
        confirming_ = false;
        phase_->begin_tutorial();
    }

    void collect_edit(std::vector<std::string>& labels, std::vector<std::string_view>& views) const {
        labels.clear();
        views.clear();
        labels.reserve(static_cast<std::size_t>(kEditRows));
        for (int slot = 0; slot < kRankRows; ++slot) {
            labels.push_back(rank_label(slot, ranks_[slot]));
        }
        labels.emplace_back(kProceed);
        views.reserve(labels.size());
        for (const std::string& label : labels) {
            views.emplace_back(label);
        }
    }

    void draw_edit(ICanvas& canvas, const ui::Theme& colors) const {
        std::vector<std::string> labels;
        std::vector<std::string_view> views;
        collect_edit(labels, views);
        const std::string shown = status_.empty() ? std::string(kPlaceHint) : status_;
        ui::MenuSpec spec = edit_spec(canvas.width(), canvas.height(), shown, colors);
        spec.rows = views;
        const ui::MenuStack layout = ui::layout_menu(spec);
        spec.hint = index_ < kRankRows ? kChangeHint : kConfirmHint;
        canvas.clear(colors.background);
        ui::draw_menu_panel(canvas, layout, colors, false);
        for (int row = 0; row < static_cast<int>(labels.size()) && row < static_cast<int>(layout.rows.size()); ++row) {
            ui::draw_menu_button(
                canvas, layout.rows[static_cast<std::size_t>(row)], labels[static_cast<std::size_t>(row)], row == index_, colors,
                layout.row_pad_x, layout.row_pad_right);
        }
        if (!layout.rows.empty()) {
            const Rect& last = layout.rows.back();
            const float top = last.y + last.h + 8.f;
            const float bottom = layout.status.y - 8.f;
            draw_strip(canvas, {layout.panel.x + 28.f, top, std::max(0.f, layout.panel.w - 56.f), std::max(0.f, bottom - top)}, ranks_, colors);
        }
        ui::draw_menu_text(canvas, layout, spec, colors);
    }

    void draw_confirm(ICanvas& canvas, const ui::Theme& colors) const {
        ui::MenuSpec spec = edit_spec(canvas.width(), canvas.height(), kPlaceHint, colors);
        spec.title = kConfirmTitle;
        spec.hint = kConfirmHint;
        spec.rows = kConfirmRows;
        const ui::MenuStack layout = ui::layout_menu(spec);
        canvas.clear(colors.background);
        ui::draw_menu_panel(canvas, layout, colors, false);
        for (int row = 0; row < 2 && row < static_cast<int>(layout.rows.size()); ++row) {
            ui::draw_menu_button(
                canvas, layout.rows[static_cast<std::size_t>(row)], kConfirmRows[row], row == index_, colors, layout.row_pad_x,
                layout.row_pad_right);
        }
        if (!layout.rows.empty()) {
            const Rect& last = layout.rows.back();
            const float top = last.y + last.h + 8.f;
            const float bottom = layout.status.y - 8.f;
            draw_strip(canvas, {layout.panel.x + 28.f, top, std::max(0.f, layout.panel.w - 56.f), std::max(0.f, bottom - top)}, ranks_, colors);
        }
        ui::draw_menu_text(canvas, layout, spec, colors);
    }

    ISaveSlots* saves_ = nullptr;
    IPlayPhase* phase_ = nullptr;
    ISettingsScreen** settings_ = nullptr;
    IDeviceSettings* devices_ = nullptr;
    bool active_ = false;
    bool confirming_ = false;
    bool paused_ = false;
    int index_ = 0;
    int pause_index_ = 0;
    int ranks_[kPartySlotCount] = {};
    std::string status_;
};

}  // namespace

void register_party(
    ModuleRegistry& registry, ISaveSlots* saves, IPlayPhase* phase, ISettingsScreen** settings, IDeviceSettings* devices) {
    registry.add<PartyModule>(saves, phase, settings, devices);
}

}  // namespace lla
