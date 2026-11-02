#pragma once

#include "lla/contracts.hpp"

namespace lla::ui {

struct Theme {
    Color background{16, 16, 20, 255};
    Color panel{34, 36, 44, 255};
    Color panel_edge{150, 156, 168, 255};
    Color button{46, 50, 60, 255};
    Color button_edge{96, 102, 116, 255};
    Color selected{58, 92, 148, 255};
    Color selected_edge{150, 190, 230, 255};
    Color title{236, 236, 236, 255};
    Color label{236, 236, 236, 255};
    Color status{232, 196, 120, 255};
    Color hint{150, 156, 168, 255};
    int text_scale = 2;
    int title_scale = 3;
};

inline Theme theme_from(int text_scale, bool high_contrast) {
    Theme theme;
    if (text_scale < 2) {
        text_scale = 2;
    } else if (text_scale > 4) {
        text_scale = 4;
    }
    theme.text_scale = text_scale;
    theme.title_scale = text_scale + 1;
    if (!high_contrast) {
        return theme;
    }
    theme.background = {0, 0, 0, 255};
    theme.panel = {0, 0, 0, 255};
    theme.panel_edge = {255, 255, 255, 255};
    theme.button = {0, 0, 0, 255};
    theme.button_edge = {255, 255, 255, 255};
    theme.selected = {0, 70, 170, 255};
    theme.selected_edge = {255, 255, 255, 255};
    theme.title = {255, 255, 255, 255};
    theme.label = {255, 255, 255, 255};
    theme.status = {255, 220, 80, 255};
    theme.hint = {255, 255, 255, 255};
    return theme;
}

inline Theme theme_from(const IDeviceSettings* settings) {
    if (settings == nullptr) {
        return theme_from(2, false);
    }
    const DeviceSettings& current = settings->get();
    return theme_from(current.text_scale, current.high_contrast);
}

}  // namespace lla::ui
