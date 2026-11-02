#include "lla/settings/device_settings.hpp"
#include "lla/settings/logic.hpp"

#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>
#include <unistd.h>
#include <vector>

namespace {

namespace fs = std::filesystem;

struct TempDir {
    fs::path path;

    explicit TempDir(fs::path directory) : path(std::move(directory)) { fs::remove_all(path); }

    ~TempDir() {
        std::error_code error;
        fs::remove_all(path, error);
    }
};

int fail(const char* message) {
    std::cerr << message << '\n';
    return 1;
}

bool write_text(const fs::path& path, std::string_view text) {
    std::ofstream file(path, std::ios::binary | std::ios::trunc);
    if (!file) {
        return false;
    }
    file.write(text.data(), static_cast<std::streamsize>(text.size()));
    return static_cast<bool>(file);
}

bool has_resolution(const std::vector<lla::Resolution>& list, int width, int height) {
    for (const lla::Resolution& item : list) {
        if (item.width == width && item.height == height) {
            return true;
        }
    }
    return false;
}

int test_logic() {
    if (lla::step_volume(80, 1) != 85 || lla::step_volume(100, 1) != 100 || lla::step_volume(0, -1) != 0 ||
        lla::step_volume(98, 1) != 100) {
        return fail("volume did not step by 5 inside 0 to 100");
    }
    if (lla::step_text_scale(2, 1) != 3 || lla::step_text_scale(3, 1) != 4 || lla::step_text_scale(4, 1) != 4 ||
        lla::step_text_scale(2, -1) != 2) {
        return fail("text size did not step Small Medium Large");
    }
    if (std::string(lla::text_size_name(2)) != "Small" || std::string(lla::text_size_name(3)) != "Medium" ||
        std::string(lla::text_size_name(4)) != "Large") {
        return fail("text size name mismatch");
    }

    std::vector<lla::Resolution> list;
    lla::list_resolutions(1920, 1080, list);
    if (!has_resolution(list, 1280, 720) || !has_resolution(list, 1920, 1080) || has_resolution(list, 2560, 1440)) {
        return fail("1080p resolution list was wrong");
    }
    lla::list_resolutions(1280, 720, list);
    if (list.size() != 1 || !has_resolution(list, 1280, 720)) {
        return fail("720p resolution list was not unique");
    }
    lla::list_resolutions(800, 600, list);
    if (list.size() != 1 || !has_resolution(list, 800, 600)) {
        return fail("a screen that is not a preset was dropped");
    }
    lla::list_resolutions(100, 100, list);
    if (list.size() != 1 || !has_resolution(list, 1280, 720)) {
        return fail("a tiny screen did not fall back to 1280x720");
    }

    lla::DisplayPrompt prompt;
    lla::begin_display_trial(prompt, {true, 800, 600}, {false, 1280, 720});
    if (!prompt.open || prompt.remaining != lla::kDisplayRevertSeconds) {
        return fail("display trial did not start at 15 seconds");
    }
    if (lla::tick_display_trial(prompt, 14.f) || !prompt.open || prompt.remaining != 1.f) {
        return fail("display trial ended early");
    }
    if (!lla::tick_display_trial(prompt, 1.f) || prompt.open) {
        return fail("display trial did not revert when the timer ended");
    }
    if (lla::tick_display_trial(prompt, 1.f)) {
        return fail("a closed display trial ticked again");
    }

    const lla::KeyBinds binds;
    if (!lla::bind_in_use(binds, 38, 0) || lla::bind_in_use(binds, 38, 1) || lla::bind_in_use(binds, 99, 0)) {
        return fail("bind in use check was wrong");
    }
    if (!lla::key_code_allowed(111) || !lla::key_code_allowed(116) || !lla::key_code_allowed(25) || !lla::key_code_allowed(39) ||
        lla::key_code_allowed(7) || !lla::key_code_allowed(9) || !lla::key_code_allowed(38) || !lla::key_code_allowed(40) ||
        !lla::key_code_allowed(255)) {
        return fail("menu keys must be allowed binds");
    }
    if (binds.menu_up != 111 || binds.menu_up_alt != 25 || binds.menu_down != 116 || binds.menu_down_alt != 39) {
        return fail("menu up and down did not default to the arrows and W/S");
    }
    return 0;
}

int test_store(const fs::path& directory) {
    const fs::path file = directory / "device.json";
    {
        lla::JsonDeviceSettings fresh(file);
        const lla::DeviceSettings defaults = fresh.get();
        if (defaults.fullscreen || defaults.width != 1280 || defaults.height != 720 || defaults.master != 100 ||
            defaults.text_scale != 2 || defaults.binds.pause != 9 || defaults.binds.menu_up != 111 ||
            defaults.binds.menu_up_alt != 25 || defaults.binds.menu_down != 116 || defaults.binds.menu_down_alt != 39 ||
            defaults.binds.move_left_alt != 38 || defaults.binds.move_right_alt != 40) {
            return fail("missing device file did not use defaults");
        }
        lla::DeviceSettings next = defaults;
        next.fullscreen = true;
        next.width = 1600;
        next.height = 900;
        next.mute = true;
        next.master = 40;
        next.music = 50;
        next.effects = 60;
        next.menu_volume = 70;
        next.text_scale = 4;
        next.high_contrast = true;
        next.binds.move_left = 38;
        next.binds.menu_up = 24;
        next.binds.menu_down_alt = 26;
        next.binds.pause = 33;
        fresh.replace(next);
        fresh.preview_display(false, 800, 600);
        if (fresh.get().width != 1600 || !fresh.get().fullscreen) {
            return fail("preview changed the saved display");
        }
        fresh.commit_display(false, 1280, 720);
        if (fresh.get().fullscreen || fresh.get().width != 1280 || fresh.get().height != 720 || fresh.get().master != 40) {
            return fail("commit display did not keep the other settings");
        }
    }

    lla::JsonDeviceSettings loaded(file);
    const lla::DeviceSettings& saved = loaded.get();
    if (saved.fullscreen || saved.width != 1280 || saved.height != 720 || !saved.mute || saved.master != 40 ||
        saved.music != 50 || saved.effects != 60 || saved.menu_volume != 70 || saved.text_scale != 4 ||
        !saved.high_contrast || saved.binds.move_left != 38 || saved.binds.pause != 33 || saved.binds.menu_up != 24 ||
        saved.binds.menu_down_alt != 26) {
        return fail("device file did not round trip");
    }

    if (!write_text(file, "{\n  \"width\": 10,\n  \"height\": 9000,\n  \"text\": 9,\n  \"master\": 500,\n  \"left\": 4,\n  \"contrast\": true\n}\n")) {
        return fail("could not write a bad device file");
    }
    lla::JsonDeviceSettings clamped(file);
    const lla::DeviceSettings& fixed = clamped.get();
    if (fixed.width != 320 || fixed.height != 4320 || fixed.text_scale != 4 || fixed.master != 100 ||
        fixed.binds.move_left != 113 || !fixed.high_contrast) {
        return fail("out of range device values were not clamped");
    }

    if (!write_text(file, "{\n  \"master\": 5,\n  \"master\": 55\n}\n")) {
        return fail("could not write duplicate device keys");
    }
    lla::JsonDeviceSettings duplicate(file);
    if (duplicate.get().master != 55 || duplicate.get().width != 1280) {
        return fail("the last device value did not win");
    }
    return 0;
}

}  // namespace

int main() {
    if (const int logic = test_logic(); logic != 0) {
        return logic;
    }
    const TempDir directory(fs::temp_directory_path() / ("lla-settings-" + std::to_string(::getpid())));
    return test_store(directory.path);
}
