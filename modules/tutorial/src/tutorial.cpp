#include "lla/tutorial/register.hpp"

#include "lla/party/party.hpp"
#include "lla/tutorial/follow.hpp"
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
    float scene_top = 0;
    float back_top = 0;
    float pilaster_top = 0;
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

RoomLayout room_layout(int width, int height, float top_inset, float bottom_inset) {
    const float w = static_cast<float>(width);
    const float h = static_cast<float>(height);
    RoomLayout room;
    room.side = std::max(24.f, std::round(w * 0.08f));
    const bool inset = top_inset > 0.5f || bottom_inset > 0.5f;
    if (!inset) {
        room.scene_top = 0.f;
        room.back_top = h * 0.1f;
        room.pilaster_top = h * 0.14f;
        room.horizon = std::round(h * 0.42f);
        room.floor_bottom = std::round(h * 0.86f);
        room.feet = std::round(h * 0.76f);
        room.character_h = std::round(h * 0.24f);
    } else {
        const float scene_top = std::min(std::max(0.f, top_inset), std::max(0.f, h - 1.f));
        const float scene_bottom = std::max(scene_top + 1.f, h - std::max(0.f, bottom_inset));
        const float scene_h = scene_bottom - scene_top;
        room.scene_top = scene_top;
        room.back_top = scene_top + scene_h * 0.1f;
        room.pilaster_top = scene_top + scene_h * 0.14f;
        room.horizon = std::round(scene_top + scene_h * 0.42f);
        room.floor_bottom = std::round(scene_bottom);
        room.feet = std::round(scene_bottom - std::max(10.f, scene_h * 0.06f));
        room.character_h = std::round(scene_h * 0.30f);
        const float headroom = room.feet - room.horizon - 8.f;
        if (room.character_h > headroom) {
            room.character_h = std::max(12.f, headroom);
        }
    }
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

Color scale_color(Color color, float mul) {
    return {shade(color.r * mul), shade(color.g * mul), shade(color.b * mul), 255};
}

Color mix_white(Color color, float amount) {
    return {
        shade(static_cast<float>(color.r) + (255.f - static_cast<float>(color.r)) * amount),
        shade(static_cast<float>(color.g) + (255.f - static_cast<float>(color.g)) * amount),
        shade(static_cast<float>(color.b) + (255.f - static_cast<float>(color.b)) * amount),
        255,
    };
}

void fill_part(ICanvas& canvas, float body_x, float body_w, float local_x, float local_w, float y, float h, Color color, int facing) {
    if (!(local_w > 0.f) || !(h > 0.f) || !(body_w > 0.f)) {
        return;
    }
    float x = mirrored_x(body_x, body_w, local_x, local_w, facing);
    float w = local_w;
    if (x < body_x) {
        w -= body_x - x;
        x = body_x;
    }
    if (x + w > body_x + body_w) {
        w = body_x + body_w - x;
    }
    if (w > 0.f) {
        canvas.fill_rect({x, y, w, h}, color);
    }
}

// Placeholder body. The dark plate and light mark sit on the front. Facing left
// mirrors those parts across the body's vertical center, which is the same flip
// a later sprite will use.
void draw_body(ICanvas& canvas, float x, float feet, float w, float h, Color color, int facing) {
    if (x < 0.f) {
        x = 0.f;
    }
    if (!(w > 0.f) || !(h > 0.f)) {
        return;
    }
    const float top = feet - h;
    canvas.fill_rect({x, top, w, h}, color);
    const float plate_w = std::min(w, std::max(2.f, std::round(w * 0.30f)));
    const float plate_h = std::max(2.f, std::round(h * 0.72f));
    fill_part(canvas, x, w, w - plate_w, plate_w, top, plate_h, scale_color(color, 0.55f), facing);
    const float eye_w = std::min(plate_w, std::max(2.f, std::round(w * 0.14f)));
    const float eye_h = std::max(2.f, std::round(h * 0.10f));
    const float eye_local = std::max(0.f, w - plate_w * 0.62f);
    fill_part(canvas, x, w, eye_local, eye_w, top + h * 0.16f, eye_h, mix_white(color, 0.72f), facing);
}

void draw_members(ICanvas& canvas, const RoomLayout& room, float leader_x, int leader_facing, const FollowLine& line) {
    if (line.count <= 0) {
        draw_body(canvas, leader_x, room.feet, room.character_w, room.character_h, kCharacter, leader_facing);
        return;
    }
    for (int index = 0; index < line.count; ++index) {
        const bool leader = index == line.count - 1;
        if (!leader) {
            const FollowMember& member = line.members[index];
            const ClassDef* def = find_class(member.class_id);
            const Color color = def == nullptr ? kCharacter : def->color;
            draw_body(canvas, member.x, room.feet, room.character_w, room.character_h, color, member.facing);
        }
    }
    const FollowMember& front = line.members[line.count - 1];
    const ClassDef* def = find_class(front.class_id);
    const Color color = def == nullptr ? kCharacter : def->color;
    draw_body(canvas, front.x, room.feet, room.character_w, room.character_h, color, front.facing);
}

void draw_room(
    ICanvas& canvas,
    float position,
    int facing,
    const FollowLine& line,
    float top_inset,
    float bottom_inset,
    bool show_label,
    bool show_hint,
    const ui::Theme& theme) {
    const float width = static_cast<float>(canvas.width());
    const float height = static_cast<float>(canvas.height());
    const RoomLayout room = room_layout(canvas.width(), canvas.height(), top_inset, bottom_inset);
    canvas.clear(kCeiling);
    canvas.fill_rect({0.f, 0.f, width, room.horizon}, kCeiling);
    canvas.fill_rect({room.side, room.back_top, width - room.side * 2.f, std::max(1.f, room.horizon - room.back_top)}, kBackWall);

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

    canvas.fill_rect({0.f, room.scene_top, room.side, std::max(1.f, room.floor_bottom - room.scene_top)}, kSideWall);
    canvas.fill_rect({width - room.side, room.scene_top, room.side, std::max(1.f, room.floor_bottom - room.scene_top)}, kSideWall);
    const float pilaster_w = std::max(18.f, room.side * 0.45f);
    const float pilaster_h = std::max(1.f, room.horizon - room.pilaster_top);
    canvas.fill_rect({room.side + width * 0.16f, room.pilaster_top, pilaster_w, pilaster_h}, kPilaster);
    canvas.fill_rect({width - room.side - width * 0.16f - pilaster_w, room.pilaster_top, pilaster_w, pilaster_h}, kPilaster);
    canvas.fill_rect({room.walk_left, room.feet, room.walk_right + room.character_w - room.walk_left, 3.f}, kGroundLine);

    draw_members(canvas, room, character_x(room, position), facing, line);
    const float margin = 16.f;
    if (show_label) {
        const Rect room_label = screen_line(
            room.side + 16.f, 24.f, std::max(1.f, width - room.side - 32.f), height - margin, theme.text_scale, kRoomLabel, margin);
        ui::draw_text_box(canvas, room_label, kRoomLabel, theme.hint, theme.text_scale, ui::TextAlign::Left, false);
    }
    if (show_hint) {
        const Rect hint = screen_line(margin, height - 36.f, std::max(1.f, width - margin * 2.f), height - 22.f, theme.text_scale, kMoveHint, margin);
        ui::draw_text_box(canvas, hint, kMoveHint, theme.hint, theme.text_scale, ui::TextAlign::Left, false);
    }
}

class TutorialModule final : public IModule {
public:
    TutorialModule(IPlayPhase* phase, ISaveSlots* saves, ISettingsScreen** settings, IDeviceSettings* devices, IDungeonHud* hud)
        : phase_(phase), saves_(saves), settings_(settings), devices_(devices), hud_(hud) {}

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
            facing_ = 1;
            line_ready_ = false;
            const int* stored = phase_->party_ranks();
            for (int slot = 0; slot < kPartySlotCount; ++slot) {
                ranks_[slot] = stored == nullptr ? 0 : stored[slot];
            }
            phase_->note_position(position_);
        }
        if (input.window_width <= 0 || input.window_height <= 0) {
            return;
        }
        sync_follow(input.window_width, input.window_height, false);
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
        facing_ = held_facing(facing_, input.left, input.right);
        position_ = move_walk(position_, input.left, input.right, input.dt, kWalkSpeed);
        phase_->note_position(position_);
        sync_follow(input.window_width, input.window_height, true);
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
        float top_inset = 0.f;
        float bottom_inset = 0.f;
        if (hud_ != nullptr) {
            top_inset = hud_->top_band(canvas.width(), canvas.height(), colors.text_scale);
            bottom_inset = hud_->bottom_band(canvas.width(), canvas.height(), colors.text_scale);
        }
        const bool hud_on = hud_ != nullptr && top_inset > 0.f && bottom_inset > 0.f;
        if (!line_ready_) {
            sync_follow(canvas.width(), canvas.height(), false);
        }
        draw_room(canvas, position_, facing_, line_, top_inset, bottom_inset, !hud_on, !show_pause && !hud_on, colors);
        if (hud_on) {
            const bool contrast = devices_ != nullptr && devices_->get().high_contrast;
            hud_->draw(canvas, kRoomLabel, ranks_, position_, colors.text_scale, contrast);
        }
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

    bool near_layout(float step, float walk_left, float span) const {
        const auto close = [](float actual, float expected) {
            const float delta = actual - expected;
            return (delta < 0.f ? -delta : delta) < 0.5f;
        };
        return close(follow_step_, step) && close(follow_walk_left_, walk_left) && close(follow_span_, span);
    }

    void sync_follow(int width, int height, bool advance) {
        if (width <= 0 || height <= 0) {
            return;
        }
        float top = 0.f;
        float bottom = 0.f;
        if (hud_ != nullptr) {
            const int scale = theme().text_scale;
            top = hud_->top_band(width, height, scale);
            bottom = hud_->bottom_band(width, height, scale);
        }
        const RoomLayout room = room_layout(width, height, top, bottom);
        const float step = follow_distance(room.character_w);
        const float leader_x = character_x(room, position_);
        const float span = room.walk_right - room.walk_left;
        const bool layout_same = line_ready_ && near_layout(step, room.walk_left, span);
        const bool roster_same = line_ready_ && follow_roster_same(line_, ranks_);
        if (!layout_same || !roster_same) {
            line_ = arrange_follow(ranks_, leader_x, facing_, step);
            line_ready_ = true;
            follow_step_ = step;
            follow_walk_left_ = room.walk_left;
            follow_span_ = span;
            return;
        }
        if (!advance) {
            return;
        }
        line_ = advance_follow(ranks_, leader_x, facing_, step, line_);
        follow_step_ = step;
        follow_walk_left_ = room.walk_left;
        follow_span_ = span;
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
    IDungeonHud* hud_ = nullptr;
    bool active_ = false;
    bool paused_ = false;
    int pause_index_ = 0;
    float position_ = 0.5f;
    int facing_ = 1;
    bool line_ready_ = false;
    float follow_step_ = -1.f;
    float follow_walk_left_ = 0.f;
    float follow_span_ = 0.f;
    FollowLine line_{};
    int ranks_[kPartySlotCount] = {};
};

}  // namespace

void register_tutorial(
    ModuleRegistry& registry, IPlayPhase* phase, ISaveSlots* saves, ISettingsScreen** settings, IDeviceSettings* devices, IDungeonHud* hud) {
    registry.add<TutorialModule>(phase, saves, settings, devices, hud);
}

}  // namespace lla
