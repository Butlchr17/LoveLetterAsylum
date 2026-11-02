#include "lla/settings/register.hpp"

#include "lla/settings/logic.hpp"
#include "lla/ui/menu_layout.hpp"
#include "lla/ui/text.hpp"
#include "lla/ui/theme.hpp"

#include <algorithm>
#include <cmath>
#include <string>
#include <string_view>
#include <vector>

namespace lla {
namespace {

enum class Page {
    Home,
    Display,
    Audio,
    Controls,
    Access,
    ResetAsk,
    DisplayAsk,
    Keep,
};

std::string key_name(int code) {
    switch (code) {
        case 9:
            return "Esc";
        case 22:
            return "Backspace";
        case 23:
            return "Tab";
        case 36:
            return "Enter";
        case 37:
            return "Ctrl";
        case 50:
            return "Shift";
        case 62:
            return "RShift";
        case 64:
            return "Alt";
        case 65:
            return "Space";
        case 66:
            return "Caps";
        case 104:
            return "KpEnter";
        case 105:
            return "RCtrl";
        case 108:
            return "RAlt";
        case 110:
            return "Home";
        case 111:
            return "Up";
        case 112:
            return "PageUp";
        case 113:
            return "Left";
        case 114:
            return "Right";
        case 115:
            return "End";
        case 116:
            return "Down";
        case 117:
            return "PageDown";
        case 119:
            return "Delete";
        default:
            break;
    }
    if (code >= 10 && code <= 18) {
        return std::string(1, static_cast<char>('1' + (code - 10)));
    }
    if (code == 19) {
        return "0";
    }
    if (code >= 67 && code <= 76) {
        return "F" + std::to_string(code - 66);
    }
    if (code == 95) {
        return "F11";
    }
    if (code == 96) {
        return "F12";
    }
    constexpr char kTop[] = "QWERTYUIOP";
    constexpr char kHome[] = "ASDFGHJKL";
    constexpr char kBottom[] = "ZXCVBNM";
    if (code >= 24 && code <= 33) {
        return std::string(1, kTop[code - 24]);
    }
    if (code >= 38 && code <= 46) {
        return std::string(1, kHome[code - 38]);
    }
    if (code >= 52 && code <= 58) {
        return std::string(1, kBottom[code - 52]);
    }
    return std::to_string(code);
}

const DeviceSettings& current_settings(const IDeviceSettings* devices) {
    static const DeviceSettings kFallback{};
    return devices != nullptr ? devices->get() : kFallback;
}

constexpr const char* kWideHint = "Up/Down select    Enter or click confirm";

ui::MenuSpec settings_spec(int width, int height, std::string_view title, std::string_view status, const ui::Theme& theme) {
    ui::MenuSpec spec;
    spec.window_w = width;
    spec.window_h = height;
    spec.text_scale = theme.text_scale;
    spec.title_scale = theme.title_scale;
    spec.title = title;
    spec.status = status;
    // Size from the wider hint so the panel does not jump between rows.
    spec.hint = kWideHint;
    spec.preferred_w = std::min(760.f, std::max(560.f, static_cast<float>(width) - 48.f));
    spec.preferred_h = std::min(680.f, std::max(520.f, static_cast<float>(height) - 32.f));
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
    return spec;
}

int navigate_rows(const InputFrame& input, const std::vector<Rect>& rows, int& index) {
    const int count = static_cast<int>(rows.size());
    if (input.mouse_moved) {
        for (int row = 0; row < count; ++row) {
            if (rows[static_cast<std::size_t>(row)].contains(input.mouse_x, input.mouse_y)) {
                index = row;
                break;
            }
        }
    }
    if (input.mouse_pressed) {
        for (int row = 0; row < count; ++row) {
            if (rows[static_cast<std::size_t>(row)].contains(input.mouse_x, input.mouse_y)) {
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



class SettingsModule final : public IModule, public ISettingsScreen {
public:
    explicit SettingsModule(IDeviceSettings* devices) : devices_(devices) {}

    std::string_view name() const override { return "settings.menu"; }

    bool is_open() const override { return open_; }

    void open() override {
        open_ = true;
        ignore_input_ = true;
        capturing_ = false;
        page_ = Page::Home;
        index_ = 0;
        status_.clear();
        prompt_ = {};
        draft_ = display_choice(current_settings(devices_));
        reset_hold();
    }

    void close() override {
        if (prompt_.open) {
            restore_committed_display();
            prompt_.open = false;
        }
        open_ = false;
        capturing_ = false;
        page_ = Page::Home;
        reset_hold();
    }

    void update(const InputFrame& input) override {
        if (!open_) {
            return;
        }
        if (ignore_input_) {
            ignore_input_ = false;
            return;
        }
        if (input.window_width <= 0 || input.window_height <= 0) {
            return;
        }
        if (capturing_) {
            update_capture(input);
            return;
        }

        const int count = row_count();
        if (index_ < 0 || index_ >= count) {
            index_ = 0;
        }
        const int previous = index_;
        std::vector<std::string> labels;
        std::vector<std::string_view> views;
        collect_rows(labels, views);
        const std::string status = status_line();
        ui::MenuSpec spec = settings_spec(input.window_width, input.window_height, title(), status, theme());
        spec.rows = views;
        const ui::MenuStack layout = ui::layout_menu(spec);
        const int activated = navigate_rows(input, layout.rows, index_);
        const bool moved_row = index_ != previous;
        if (moved_row) {
            reset_hold();
        }
        // Escape still leaves a page when Pause is bound to another key.
        const bool back = input.key_code == kMenuBackKey || input.pause;
        if (back) {
            reset_hold();
            go_back();
            return;
        }
        // Bound move keys change the highlighted value. Up or Down wins on the same frame.
        if (!moved_row && handle_axis(input)) {
            return;
        }
        if (activated >= 0) {
            activate(activated);
            return;
        }
        if (page_ == Page::Keep && tick_display_trial(prompt_, input.dt)) {
            revert_trial();
        }
    }

    void draw(ICanvas& canvas) override {
        if (!open_ || canvas.width() <= 0 || canvas.height() <= 0) {
            return;
        }
        const ui::Theme theme_now = theme();
        const int count = row_count();
        std::vector<std::string> labels;
        std::vector<std::string_view> views;
        collect_rows(labels, views);
        const std::string shown = status_line();
        ui::MenuSpec spec = settings_spec(canvas.width(), canvas.height(), title(), shown, theme_now);
        spec.rows = views;
        const ui::MenuStack layout = ui::layout_menu(spec);
        const char* hint = row_changes(index_) ? "Up/Down select    Left/Right change" : kWideHint;
        spec.hint = hint;
        ui::draw_menu_panel(canvas, layout, theme_now, false);
        for (int index = 0; index < count && index < static_cast<int>(layout.rows.size()); ++index) {
            ui::draw_menu_button(
                canvas, layout.rows[static_cast<std::size_t>(index)], labels[static_cast<std::size_t>(index)], index == index_, theme_now,
                layout.row_pad_x, layout.row_pad_right);
        }
        ui::draw_menu_text(canvas, layout, spec, theme_now);
    }

private:
    ui::Theme theme() const { return ui::theme_from(devices_); }

    const char* title() const {
        switch (page_) {
            case Page::Home:
                return "Options";
            case Page::Display:
            case Page::DisplayAsk:
                return "Display";
            case Page::Audio:
                return "Audio";
            case Page::Controls:
                return "Controls";
            case Page::Access:
                return "Access";
            case Page::ResetAsk:
                return "Reset";
            case Page::Keep:
                return "Keep this display";
        }
        return "Options";
    }

    int row_count() const {
        switch (page_) {
            case Page::Home:
                return 6;
            case Page::Display:
                return 4;
            case Page::Audio:
                return 6;
            case Page::Controls:
                return kBindCount + 2;
            case Page::Access:
                return 3;
            case Page::ResetAsk:
            case Page::DisplayAsk:
            case Page::Keep:
                return 2;
        }
        return 0;
    }

    void collect_rows(std::vector<std::string>& labels, std::vector<std::string_view>& views) const {
        labels.clear();
        views.clear();
        const int count = row_count();
        labels.reserve(static_cast<std::size_t>(count));
        for (int index = 0; index < count; ++index) {
            labels.push_back(row_label(index));
        }
        views.reserve(labels.size());
        for (const std::string& label : labels) {
            views.emplace_back(label);
        }
    }

    std::string row_label(int index) const {
        const DeviceSettings& settings = current_settings(devices_);
        switch (page_) {
            case Page::Home:
                switch (index) {
                    case 0:
                        return "Display";
                    case 1:
                        return "Audio";
                    case 2:
                        return "Controls";
                    case 3:
                        return "Access";
                    case 4:
                        return "Reset";
                    default:
                        return "Back";
                }
            case Page::Display:
                switch (index) {
                    case 0:
                        return draft_.fullscreen ? "Mode Fullscreen" : "Mode Windowed";
                    case 1:
                        return "Size " + std::to_string(draft_.width) + "x" + std::to_string(draft_.height);
                    case 2:
                        return "Apply";
                    default:
                        return "Back";
                }
            case Page::Audio:
                switch (index) {
                    case 0:
                        return "Master " + std::to_string(settings.master);
                    case 1:
                        return "Music " + std::to_string(settings.music);
                    case 2:
                        return "Effects " + std::to_string(settings.effects);
                    case 3:
                        return "Menu " + std::to_string(settings.menu_volume);
                    case 4:
                        return settings.mute ? "Mute On" : "Mute Off";
                    default:
                        return "Back";
                }
            case Page::Controls:
                if (index >= 0 && index < kBindCount) {
                    if (capturing_ && index == capture_index_) {
                        return "Press a key";
                    }
                    constexpr const char* kNames[] = {
                        "Move left", "Move left 2", "Move right", "Move right 2", "Menu up", "Menu up 2",
                        "Menu down", "Menu down 2", "Confirm", "Confirm 2", "Pause",
                    };
                    static_assert(sizeof(kNames) / sizeof(kNames[0]) == static_cast<std::size_t>(kBindCount));
                    return std::string(kNames[index]) + " " + key_name(bind_value(settings.binds, index));
                }
                if (index == kBindCount) {
                    return "Restore defaults";
                }
                return "Back";
            case Page::Access:
                switch (index) {
                    case 0:
                        return std::string("Text size ") + text_size_name(settings.text_scale);
                    case 1:
                        return settings.high_contrast ? "High contrast On" : "High contrast Off";
                    default:
                        return "Back";
                }
            case Page::ResetAsk:
                return index == 0 ? "Reset" : "Back";
            case Page::DisplayAsk:
                return index == 0 ? "Apply" : "Discard";
            case Page::Keep:
                return index == 0 ? "Keep" : "Revert";
        }
        return {};
    }

    std::string status_line() const {
        if (page_ == Page::Keep && prompt_.open) {
            int seconds = static_cast<int>(std::ceil(prompt_.remaining));
            if (seconds < 0) {
                seconds = 0;
            }
            return std::to_string(seconds) + " left";
        }
        return status_;
    }

    void show(Page page) {
        page_ = page;
        index_ = 0;
        capturing_ = false;
        status_.clear();
        reset_hold();
    }

    void reset_hold() {
        hold_dir_ = 0;
        hold_time_ = 0.f;
        hold_repeating_ = false;
    }

    bool row_changes(int index) const {
        switch (page_) {
            case Page::Display:
                return index == 0 || index == 1;
            case Page::Audio:
                return index >= 0 && index <= 4;
            case Page::Access:
                return index == 0 || index == 1;
            case Page::Home:
            case Page::Controls:
            case Page::ResetAsk:
            case Page::DisplayAsk:
            case Page::Keep:
                return false;
        }
        return false;
    }

    // Returns true when a bound move key changed, or is held on, the highlighted row.
    bool handle_axis(const InputFrame& input) {
        if (!row_changes(index_)) {
            reset_hold();
            return false;
        }
        int tapped = 0;
        if (input.left_pressed && !input.right_pressed) {
            tapped = -1;
        } else if (input.right_pressed && !input.left_pressed) {
            tapped = 1;
        }
        int held = 0;
        if (input.left && !input.right) {
            held = -1;
        } else if (input.right && !input.left) {
            held = 1;
        }

        if (tapped != 0) {
            adjust(index_, tapped);
            hold_dir_ = tapped;
            hold_time_ = 0.f;
            hold_repeating_ = false;
            return true;
        }
        if (held == 0) {
            reset_hold();
            return false;
        }
        if (held != hold_dir_) {
            adjust(index_, held);
            hold_dir_ = held;
            hold_time_ = 0.f;
            hold_repeating_ = false;
            return true;
        }
        hold_time_ += input.dt > 0.f ? input.dt : 0.f;
        const float delay = hold_repeating_ ? 0.12f : 0.4f;
        if (hold_time_ >= delay) {
            adjust(index_, held);
            hold_time_ = 0.f;
            hold_repeating_ = true;
        }
        return true;
    }

    bool display_dirty() const {
        return !(draft_ == display_choice(current_settings(devices_)));
    }

    void go_back() {
        switch (page_) {
            case Page::Home:
                close();
                break;
            case Page::Display:
                if (display_dirty()) {
                    show(Page::DisplayAsk);
                } else {
                    show(Page::Home);
                }
                break;
            case Page::DisplayAsk:
                show(Page::Display);
                break;
            case Page::Keep:
                revert_trial();
                break;
            case Page::Audio:
            case Page::Controls:
            case Page::Access:
            case Page::ResetAsk:
                show(Page::Home);
                break;
        }
    }

    void activate(int index) {
        switch (page_) {
            case Page::Home:
                if (index == 0) {
                    draft_ = display_choice(current_settings(devices_));
                    show(Page::Display);
                } else if (index == 1) {
                    show(Page::Audio);
                } else if (index == 2) {
                    show(Page::Controls);
                } else if (index == 3) {
                    show(Page::Access);
                } else if (index == 4) {
                    show(Page::ResetAsk);
                } else {
                    close();
                }
                break;
            case Page::Display:
                if (index == 2) {
                    start_trial();
                } else if (index == 3) {
                    go_back();
                } else {
                    adjust(index, 0);
                }
                break;
            case Page::Audio:
                if (index == 5) {
                    show(Page::Home);
                } else {
                    adjust(index, 0);
                }
                break;
            case Page::Controls:
                if (index >= 0 && index < kBindCount) {
                    capturing_ = true;
                    capture_index_ = index;
                    status_.clear();
                } else if (index == kBindCount) {
                    restore_binds();
                } else {
                    show(Page::Home);
                }
                break;
            case Page::Access:
                if (index == 2) {
                    show(Page::Home);
                } else {
                    adjust(index, 0);
                }
                break;
            case Page::ResetAsk:
                if (index == 0) {
                    reset_all();
                } else {
                    show(Page::Home);
                }
                break;
            case Page::DisplayAsk:
                if (index == 0) {
                    start_trial();
                } else {
                    draft_ = display_choice(current_settings(devices_));
                    show(Page::Home);
                }
                break;
            case Page::Keep:
                if (index == 0) {
                    accept_trial();
                } else {
                    revert_trial();
                }
                break;
        }
    }

    void adjust(int index, int direction) {
        switch (page_) {
            case Page::Display:
                if (index == 0) {
                    if (direction < 0) {
                        draft_.fullscreen = false;
                    } else if (direction > 0) {
                        draft_.fullscreen = true;
                    } else {
                        draft_.fullscreen = !draft_.fullscreen;
                    }
                } else if (index == 1) {
                    cycle_resolution(direction);
                }
                break;
            case Page::Audio:
                if (index >= 0 && index <= 3) {
                    edit_settings([&](DeviceSettings& next) {
                        int* volumes[] = {&next.master, &next.music, &next.effects, &next.menu_volume};
                        int& volume = *volumes[index];
                        if (direction == 0) {
                            volume += 5;
                            if (volume > 100) {
                                volume = 0;
                            }
                        } else {
                            volume = step_volume(volume, direction);
                        }
                    });
                } else if (index == 4) {
                    edit_settings([direction](DeviceSettings& next) {
                        if (direction < 0) {
                            next.mute = false;
                        } else if (direction > 0) {
                            next.mute = true;
                        } else {
                            next.mute = !next.mute;
                        }
                    });
                }
                break;
            case Page::Access:
                if (index == 0) {
                    edit_settings([&](DeviceSettings& next) {
                        if (direction == 0) {
                            if (next.text_scale <= 2) {
                                next.text_scale = 3;
                            } else if (next.text_scale == 3) {
                                next.text_scale = 4;
                            } else {
                                next.text_scale = 2;
                            }
                        } else {
                            next.text_scale = step_text_scale(next.text_scale, direction);
                        }
                    });
                } else if (index == 1) {
                    edit_settings([direction](DeviceSettings& next) {
                        if (direction < 0) {
                            next.high_contrast = false;
                        } else if (direction > 0) {
                            next.high_contrast = true;
                        } else {
                            next.high_contrast = !next.high_contrast;
                        }
                    });
                }
                break;
            case Page::Home:
            case Page::Controls:
            case Page::ResetAsk:
            case Page::DisplayAsk:
            case Page::Keep:
                break;
        }
    }

    void cycle_resolution(int direction) {
        std::vector<Resolution> choices;
        const int screen_w = devices_ != nullptr ? devices_->screen_width() : 1920;
        const int screen_h = devices_ != nullptr ? devices_->screen_height() : 1080;
        list_resolutions(screen_w, screen_h, choices);
        bool listed = false;
        for (const Resolution& choice : choices) {
            if (choice.width == draft_.width && choice.height == draft_.height) {
                listed = true;
            }
        }
        if (!listed) {
            choices.insert(choices.begin(), Resolution{draft_.width, draft_.height});
        }
        if (choices.empty()) {
            return;
        }
        int found = 0;
        for (int index = 0; index < static_cast<int>(choices.size()); ++index) {
            if (choices[static_cast<std::size_t>(index)].width == draft_.width &&
                choices[static_cast<std::size_t>(index)].height == draft_.height) {
                found = index;
            }
        }
        const int step = direction == 0 ? 1 : direction;
        int next = found + step;
        const int count = static_cast<int>(choices.size());
        if (next < 0) {
            next = count - 1;
        } else if (next >= count) {
            next = 0;
        }
        draft_.width = choices[static_cast<std::size_t>(next)].width;
        draft_.height = choices[static_cast<std::size_t>(next)].height;
    }

    void start_trial() {
        if (devices_ == nullptr || !display_dirty()) {
            return;
        }
        const DisplayChoice committed = display_choice(devices_->get());
        begin_display_trial(prompt_, draft_, committed);
        devices_->preview_display(draft_.fullscreen, draft_.width, draft_.height);
        show(Page::Keep);
    }

    void accept_trial() {
        if (devices_ != nullptr) {
            devices_->commit_display(prompt_.trial.fullscreen, prompt_.trial.width, prompt_.trial.height);
            draft_ = display_choice(devices_->get());
        }
        prompt_.open = false;
        show(Page::Display);
    }

    void restore_committed_display() {
        if (devices_ != nullptr) {
            devices_->preview_display(prompt_.committed.fullscreen, prompt_.committed.width, prompt_.committed.height);
        }
        draft_ = prompt_.committed;
    }

    void revert_trial() {
        restore_committed_display();
        prompt_.open = false;
        show(Page::Display);
    }

    void reset_all() {
        if (prompt_.open) {
            prompt_.open = false;
        }
        if (devices_ != nullptr) {
            devices_->replace(DeviceSettings{});
            draft_ = display_choice(devices_->get());
        } else {
            draft_ = {};
        }
        show(Page::Home);
    }

    void restore_binds() {
        edit_settings([](DeviceSettings& next) { next.binds = KeyBinds{}; });
    }

    template <typename Edit>
    void edit_settings(Edit edit) {
        if (devices_ == nullptr) {
            return;
        }
        DeviceSettings next = devices_->get();
        edit(next);
        devices_->replace(next);
    }

    void update_capture(const InputFrame& input) {
        if (input.mouse_pressed) {
            capturing_ = false;
            return;
        }
        if (input.key_code == 0) {
            return;
        }
        if (input.key_code == kMenuBackKey) {
            capturing_ = false;
            return;
        }
        if (!key_code_allowed(input.key_code) || devices_ == nullptr) {
            return;
        }
        DeviceSettings next = devices_->get();
        if (bind_in_use(next.binds, input.key_code, capture_index_)) {
            status_ = "Key in use";
            capturing_ = false;
            return;
        }
        set_bind_value(next.binds, capture_index_, input.key_code);
        devices_->replace(next);
        capturing_ = false;
        status_.clear();
    }

    IDeviceSettings* devices_ = nullptr;
    bool open_ = false;
    bool ignore_input_ = false;
    bool capturing_ = false;
    Page page_ = Page::Home;
    int index_ = 0;
    int capture_index_ = 0;
    DisplayChoice draft_{};
    DisplayPrompt prompt_{};
    std::string status_;
    int hold_dir_ = 0;
    float hold_time_ = 0.f;
    bool hold_repeating_ = false;
};

}  // namespace

void register_settings(ModuleRegistry& registry, IDeviceSettings* devices, IPlayPhase*, ISettingsScreen*& screen) {
    screen = &registry.add<SettingsModule>(devices);
}

}  // namespace lla
