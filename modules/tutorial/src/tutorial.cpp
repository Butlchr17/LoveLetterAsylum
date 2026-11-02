#include "lla/tutorial/register.hpp"

#include "lla/party/party.hpp"
#include "lla/tutorial/walk.hpp"
#include "lla/ui/menu_layout.hpp"
#include "lla/ui/text.hpp"
#include "lla/ui/theme.hpp"

#include <algorithm>
#include <cmath>
#include <string_view>

namespace lla {
namespace {

constexpr int kPauseRows = 3;

constexpr Color kCeiling{12, 12, 16, 255};
constexpr Color kBackWall{26, 28, 34, 255};
constexpr Color kSideWall{16, 16, 20, 255};
constexpr Color kPilaster{40, 44, 54, 255};
constexpr Color kFloorFar{34, 36, 44, 255};
constexpr Color kFloorNear{68, 72, 84, 255};
constexpr Color kGroundLine{120, 128, 142, 255};
constexpr Color kCharacter{214, 218, 226, 255};
constexpr const char* kRoomLabel = "Tutorial";
constexpr const char* kMoveHint = "Arrows or A/D move    Esc pause";
constexpr const char* kPauseTitle = "Paused";
constexpr const char* kContinue = "Continue";
constexpr const char* kOptions = "Options";
constexpr const char* kExit = "Exit to main menu";
constexpr const char* kPauseHint = "Up/Down select    Enter or click confirm";
constexpr const char* kPauseLabels[kPauseRows] = {kContinue, kOptions, kExit};
const std::string_view kPauseRowText[] = {kContinue, kOptions, kExit};

struct RoomLayout {
    float side = 0;
    float horizon = 0;
    float floor_bottom = 0;
    float feet = 0;
    float character_w = 0;
    float character_h = 0;
    float walk_left = 0;
    float walk_right = 0;
};



std::uint8_t shade(float value) {
    if (value < 0.f) {
        return 0;
    }
    if (value > 255.f) {
        return 255;
    }
    return static_cast<std::uint8_t>(value + 0.5f);
}

RoomLayout room_layout(int width, int height) {
    const float w = static_cast<float>(width);
    const float h = static_cast<float>(height);
    RoomLayout room;
    room.side = std::max(24.f, std::round(w * 0.08f));
    room.horizon = std::round(h * 0.42f);
    room.floor_bottom = std::round(h * 0.86f);
    room.feet = std::round(h * 0.76f);
    room.character_h = std::round(h * 0.24f);
    room.character_w = std::round(room.character_h * 0.36f);
    room.walk_left = room.side + 28.f;
    room.walk_right = w - room.side - room.character_w - 28.f;
    if (room.walk_right < room.walk_left) {
        room.walk_right = room.walk_left;
    }
    return room;
}

float character_x(const RoomLayout& room, float position) {
    return room.walk_left + (room.walk_right - room.walk_left) * position;
}

ui::MenuStack pause_layout(int width, int height, const ui::Theme& theme) {
    ui::MenuSpec spec;
    spec.window_w = width;
    spec.window_h = height;
    spec.text_scale = theme.text_scale;
    spec.title_scale = theme.title_scale;
    spec.title = kPauseTitle;
    spec.hint = kPauseHint;
    spec.preferred_w = std::min(560.f, std::max(320.f, static_cast<float>(width) - 48.f));
    spec.preferred_h = std::min(400.f, std::max(360.f, static_cast<float>(height) - 80.f));
    spec.rows = kPauseRowText;
    return ui::layout_menu(spec);
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

Rect screen_line(float x, float y, float max_w, float max_bottom, int scale, std::string_view text, float margin) {
    const int width_px = std::max(1, static_cast<int>(std::floor(max_w)));
    const int lines = std::max(1, ui::wrapped_line_count(text, scale, width_px));
    const float height = static_cast<float>(ui::text_block_height(lines, scale));
    float top = y;
    if (top + height > max_bottom) {
        top = max_bottom - height;
    }
    if (top < margin) {
        top = margin;
    }
    float box_h = height;
    if (top + box_h > max_bottom) {
        box_h = std::max(0.f, max_bottom - top);
    }
    return {x, top, max_w, box_h};
}

void draw_members(ICanvas& canvas, const RoomLayout& room, float position, const int* ranks) {
    const float front_x = character_x(room, position);
    const float step = std::max(18.f, room.character_w * 0.85f);
    MemberPlace members[kPartySlotCount];
    const int count = place_members(ranks, front_x, step, members, kPartySlotCount);
    if (count <= 0) {
        canvas.fill_rect({front_x, room.feet - room.character_h, room.character_w, room.character_h}, kCharacter);
        return;
    }
    for (int index = 0; index < count; ++index) {
        float x = members[index].x;
        if (x < 0.f) {
            x = 0.f;
        }
        const ClassDef* def = find_class(members[index].class_id);
        const Color color = def == nullptr ? kCharacter : def->color;
        canvas.fill_rect({x, room.feet - room.character_h, room.character_w, room.character_h}, color);
    }
}

void draw_room(ICanvas& canvas, float position, const int* ranks, bool show_hint, const ui::Theme& theme) {
    const float width = static_cast<float>(canvas.width());
    const float height = static_cast<float>(canvas.height());
    const RoomLayout room = room_layout(canvas.width(), canvas.height());
    canvas.clear(kCeiling);
    canvas.fill_rect({0.f, 0.f, width, room.horizon}, kCeiling);
    canvas.fill_rect({room.side, height * 0.1f, width - room.side * 2.f, room.horizon - height * 0.1f}, kBackWall);

    constexpr int kBands = 8;
    const float floor_h = std::max(1.f, room.floor_bottom - room.horizon);
    for (int band = 0; band < kBands; ++band) {
        const float start = static_cast<float>(band) / kBands;
        const float end = static_cast<float>(band + 1) / kBands;
        const float y = room.horizon + floor_h * start;
        const float band_h = floor_h * (end - start) + 1.f;
        const float inset = (1.f - start) * room.side * 0.45f;
        const Color color{
            shade(static_cast<float>(kFloorFar.r) + (static_cast<float>(kFloorNear.r) - kFloorFar.r) * start),
            shade(static_cast<float>(kFloorFar.g) + (static_cast<float>(kFloorNear.g) - kFloorFar.g) * start),
            shade(static_cast<float>(kFloorFar.b) + (static_cast<float>(kFloorNear.b) - kFloorFar.b) * start),
            255,
        };
        canvas.fill_rect({room.side + inset, y, std::max(1.f, width - (room.side + inset) * 2.f), band_h}, color);
    }

    canvas.fill_rect({0.f, 0.f, room.side, room.floor_bottom}, kSideWall);
    canvas.fill_rect({width - room.side, 0.f, room.side, room.floor_bottom}, kSideWall);
    const float pilaster_w = std::max(18.f, room.side * 0.45f);
    canvas.fill_rect({room.side + width * 0.16f, height * 0.14f, pilaster_w, room.horizon - height * 0.14f}, kPilaster);
    canvas.fill_rect({width - room.side - width * 0.16f - pilaster_w, height * 0.14f, pilaster_w, room.horizon - height * 0.14f}, kPilaster);
    canvas.fill_rect({room.walk_left, room.feet, room.walk_right + room.character_w - room.walk_left, 3.f}, kGroundLine);

    draw_members(canvas, room, position, ranks);
    const float margin = 16.f;
    const Rect room_label = screen_line(
        room.side + 16.f, 24.f, std::max(1.f, width - room.side - 32.f), height - margin, theme.text_scale, kRoomLabel, margin);
    ui::draw_text_box(canvas, room_label, kRoomLabel, theme.hint, theme.text_scale, ui::TextAlign::Left, false);
    if (show_hint) {
        const Rect hint = screen_line(margin, height - 36.f, std::max(1.f, width - margin * 2.f), height - 22.f, theme.text_scale, kMoveHint, margin);
        ui::draw_text_box(canvas, hint, kMoveHint, theme.hint, theme.text_scale, ui::TextAlign::Left, false);
    }
}

class TutorialModule final : public IModule {
public:
    TutorialModule(IPlayPhase* phase, ISaveSlots* saves, ISettingsScreen** settings, IDeviceSettings* devices)
        : phase_(phase), saves_(saves), settings_(settings), devices_(devices) {}

    std::string_view name() const override { return "tutorial.room"; }

    void shutdown() override {
        if (active_) {
            persist_position();
        }
    }

    void update(const InputFrame& input) override {
        if (phase_ == nullptr || phase_->phase() != PlayPhase::Tutorial) {
            active_ = false;
            return;
        }
        if (!active_) {
            active_ = true;
            position_ = phase_->resume_position();
            paused_ = false;
            pause_index_ = 0;
            const int* stored = phase_->party_ranks();
            for (int slot = 0; slot < kPartySlotCount; ++slot) {
                ranks_[slot] = stored == nullptr ? 0 : stored[slot];
            }
            phase_->note_position(position_);
        }
        if (input.window_width <= 0 || input.window_height <= 0) {
            return;
        }
        // Settings is updated later in the frame, so ignore input while it is open.
        // That keeps the pause menu up when Escape closes settings.
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
                persist_position();
                active_ = false;
                paused_ = false;
                phase_->return_to_menu();
            }
            return;
        }
        position_ = move_walk(position_, input.left, input.right, input.dt, kWalkSpeed);
        phase_->note_position(position_);
    }

    void draw(ICanvas& canvas) override {
        if (!active_ || phase_ == nullptr || phase_->phase() != PlayPhase::Tutorial) {
            return;
        }
        if (canvas.width() <= 0 || canvas.height() <= 0) {
            return;
        }
        const ui::Theme colors = theme();
        const bool show_pause = paused_ || settings_open();
        draw_room(canvas, position_, ranks_, !show_pause, colors);
        if (!paused_) {
            return;
        }
        ui::MenuSpec spec;
        spec.window_w = canvas.width();
        spec.window_h = canvas.height();
        spec.text_scale = colors.text_scale;
        spec.title_scale = colors.title_scale;
        spec.title = kPauseTitle;
        spec.hint = kPauseHint;
        spec.preferred_w = std::min(560.f, std::max(320.f, static_cast<float>(canvas.width()) - 48.f));
        spec.preferred_h = std::min(400.f, std::max(360.f, static_cast<float>(canvas.height()) - 80.f));
        spec.rows = kPauseRowText;
        const ui::MenuStack layout = ui::layout_menu(spec);
        ui::draw_menu_panel(canvas, layout, colors, false);
        for (int index = 0; index < kPauseRows && index < static_cast<int>(layout.rows.size()); ++index) {
            ui::draw_menu_button(
                canvas, layout.rows[static_cast<std::size_t>(index)], kPauseLabels[index], index == pause_index_, colors, layout.row_pad_x,
                layout.row_pad_right);
        }
        ui::draw_menu_text(canvas, layout, spec, colors);
    }

private:
    ui::Theme theme() const { return ui::theme_from(devices_); }

    bool settings_open() const {
        return settings_ != nullptr && *settings_ != nullptr && (*settings_)->is_open();
    }

    void persist_position() const {
        if (saves_ == nullptr || phase_ == nullptr) {
            return;
        }
        const int slot = phase_->active_slot();
        if (slot < 1) {
            return;
        }
        int millis = static_cast<int>(std::lround(position_ * 1000.f));
        if (millis < 0) {
            millis = 0;
        } else if (millis > 1000) {
            millis = 1000;
        }
        saves_->store_progress(slot, millis);
    }

    IPlayPhase* phase_ = nullptr;
    ISaveSlots* saves_ = nullptr;
    ISettingsScreen** settings_ = nullptr;
    IDeviceSettings* devices_ = nullptr;
    bool active_ = false;
    bool paused_ = false;
    int pause_index_ = 0;
    float position_ = 0.5f;
    int ranks_[kPartySlotCount] = {};
};

}  // namespace

void register_tutorial(
    ModuleRegistry& registry, IPlayPhase* phase, ISaveSlots* saves, ISettingsScreen** settings, IDeviceSettings* devices) {
    registry.add<TutorialModule>(phase, saves, settings, devices);
}

}  // namespace lla
