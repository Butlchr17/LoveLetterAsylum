#include "lla/settings/device_settings.hpp"

#include "lla/settings/logic.hpp"

#include <charconv>
#include <fstream>
#include <string>
#include <string_view>
#include <system_error>

namespace lla {
namespace {

std::string document(const DeviceSettings& settings) {
    const auto flag = [](bool value) { return value ? "true" : "false"; };
    return std::string("{\n") + "  \"fullscreen\": " + flag(settings.fullscreen) + ",\n" +
           "  \"width\": " + std::to_string(settings.width) + ",\n" +
           "  \"height\": " + std::to_string(settings.height) + ",\n" +
           "  \"mute\": " + flag(settings.mute) + ",\n" +
           "  \"master\": " + std::to_string(settings.master) + ",\n" +
           "  \"music\": " + std::to_string(settings.music) + ",\n" +
           "  \"effects\": " + std::to_string(settings.effects) + ",\n" +
           "  \"menu\": " + std::to_string(settings.menu_volume) + ",\n" +
           "  \"text\": " + std::to_string(settings.text_scale) + ",\n" +
           "  \"contrast\": " + flag(settings.high_contrast) + ",\n" +
           "  \"left\": " + std::to_string(settings.binds.move_left) + ",\n" +
           "  \"left2\": " + std::to_string(settings.binds.move_left_alt) + ",\n" +
           "  \"right\": " + std::to_string(settings.binds.move_right) + ",\n" +
           "  \"right2\": " + std::to_string(settings.binds.move_right_alt) + ",\n" +
           "  \"up\": " + std::to_string(settings.binds.menu_up) + ",\n" +
           "  \"up2\": " + std::to_string(settings.binds.menu_up_alt) + ",\n" +
           "  \"down\": " + std::to_string(settings.binds.menu_down) + ",\n" +
           "  \"down2\": " + std::to_string(settings.binds.menu_down_alt) + ",\n" +
           "  \"confirm\": " + std::to_string(settings.binds.confirm) + ",\n" +
           "  \"confirm2\": " + std::to_string(settings.binds.confirm_alt) + ",\n" +
           "  \"pause\": " + std::to_string(settings.binds.pause) + "\n" + "}\n";
}

std::size_t find_key(std::string_view text, std::string_view key, std::size_t from) {
    const std::string token = std::string("\"") + std::string(key) + "\"";
    return text.find(token, from);
}

bool value_at(std::string_view text, std::size_t key_at, std::string_view& value) {
    const std::size_t colon = text.find(':', key_at);
    if (colon == std::string_view::npos) {
        return false;
    }
    std::size_t start = colon + 1;
    while (start < text.size() && (text[start] == ' ' || text[start] == '\n' || text[start] == '\r' || text[start] == '\t')) {
        ++start;
    }
    std::size_t end = start;
    while (end < text.size() && text[end] != ',' && text[end] != '}' && text[end] != '\n' && text[end] != '\r') {
        ++end;
    }
    value = text.substr(start, end - start);
    return start < end;
}

void read_bool(std::string_view text, std::string_view key, bool& out) {
    std::size_t from = 0;
    while (true) {
        const std::size_t at = find_key(text, key, from);
        if (at == std::string_view::npos) {
            return;
        }
        std::string_view value;
        if (value_at(text, at, value)) {
            if (value == "true") {
                out = true;
            } else if (value == "false") {
                out = false;
            }
        }
        from = at + key.size() + 2;
    }
}

void read_int(std::string_view text, std::string_view key, int& out) {
    std::size_t from = 0;
    while (true) {
        const std::size_t at = find_key(text, key, from);
        if (at == std::string_view::npos) {
            return;
        }
        std::string_view value;
        if (value_at(text, at, value)) {
            int parsed = 0;
            const auto result = std::from_chars(value.data(), value.data() + value.size(), parsed);
            if (result.ec == std::errc{} && result.ptr == value.data() + value.size()) {
                out = parsed;
            }
        }
        from = at + key.size() + 2;
    }
}

int clamp_int(int value, int low, int high) {
    if (value < low) {
        return low;
    }
    if (value > high) {
        return high;
    }
    return value;
}

int clamp_key(int code, int fallback) {
    if (!key_code_allowed(code)) {
        return fallback;
    }
    return code;
}

DeviceSettings sanitized(DeviceSettings settings) {
    const KeyBinds defaults;
    settings.width = clamp_int(settings.width, 320, 7680);
    settings.height = clamp_int(settings.height, 240, 4320);
    settings.master = clamp_int(settings.master, 0, 100);
    settings.music = clamp_int(settings.music, 0, 100);
    settings.effects = clamp_int(settings.effects, 0, 100);
    settings.menu_volume = clamp_int(settings.menu_volume, 0, 100);
    settings.text_scale = clamp_int(settings.text_scale, 2, 4);
    settings.binds.move_left = clamp_key(settings.binds.move_left, defaults.move_left);
    settings.binds.move_left_alt = clamp_key(settings.binds.move_left_alt, defaults.move_left_alt);
    settings.binds.move_right = clamp_key(settings.binds.move_right, defaults.move_right);
    settings.binds.move_right_alt = clamp_key(settings.binds.move_right_alt, defaults.move_right_alt);
    settings.binds.menu_up = clamp_key(settings.binds.menu_up, defaults.menu_up);
    settings.binds.menu_up_alt = clamp_key(settings.binds.menu_up_alt, defaults.menu_up_alt);
    settings.binds.menu_down = clamp_key(settings.binds.menu_down, defaults.menu_down);
    settings.binds.menu_down_alt = clamp_key(settings.binds.menu_down_alt, defaults.menu_down_alt);
    settings.binds.confirm = clamp_key(settings.binds.confirm, defaults.confirm);
    settings.binds.confirm_alt = clamp_key(settings.binds.confirm_alt, defaults.confirm_alt);
    settings.binds.pause = clamp_key(settings.binds.pause, defaults.pause);
    return settings;
}

}  // namespace

JsonDeviceSettings::JsonDeviceSettings(std::filesystem::path file) : file_(std::move(file)) {
    std::ifstream input(file_, std::ios::binary);
    if (!input) {
        return;
    }
    const std::string text((std::istreambuf_iterator<char>(input)), std::istreambuf_iterator<char>());
    read_bool(text, "fullscreen", current_.fullscreen);
    read_int(text, "width", current_.width);
    read_int(text, "height", current_.height);
    read_bool(text, "mute", current_.mute);
    read_int(text, "master", current_.master);
    read_int(text, "music", current_.music);
    read_int(text, "effects", current_.effects);
    read_int(text, "menu", current_.menu_volume);
    read_int(text, "text", current_.text_scale);
    read_bool(text, "contrast", current_.high_contrast);
    read_int(text, "left", current_.binds.move_left);
    read_int(text, "left2", current_.binds.move_left_alt);
    read_int(text, "right", current_.binds.move_right);
    read_int(text, "right2", current_.binds.move_right_alt);
    read_int(text, "up", current_.binds.menu_up);
    read_int(text, "up2", current_.binds.menu_up_alt);
    read_int(text, "down", current_.binds.menu_down);
    read_int(text, "down2", current_.binds.menu_down_alt);
    read_int(text, "confirm", current_.binds.confirm);
    read_int(text, "confirm2", current_.binds.confirm_alt);
    read_int(text, "pause", current_.binds.pause);
    current_ = sanitized(current_);
}

const DeviceSettings& JsonDeviceSettings::get() const { return current_; }

void JsonDeviceSettings::replace(const DeviceSettings& next) {
    const DisplayChoice before = display_choice(current_);
    current_ = sanitized(next);
    save();
    if (!(display_choice(current_) == before)) {
        apply_current_display();
    }
}

void JsonDeviceSettings::preview_display(bool fullscreen, int width, int height) {
    if (applier_ != nullptr) {
        applier_->apply_display(fullscreen, width, height);
    }
}

void JsonDeviceSettings::commit_display(bool fullscreen, int width, int height) {
    current_.fullscreen = fullscreen;
    current_.width = width;
    current_.height = height;
    current_ = sanitized(current_);
    save();
    apply_current_display();
}

void JsonDeviceSettings::bind_display(IDisplayApplier* applier) { applier_ = applier; }

int JsonDeviceSettings::screen_width() const {
    return applier_ != nullptr ? applier_->screen_width() : 1920;
}

int JsonDeviceSettings::screen_height() const {
    return applier_ != nullptr ? applier_->screen_height() : 1080;
}

bool JsonDeviceSettings::save() const {
    std::error_code error;
    if (file_.has_parent_path()) {
        std::filesystem::create_directories(file_.parent_path(), error);
    }
    const std::filesystem::path temporary(file_.string() + ".tmp");
    {
        std::ofstream output(temporary, std::ios::binary | std::ios::trunc);
        if (!output) {
            return false;
        }
        const std::string text = document(current_);
        output.write(text.data(), static_cast<std::streamsize>(text.size()));
        output.flush();
        if (!output) {
            std::filesystem::remove(temporary, error);
            return false;
        }
    }
    std::filesystem::rename(temporary, file_, error);
    if (error) {
        std::filesystem::remove(temporary, error);
        return false;
    }
    return true;
}

void JsonDeviceSettings::apply_current_display() const {
    if (applier_ != nullptr) {
        applier_->apply_display(current_.fullscreen, current_.width, current_.height);
    }
}

}  // namespace lla
