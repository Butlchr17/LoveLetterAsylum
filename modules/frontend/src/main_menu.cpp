#include "lla/frontend/register.hpp"

#include "lla/ui/menu_layout.hpp"
#include "lla/ui/text.hpp"
#include "lla/ui/theme.hpp"

#include <algorithm>
#include <cmath>
#include <string>
#include <vector>

namespace lla {
namespace {

struct Entry {
    const char* label;
    AppCommand command;
};

constexpr Entry kEntries[] = {
    {"New Game", AppCommand::StartNewGame},
    {"Continue", AppCommand::Continue},
    {"Options", AppCommand::OpenOptions},
    {"Credits", AppCommand::OpenCredits},
    {"Quit", AppCommand::Quit},
};

constexpr int kEntryCount = static_cast<int>(sizeof(kEntries) / sizeof(kEntries[0]));
constexpr int kSlotRowCount = ISaveSlots::kSlotCount + 1;
constexpr const char* kTitle = "Love Letter Asylum";
constexpr const char* kHintText = "Up/Down select    Enter or click confirm";
constexpr const char* kSlotTitle = "Select a slot";
constexpr const char* kContinueTitle = "Continue";
constexpr const char* kStartedMessage = "The game has been started.";
constexpr const char* kExitHint = "Enter or click OK to exit";
constexpr const char* kOkLabel = "OK";
constexpr const char* kBackLabel = "Back";
constexpr const char* kWriteFailed = "Could not write the save.";
constexpr const char* kNoSaves = "No saves yet.";
constexpr const char* kReadFailed = "Could not read the save.";

enum class Screen {
    Main,
    Slots,
    ContinueSlots,
    Started,
};

ui::MenuSpec menu_spec(
    int width, int height, std::string_view title, std::string_view status, std::string_view hint, const ui::Theme& theme) {
    ui::MenuSpec spec;
    spec.window_w = width;
    spec.window_h = height;
    spec.text_scale = theme.text_scale;
    spec.title_scale = theme.title_scale;
    spec.title = title;
    spec.status = status;
    spec.hint = hint;
    spec.preferred_w = std::min(560.f, std::max(320.f, static_cast<float>(width) - 48.f));
    spec.preferred_h = std::min(540.f, std::max(420.f, static_cast<float>(height) - 48.f));
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

void draw_rows(ICanvas& canvas, const ui::MenuStack& stack, const std::vector<std::string>& labels, int selected, const ui::Theme& theme) {
    const int count = std::min(static_cast<int>(stack.rows.size()), static_cast<int>(labels.size()));
    for (int index = 0; index < count; ++index) {
        ui::draw_menu_button(
            canvas, stack.rows[static_cast<std::size_t>(index)], labels[static_cast<std::size_t>(index)], index == selected, theme,
            stack.row_pad_x, stack.row_pad_right);
    }
}

std::string slot_label(const ISaveSlots* saves, int index) {
    if (index == ISaveSlots::kSlotCount) {
        return kBackLabel;
    }
    const int slot = index + 1;
    const bool saved = saves != nullptr && saves->occupied(slot);
    return "Slot " + std::to_string(slot) + (saved ? " saved" : " empty");
}

class MainMenuModule final : public IModule {
public:
    MainMenuModule(ISaveSlots* saves, IPlayPhase* phase, ISettingsScreen** settings, IDeviceSettings* devices)
        : saves_(saves), phase_(phase), settings_(settings), devices_(devices) {}

    std::string_view name() const override { return "frontend.main_menu"; }

    void update(const InputFrame& input) override {
        if (phase_ != nullptr && phase_->phase() != PlayPhase::Frontend) {
            return;
        }
        if (settings_open()) {
            return;
        }
        if (input.window_width <= 0 || input.window_height <= 0) {
            return;
        }
        switch (screen_) {
            case Screen::Main:
                update_main(input);
                break;
            case Screen::Slots:
                update_slots(input);
                break;
            case Screen::ContinueSlots:
                update_continue(input);
                break;
            case Screen::Started:
                update_started(input);
                break;
        }
    }

    void draw(ICanvas& canvas) override {
        if (phase_ != nullptr && phase_->phase() != PlayPhase::Frontend) {
            return;
        }
        switch (screen_) {
            case Screen::Main:
                draw_main(canvas);
                break;
            case Screen::Slots:
                draw_slots(canvas);
                break;
            case Screen::ContinueSlots:
                draw_continue(canvas);
                break;
            case Screen::Started:
                draw_started(canvas);
                break;
        }
    }

    AppCommand poll_command() override {
        const AppCommand command = pending_;
        pending_ = AppCommand::None;
        return command;
    }

private:
    ui::Theme theme() const { return ui::theme_from(devices_); }

    bool settings_open() const {
        return settings_ != nullptr && *settings_ != nullptr && (*settings_)->is_open();
    }

    ui::MenuSpec main_spec(int width, int height, const ui::Theme& colors) const {
        return menu_spec(width, height, kTitle, status_, kHintText, colors);
    }

    ui::MenuStack layout_main(int width, int height, const ui::Theme& colors) const {
        ui::MenuSpec spec = main_spec(width, height, colors);
        std::string_view labels[kEntryCount];
        for (int index = 0; index < kEntryCount; ++index) {
            labels[index] = kEntries[index].label;
        }
        spec.rows = labels;
        return ui::layout_menu(spec);
    }

    ui::MenuSpec counted_spec(int width, int height, std::string_view title, const ui::Theme& colors) const {
        ui::MenuSpec spec = menu_spec(width, height, title, status_, kHintText, colors);
        spec.min_row_h = 40.f;
        spec.row_gap = 8.f;
        spec.title_gap = 31.f;
        return spec;
    }

    ui::MenuStack layout_started(int width, int height, const ui::Theme& colors, const std::string& slot_line) const {
        ui::MenuSpec spec = menu_spec(width, height, kTitle, {}, kExitHint, colors);
        spec.title_gap = 24.f;
        const std::string_view preface[] = {kStartedMessage, slot_line};
        const std::string_view rows[] = {kOkLabel};
        spec.preface = preface;
        spec.rows = rows;
        return ui::layout_menu(spec);
    }

    void update_main(const InputFrame& input) {
        const ui::MenuStack stack = layout_main(input.window_width, input.window_height, theme());
        const int activated = navigate_rows(input, stack.rows.data(), static_cast<int>(stack.rows.size()), index_);
        if (activated >= 0) {
            activate_main(activated);
        }
    }

    void update_slots(const InputFrame& input) {
        std::vector<std::string> labels;
        std::vector<std::string_view> views;
        fill_slot_labels(labels, views);
        ui::MenuSpec spec = counted_spec(input.window_width, input.window_height, kSlotTitle, theme());
        spec.rows = views;
        const ui::MenuStack stack = ui::layout_menu(spec);
        const int activated = navigate_rows(input, stack.rows.data(), static_cast<int>(stack.rows.size()), index_);
        if (activated >= 0) {
            activate_slot(activated);
        }
    }

    void update_continue(const InputFrame& input) {
        const int count = continue_count();
        if (index_ >= count) {
            index_ = 0;
        }
        std::vector<std::string> labels;
        std::vector<std::string_view> views;
        fill_continue_labels(labels, views);
        ui::MenuSpec spec = counted_spec(input.window_width, input.window_height, kContinueTitle, theme());
        spec.rows = views;
        const ui::MenuStack stack = ui::layout_menu(spec);
        const int activated = navigate_rows(input, stack.rows.data(), static_cast<int>(stack.rows.size()), index_);
        if (activated >= 0) {
            activate_continue(activated);
        }
    }

    void update_started(const InputFrame& input) {
        const std::string slot_line = "Slot " + std::to_string(started_slot_);
        const ui::MenuStack stack = layout_started(input.window_width, input.window_height, theme(), slot_line);
        const int activated = navigate_rows(input, stack.rows.data(), static_cast<int>(stack.rows.size()), index_);
        if (activated == 0) {
            pending_ = AppCommand::Quit;
        }
    }

    void draw_main(ICanvas& canvas) const {
        const ui::Theme colors = theme();
        const ui::MenuSpec spec = main_spec(canvas.width(), canvas.height(), colors);
        std::string_view labels[kEntryCount];
        for (int index = 0; index < kEntryCount; ++index) {
            labels[index] = kEntries[index].label;
        }
        ui::MenuSpec laid = spec;
        laid.rows = labels;
        const ui::MenuStack stack = ui::layout_menu(laid);
        ui::draw_menu_panel(canvas, stack, colors, true);
        for (int entry = 0; entry < kEntryCount && entry < static_cast<int>(stack.rows.size()); ++entry) {
            ui::draw_menu_button(
                canvas, stack.rows[static_cast<std::size_t>(entry)], labels[entry], entry == index_, colors, stack.row_pad_x,
                stack.row_pad_right);
        }
        ui::draw_menu_text(canvas, stack, laid, colors);
    }

    void draw_slots(ICanvas& canvas) const {
        const ui::Theme colors = theme();
        std::vector<std::string> labels;
        std::vector<std::string_view> views;
        fill_slot_labels(labels, views);
        ui::MenuSpec spec = counted_spec(canvas.width(), canvas.height(), kSlotTitle, colors);
        spec.rows = views;
        const ui::MenuStack stack = ui::layout_menu(spec);
        ui::draw_menu_panel(canvas, stack, colors, true);
        draw_rows(canvas, stack, labels, index_, colors);
        ui::draw_menu_text(canvas, stack, spec, colors);
    }

    void draw_continue(ICanvas& canvas) const {
        const ui::Theme colors = theme();
        std::vector<std::string> labels;
        std::vector<std::string_view> views;
        fill_continue_labels(labels, views);
        ui::MenuSpec spec = counted_spec(canvas.width(), canvas.height(), kContinueTitle, colors);
        spec.rows = views;
        const ui::MenuStack stack = ui::layout_menu(spec);
        ui::draw_menu_panel(canvas, stack, colors, true);
        draw_rows(canvas, stack, labels, index_, colors);
        ui::draw_menu_text(canvas, stack, spec, colors);
    }

    void draw_started(ICanvas& canvas) const {
        const ui::Theme colors = theme();
        const std::string slot_line = "Slot " + std::to_string(started_slot_);
        const std::string_view preface[] = {kStartedMessage, slot_line};
        const std::string_view rows[] = {kOkLabel};
        ui::MenuSpec spec = menu_spec(canvas.width(), canvas.height(), kTitle, {}, kExitHint, colors);
        spec.title_gap = 24.f;
        spec.preface = preface;
        spec.rows = rows;
        const ui::MenuStack stack = ui::layout_menu(spec);
        ui::draw_menu_panel(canvas, stack, colors, true);
        if (!stack.rows.empty()) {
            ui::draw_menu_button(canvas, stack.rows[0], kOkLabel, true, colors, stack.row_pad_x, stack.row_pad_right);
        }
        ui::draw_menu_text(canvas, stack, spec, colors);
    }

    void activate_main(int index) {
        const Entry& entry = kEntries[index];
        if (entry.command == AppCommand::StartNewGame && saves_ != nullptr) {
            screen_ = Screen::Slots;
            index_ = 0;
            status_.clear();
            return;
        }
        if (entry.command == AppCommand::Continue && saves_ != nullptr && phase_ != nullptr) {
            open_continue();
            return;
        }
        if (entry.command == AppCommand::OpenOptions && settings_ != nullptr && *settings_ != nullptr) {
            (*settings_)->open();
            return;
        }
        pending_ = entry.command;
        if (pending_ != AppCommand::Quit) {
            status_ = std::string(entry.label) + " has no handler yet.";
        }
    }

    void open_continue() {
        continue_slots_.clear();
        for (int slot = 1; slot <= ISaveSlots::kSlotCount; ++slot) {
            if (saves_->occupied(slot)) {
                continue_slots_.push_back(slot);
            }
        }
        if (continue_slots_.empty()) {
            status_ = kNoSaves;
            return;
        }
        screen_ = Screen::ContinueSlots;
        index_ = 0;
        status_.clear();
    }

    void activate_slot(int index) {
        if (index == ISaveSlots::kSlotCount) {
            screen_ = Screen::Main;
            index_ = 0;
            status_.clear();
            return;
        }
        const int slot = index + 1;
        if (saves_ == nullptr || !saves_->start_new_game(slot)) {
            status_ = kWriteFailed;
            return;
        }
        started_slot_ = slot;
        index_ = 0;
        status_.clear();
        if (phase_ != nullptr) {
            screen_ = Screen::Main;
            phase_->begin_intro(slot);
            return;
        }
        screen_ = Screen::Started;
    }

    void activate_continue(int index) {
        if (index == static_cast<int>(continue_slots_.size())) {
            screen_ = Screen::Main;
            index_ = 0;
            status_.clear();
            return;
        }
        const int slot = continue_slots_[static_cast<std::size_t>(index)];
        int millis = 500;
        if (saves_ == nullptr || !saves_->load_progress(slot, millis)) {
            status_ = kReadFailed;
            return;
        }
        screen_ = Screen::Main;
        index_ = 0;
        status_.clear();
        phase_->continue_game(slot, static_cast<float>(millis) / 1000.f);
    }

    int continue_count() const { return static_cast<int>(continue_slots_.size()) + 1; }

    std::string continue_label(int index) const {
        if (index == static_cast<int>(continue_slots_.size())) {
            return kBackLabel;
        }
        return "Slot " + std::to_string(continue_slots_[static_cast<std::size_t>(index)]);
    }

    void fill_slot_labels(std::vector<std::string>& labels, std::vector<std::string_view>& views) const {
        labels.clear();
        views.clear();
        labels.reserve(static_cast<std::size_t>(kSlotRowCount));
        for (int index = 0; index < kSlotRowCount; ++index) {
            labels.push_back(slot_label(saves_, index));
        }
        views.reserve(labels.size());
        for (const std::string& label : labels) {
            views.emplace_back(label);
        }
    }

    void fill_continue_labels(std::vector<std::string>& labels, std::vector<std::string_view>& views) const {
        labels.clear();
        views.clear();
        const int count = continue_count();
        labels.reserve(static_cast<std::size_t>(count));
        for (int index = 0; index < count; ++index) {
            labels.push_back(continue_label(index));
        }
        views.reserve(labels.size());
        for (const std::string& label : labels) {
            views.emplace_back(label);
        }
    }

    ISaveSlots* saves_ = nullptr;
    IPlayPhase* phase_ = nullptr;
    ISettingsScreen** settings_ = nullptr;
    IDeviceSettings* devices_ = nullptr;
    std::vector<int> continue_slots_;
    Screen screen_ = Screen::Main;
    int index_ = 0;
    int started_slot_ = 0;
    AppCommand pending_ = AppCommand::None;
    std::string status_;
};

}  // namespace

void register_frontend(
    ModuleRegistry& registry, ISaveSlots* saves, IPlayPhase* phase, ISettingsScreen** settings, IDeviceSettings* devices) {
    registry.add<MainMenuModule>(saves, phase, settings, devices);
}

}  // namespace lla
