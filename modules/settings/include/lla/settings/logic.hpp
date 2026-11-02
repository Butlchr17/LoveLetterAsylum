#pragma once

#include "lla/contracts.hpp"

#include <vector>

namespace lla {

inline constexpr float kDisplayRevertSeconds = 15.f;

struct Resolution {
    int width = 1280;
    int height = 720;
};

struct DisplayChoice {
    bool fullscreen = false;
    int width = 1280;
    int height = 720;

    bool operator==(const DisplayChoice&) const = default;
};

struct DisplayPrompt {
    bool open = false;
    float remaining = 0.f;
    DisplayChoice trial{};
    DisplayChoice committed{};
};

inline constexpr int kBindCount = 11;

inline bool key_code_allowed(int code) {
    return code >= 8 && code <= 255;
}

inline int bind_value(const KeyBinds& binds, int index) {
    switch (index) {
        case 0:
            return binds.move_left;
        case 1:
            return binds.move_left_alt;
        case 2:
            return binds.move_right;
        case 3:
            return binds.move_right_alt;
        case 4:
            return binds.menu_up;
        case 5:
            return binds.menu_up_alt;
        case 6:
            return binds.menu_down;
        case 7:
            return binds.menu_down_alt;
        case 8:
            return binds.confirm;
        case 9:
            return binds.confirm_alt;
        case 10:
            return binds.pause;
        default:
            return 0;
    }
}

inline void set_bind_value(KeyBinds& binds, int index, int code) {
    switch (index) {
        case 0:
            binds.move_left = code;
            break;
        case 1:
            binds.move_left_alt = code;
            break;
        case 2:
            binds.move_right = code;
            break;
        case 3:
            binds.move_right_alt = code;
            break;
        case 4:
            binds.menu_up = code;
            break;
        case 5:
            binds.menu_up_alt = code;
            break;
        case 6:
            binds.menu_down = code;
            break;
        case 7:
            binds.menu_down_alt = code;
            break;
        case 8:
            binds.confirm = code;
            break;
        case 9:
            binds.confirm_alt = code;
            break;
        case 10:
            binds.pause = code;
            break;
        default:
            break;
    }
}

inline bool bind_in_use(const KeyBinds& binds, int code, int except_index) {
    for (int index = 0; index < kBindCount; ++index) {
        if (index != except_index && bind_value(binds, index) == code) {
            return true;
        }
    }
    return false;
}

inline int step_volume(int value, int direction) {
    int next = value + direction * 5;
    if (next < 0) {
        return 0;
    }
    if (next > 100) {
        return 100;
    }
    return next;
}

inline int step_text_scale(int scale, int direction) {
    int current = 0;
    if (scale >= 4) {
        current = 2;
    } else if (scale >= 3) {
        current = 1;
    }
    int next = current + direction;
    if (next < 0) {
        next = 0;
    } else if (next > 2) {
        next = 2;
    }
    constexpr int kSteps[] = {2, 3, 4};
    return kSteps[next];
}

inline const char* text_size_name(int scale) {
    if (scale <= 2) {
        return "Small";
    }
    if (scale == 3) {
        return "Medium";
    }
    return "Large";
}

inline void list_resolutions(int screen_w, int screen_h, std::vector<Resolution>& out) {
    out.clear();
    constexpr Resolution kPresets[] = {
        {1280, 720}, {1366, 768}, {1600, 900}, {1920, 1080}, {2560, 1440}, {3840, 2160},
    };
    for (const Resolution& preset : kPresets) {
        if (preset.width <= screen_w && preset.height <= screen_h) {
            out.push_back(preset);
        }
    }
    bool have_screen = false;
    for (const Resolution& item : out) {
        if (item.width == screen_w && item.height == screen_h) {
            have_screen = true;
        }
    }
    if (!have_screen && screen_w >= 320 && screen_h >= 240) {
        out.push_back({screen_w, screen_h});
    }
    if (out.empty()) {
        out.push_back({1280, 720});
    }
}

inline void begin_display_trial(DisplayPrompt& prompt, DisplayChoice trial, DisplayChoice committed) {
    prompt.open = true;
    prompt.remaining = kDisplayRevertSeconds;
    prompt.trial = trial;
    prompt.committed = committed;
}

inline bool tick_display_trial(DisplayPrompt& prompt, float dt) {
    if (!prompt.open) {
        return false;
    }
    if (dt > 0.f) {
        prompt.remaining -= dt;
    }
    if (prompt.remaining <= 0.f) {
        prompt.open = false;
        prompt.remaining = 0.f;
        return true;
    }
    return false;
}

inline DisplayChoice display_choice(const DeviceSettings& settings) {
    return {settings.fullscreen, settings.width, settings.height};
}

}  // namespace lla
