#include "lla/dungeon_ui/hud.hpp"

#include "lla/party/party.hpp"
#include "lla/ui/text.hpp"
#include "lla/ui/theme.hpp"

#include <algorithm>
#include <cmath>
#include <string_view>

namespace lla {
namespace {

constexpr Color kWell{18, 20, 26, 255};
constexpr Color kEmptySwatch{28, 30, 36, 255};
constexpr Color kHall{72, 76, 88, 255};
constexpr Color kDarkInk{16, 16, 20, 255};
constexpr Color kLightInk{236, 236, 240, 255};
constexpr float kTileRatio = 0.34f;
constexpr float kHallGap = 2.f;
constexpr float kMapPad = 8.f;

int clamp_scale(int scale) {
    if (scale < 2) {
        return 2;
    }
    if (scale > 4) {
        return 4;
    }
    return scale;
}

float clamp01(float value) {
    if (value < 0.f) {
        return 0.f;
    }
    if (value > 1.f) {
        return 1.f;
    }
    return value;
}

void row_boxes(float x, float y, float width, float height, float gap, Rect* out, int count) {
    if (count <= 0 || out == nullptr) {
        return;
    }
    const float gaps = gap * static_cast<float>(count - 1);
    const float box_w = std::max(1.f, (width - gaps) / static_cast<float>(count));
    for (int index = 0; index < count; ++index) {
        out[index] = {x + static_cast<float>(index) * (box_w + gap), y, box_w, height};
    }
}

struct Fit {
    bool side = false;
    bool front_word = false;
    bool pack = false;
    float pad = 8.f;
    float gap = 6.f;
    float swatch = 28.f;
    int hint_lines = 2;
    float map_h = 0.f;
};

struct GridSpan {
    int min_c = 0;
    int max_c = 0;
    int min_r = 0;
    int max_r = 0;
    int max_seg = 1;
    int cols = 1;
    int rows = 1;
};

Color ink_for(Color color) {
    const int light = static_cast<int>(color.r) * 3 + static_cast<int>(color.g) * 6 + static_cast<int>(color.b);
    if (light > 1400) {
        return kDarkInk;
    }
    return kLightInk;
}

int clamped_segments(int segments) {
    if (segments < 1) {
        return 1;
    }
    if (segments > 6) {
        return 6;
    }
    return segments;
}

GridSpan measure_grid(const DungeonMap& map, int room_count) {
    GridSpan grid;
    grid.min_c = map.rooms[0].column;
    grid.max_c = grid.min_c;
    grid.min_r = map.rooms[0].row;
    grid.max_r = grid.min_r;
    for (int index = 0; index < room_count; ++index) {
        grid.min_c = std::min(grid.min_c, map.rooms[index].column);
        grid.max_c = std::max(grid.max_c, map.rooms[index].column);
        grid.min_r = std::min(grid.min_r, map.rooms[index].row);
        grid.max_r = std::max(grid.max_r, map.rooms[index].row);
    }
    if (map.halls != nullptr) {
        for (int index = 0; index < map.hall_count; ++index) {
            if (map.halls[index].segments > grid.max_seg) {
                grid.max_seg = map.halls[index].segments;
            }
        }
    }
    grid.max_seg = clamped_segments(grid.max_seg);
    grid.cols = std::max(1, grid.max_c - grid.min_c + 1);
    grid.rows = std::max(1, grid.max_r - grid.min_r + 1);
    return grid;
}

float map_well_height(const DungeonMap& map, int scale) {
    if (map.rooms == nullptr || map.room_count <= 0) {
        return 0.f;
    }
    const int room_count = std::min(map.room_count, kMaxMappedRooms);
    const GridSpan grid = measure_grid(map, room_count);
    const float digit_h = static_cast<float>(ui::text_height(scale)) + 2.f;
    const float room = digit_h + 14.f;
    const float link = static_cast<float>(grid.max_seg) * room * kTileRatio + static_cast<float>(grid.max_seg + 1) * kHallGap;
    const float used = static_cast<float>(grid.rows) * room + static_cast<float>(std::max(0, grid.rows - 1)) * link;
    return used + kMapPad * 2.f;
}

const DungeonRoom* mapped_room(const DungeonMap& map, int count, int id) {
    for (int index = 0; index < count; ++index) {
        if (map.rooms[index].id == id) {
            return &map.rooms[index];
        }
    }
    return nullptr;
}

void place_hall_tiles(
    DungeonHudLayout& layout,
    Rect from,
    Rect to,
    int segments,
    float tile,
    bool horizontal) {
    const float chain_gaps = static_cast<float>(std::max(0, segments - 1));
    float tile_used = tile;
    float chain = static_cast<float>(segments) * tile_used + chain_gaps;
    const float gap_start = horizontal ? std::min(from.x + from.w, to.x + to.w) : std::min(from.y + from.h, to.y + to.h);
    const float gap_end = horizontal ? std::max(from.x, to.x) : std::max(from.y, to.y);
    const float gap_span = gap_end - gap_start;
    if (gap_span < 2.f || segments < 1) {
        return;
    }
    if (chain > gap_span) {
        tile_used = (gap_span - chain_gaps) / static_cast<float>(segments);
        if (tile_used < 1.f) {
            return;
        }
        chain = static_cast<float>(segments) * tile_used + chain_gaps;
    }
    const float start = gap_start + (gap_span - chain) * 0.5f;
    const float step = tile_used + 1.f;
    for (int index = 0; index < segments && layout.map_tile_count < kMaxHallTiles; ++index) {
        if (horizontal) {
            const float y = from.y + (from.h - tile_used) * 0.5f;
            layout.map_tiles[layout.map_tile_count] = {start + static_cast<float>(index) * step, y, tile_used, tile_used};
        } else {
            const float x = from.x + (from.w - tile_used) * 0.5f;
            layout.map_tiles[layout.map_tile_count] = {x, start + static_cast<float>(index) * step, tile_used, tile_used};
        }
        ++layout.map_tile_count;
    }
}

void place_map(DungeonHudLayout& layout, const DungeonMap& map, int scale, float position) {
    layout.map_room_count = 0;
    layout.map_tile_count = 0;
    layout.map_current = map.current_room;
    layout.map_mark = {};
    if (map.rooms == nullptr || map.room_count <= 0 || layout.map.w < 8.f || layout.map.h < 8.f) {
        return;
    }
    const int room_count = std::min(map.room_count, kMaxMappedRooms);
    const GridSpan grid = measure_grid(map, room_count);
    const float pad = std::min(kMapPad, std::max(4.f, std::min(layout.map.w, layout.map.h) * 0.06f));
    const float inner_w = std::max(1.f, layout.map.w - pad * 2.f);
    const float inner_h = std::max(1.f, layout.map.h - pad * 2.f);
    const float extra_w = static_cast<float>(std::max(0, grid.cols - 1)) * static_cast<float>(grid.max_seg + 1) * kHallGap;
    const float extra_h = static_cast<float>(std::max(0, grid.rows - 1)) * static_cast<float>(grid.max_seg + 1) * kHallGap;
    const float span_w = static_cast<float>(grid.cols) +
                          static_cast<float>(std::max(0, grid.cols - 1)) * kTileRatio * static_cast<float>(grid.max_seg);
    const float span_h = static_cast<float>(grid.rows) +
                          static_cast<float>(std::max(0, grid.rows - 1)) * kTileRatio * static_cast<float>(grid.max_seg);
    float room_size = std::min((inner_w - extra_w) / span_w, (inner_h - extra_h) / span_h);
    const float digit_h = static_cast<float>(ui::text_height(scale)) + 2.f;
    const float digit_w = static_cast<float>(ui::text_width("8", scale)) + 2.f;
    room_size = std::min(room_size, digit_h + 14.f);
    if (room_size < 4.f) {
        room_size = std::min(inner_w / static_cast<float>(grid.cols), inner_h / static_cast<float>(grid.rows));
    }
    if (room_size < 2.f) {
        return;
    }
    const float tile = std::max(1.f, room_size * kTileRatio);
    const float link = static_cast<float>(grid.max_seg) * tile + static_cast<float>(grid.max_seg + 1) * kHallGap;
    const float pitch = room_size + link;
    const float used_w = static_cast<float>(grid.cols) * room_size + static_cast<float>(std::max(0, grid.cols - 1)) * link;
    const float used_h = static_cast<float>(grid.rows) * room_size + static_cast<float>(std::max(0, grid.rows - 1)) * link;
    const float origin_x = layout.map.x + pad + std::max(0.f, (inner_w - used_w) * 0.5f);
    const float origin_y = layout.map.y + pad + std::max(0.f, (inner_h - used_h) * 0.5f);
    const auto room_rect = [&](int column, int row) {
        return Rect{
            origin_x + static_cast<float>(column - grid.min_c) * pitch,
            origin_y + static_cast<float>(row - grid.min_r) * pitch,
            room_size,
            room_size,
        };
    };

    int current_index = -1;
    layout.map_room_count = room_count;
    for (int index = 0; index < room_count; ++index) {
        const DungeonRoom& room = map.rooms[index];
        layout.map_room_ids[index] = room.id;
        layout.map_rooms[index] = room_rect(room.column, room.row);
        layout.map_digits[index] = {};
        const bool one_digit = room.id >= 0 && room.id <= 9;
        const Rect& box = layout.map_rooms[index];
        if (one_digit && digit_w <= box.w && digit_h <= box.h) {
            float digit_y = box.y + (box.h - digit_h) * 0.5f;
            const bool current = room.id == map.current_room;
            const float pip = std::min(8.f, std::max(4.f, box.h * 0.18f));
            const float reserve = pip + 3.f;
            if (current && digit_h + reserve <= box.h) {
                const float area_h = box.h - reserve;
                digit_y = box.y + (area_h - digit_h) * 0.5f;
            }
            layout.map_digits[index] = {box.x + (box.w - digit_w) * 0.5f, digit_y, digit_w, digit_h};
        }
        if (room.id == map.current_room) {
            current_index = index;
        }
    }

    if (map.halls != nullptr) {
        for (int index = 0; index < map.hall_count; ++index) {
            const DungeonHall& hall = map.halls[index];
            if (hall.segments < 1) {
                continue;
            }
            const DungeonRoom* from = mapped_room(map, room_count, hall.from_id);
            const DungeonRoom* to = mapped_room(map, room_count, hall.to_id);
            if (from == nullptr || to == nullptr || from == to) {
                continue;
            }
            const int column_delta = to->column - from->column;
            const int row_delta = to->row - from->row;
            const bool horizontal = row_delta == 0 && column_delta != 0;
            const bool vertical = column_delta == 0 && row_delta != 0;
            if (!horizontal && !vertical) {
                continue;
            }
            place_hall_tiles(
                layout,
                room_rect(from->column, from->row),
                room_rect(to->column, to->row),
                clamped_segments(hall.segments),
                tile,
                horizontal);
        }
    }

    if (current_index >= 0) {
        const Rect room = layout.map_rooms[current_index];
        const Rect digit = layout.map_digits[current_index];
        const float pip = std::min(8.f, std::max(4.f, std::min(room.w, room.h) * 0.18f));
        const float pip_y = room.y + room.h - pip - 1.f;
        const bool clears_digit = digit.h <= 0.f || pip_y >= digit.y + digit.h;
        if (room.w >= pip + 1.f && room.h >= pip + 2.f && clears_digit && pip_y >= room.y) {
            const float travel = std::max(0.f, room.w - pip);
            layout.map_mark = {room.x + travel * clamp01(position), pip_y, pip, pip};
        }
    }

    // The well starts as wide as the skill column. Hug it to the room cluster and
    // keep that cluster against the right edge, which is the bottom-right corner.
    const float wanted_w = std::min(layout.map.w, used_w + pad * 2.f);
    if (wanted_w + 8.f < layout.map.w) {
        const float content_shift = (layout.map.w - wanted_w) * 0.5f;
        layout.map.x += layout.map.w - wanted_w;
        layout.map.w = wanted_w;
        layout.map_label.x = layout.map.x;
        const auto nudge = [content_shift](Rect& rect) {
            if (rect.w > 0.f && rect.h > 0.f) {
                rect.x += content_shift;
            }
        };
        for (int index = 0; index < layout.map_room_count; ++index) {
            nudge(layout.map_rooms[index]);
            nudge(layout.map_digits[index]);
        }
        for (int index = 0; index < layout.map_tile_count; ++index) {
            nudge(layout.map_tiles[index]);
        }
        nudge(layout.map_mark);
    }
}

DungeonHudLayout build_layout(
    int width,
    int height,
    int scale,
    int title_scale,
    Fit fit,
    float position,
    std::string_view room_name,
    int front,
    const DungeonMap* map) {
    DungeonHudLayout layout;
    layout.front_slot = front;
    const float margin = std::min(10.f, std::max(4.f, static_cast<float>(width) * 0.008f));
    const float line = static_cast<float>(ui::text_height(scale));
    const float title_line = static_cast<float>(ui::text_height(title_scale));
    const float pad = fit.pad;
    const float gap = fit.gap;

    const float top_h = pad + title_line + pad;
    layout.top_bar = {margin, margin, std::max(1.f, static_cast<float>(width) - margin * 2.f), top_h};
    layout.top_band = layout.top_bar.y + layout.top_bar.h;

    const float tx = layout.top_bar.x + pad;
    const float ty = layout.top_bar.y + pad;
    const float tw = std::max(1.f, layout.top_bar.w - pad * 2.f);
    const float place_w = static_cast<float>(ui::text_width(kHudPlace, scale)) + 2.f;
    const float min_track = 36.f;
    float room_w = static_cast<float>(ui::text_width(room_name, title_scale)) + 2.f;
    const float chrome = place_w + gap * 3.f + min_track;
    if (room_w + chrome > tw) {
        room_w = std::max(8.f, tw - chrome);
    }
    layout.room_name = {tx, ty, std::max(1.f, room_w), title_line};
    layout.place_label = {
        layout.room_name.x + layout.room_name.w + gap * 2.f,
        ty + std::max(0.f, (title_line - line) * 0.5f),
        place_w,
        line,
    };
    const float track_x = layout.place_label.x + layout.place_label.w + gap;
    const float track_w = std::max(8.f, tx + tw - track_x);
    const float track_h = std::max(8.f, line * 0.62f);
    layout.track = {track_x, ty + (title_line - track_h) * 0.5f, track_w, track_h};
    const float mark_w = std::min(layout.track.w, std::max(6.f, static_cast<float>(scale) * 3.f));
    layout.mark = {layout.track.x + (layout.track.w - mark_w) * clamp01(position), layout.track.y, mark_w, layout.track.h};

    const float board_w = std::max(1.f, static_cast<float>(width) - margin * 2.f);
    const float inner_w = std::max(1.f, board_w - pad * 2.f);
    int hint_lines = ui::wrapped_line_count(kHudHint, scale, std::max(1, static_cast<int>(std::floor(inner_w))));
    if (hint_lines < 1) {
        hint_lines = 1;
    }
    if (hint_lines > fit.hint_lines) {
        hint_lines = fit.hint_lines;
    }
    const float hint_h = static_cast<float>(ui::text_block_height(hint_lines, scale));
    const float name_h = line;
    const float inset = std::max(4.f, pad * 0.5f);
    const float card_h = inset + fit.swatch + gap + name_h + (fit.front_word ? gap + name_h : 0.f) + inset;
    const bool side = fit.side && width >= 900;
    const float left_w = side ? std::floor(inner_w * 0.58f) : inner_w;
    const float right_w = side ? std::max(1.f, inner_w - left_w - gap) : inner_w;
    int note_lines = ui::wrapped_line_count(kHudSkillsNote, scale, std::max(1, static_cast<int>(std::floor(right_w))));
    if (note_lines < 1) {
        note_lines = 1;
    }
    if (note_lines > 2) {
        note_lines = 2;
    }
    const float note_h = static_cast<float>(ui::text_block_height(note_lines, scale));
    const float slot_h = std::max(fit.swatch * 0.7f, name_h);
    const bool show_map = map != nullptr && map->rooms != nullptr && map->room_count > 0 && fit.map_h > 0.f;
    float right_h = slot_h + gap + note_h;
    if (fit.pack) {
        right_h += gap + slot_h;
    }
    if (show_map) {
        right_h += gap + name_h + gap + fit.map_h;
    }
    const float left_h = name_h + gap + card_h;
    const float content_h = side ? std::max(left_h, right_h) : left_h + gap + right_h;
    const float board_h = pad + content_h + gap + hint_h + pad;
    layout.board = {margin, static_cast<float>(height) - margin - board_h, board_w, board_h};
    layout.bottom_band = static_cast<float>(height) - layout.board.y;

    const float ix = layout.board.x + pad;
    const float iy = layout.board.y + pad;
    layout.party_label = {ix, iy, left_w, name_h};
    const float cards_y = layout.party_label.y + layout.party_label.h + gap;
    row_boxes(ix, cards_y, left_w, card_h, gap, layout.cards, kPartySlotCount);
    for (int index = 0; index < kPartySlotCount; ++index) {
        const float card_inset = std::min(inset, layout.cards[index].w * 0.12f);
        layout.swatches[index] = {
            layout.cards[index].x + card_inset,
            layout.cards[index].y + card_inset,
            std::max(1.f, layout.cards[index].w - card_inset * 2.f),
            fit.swatch,
        };
        layout.names[index] = {
            layout.cards[index].x + card_inset,
            layout.swatches[index].y + layout.swatches[index].h + gap,
            std::max(1.f, layout.cards[index].w - card_inset * 2.f),
            name_h,
        };
        if (fit.front_word && index == front) {
            layout.fronts[index] = {
                layout.names[index].x,
                layout.names[index].y + name_h + gap,
                layout.names[index].w,
                name_h,
            };
        }
    }

    const float right_x = side ? ix + left_w + gap : ix;
    const float right_y = side ? iy : cards_y + card_h + gap;
    const float skills_w = std::min(right_w * 0.46f, static_cast<float>(ui::text_width(kHudSkills, scale)) + 2.f);
    layout.skills_label = {right_x, right_y + std::max(0.f, (slot_h - name_h) * 0.5f), std::max(1.f, skills_w), name_h};
    const float slots_x = layout.skills_label.x + layout.skills_label.w + gap;
    const float slots_w = std::max(1.f, right_x + right_w - slots_x);
    row_boxes(slots_x, right_y, slots_w, slot_h, gap, layout.skills, kHudSkillSlots);
    layout.skills_note = {right_x, right_y + slot_h + gap, right_w, note_h};
    if (fit.pack) {
        const float pack_y = layout.skills_note.y + layout.skills_note.h + gap;
        const float pack_w = std::min(right_w * 0.34f, static_cast<float>(ui::text_width(kHudPack, scale)) + 2.f);
        layout.pack_label = {right_x, pack_y + std::max(0.f, (slot_h - name_h) * 0.5f), std::max(1.f, pack_w), name_h};
        const float cells_x = layout.pack_label.x + layout.pack_label.w + gap;
        const float cells_w = std::max(1.f, right_x + right_w - cells_x);
        row_boxes(cells_x, pack_y, cells_w, slot_h, gap, layout.pack_slots, kHudPackSlots);
    }
    layout.hint = {ix, layout.board.y + layout.board.h - pad - hint_h, inner_w, hint_h};
    if (show_map) {
        const float label_w = std::min(right_w, static_cast<float>(ui::text_width(kHudMap, scale)) + 2.f);
        layout.map = {right_x, layout.hint.y - gap - fit.map_h, right_w, fit.map_h};
        layout.map_label = {right_x, layout.map.y - gap - name_h, std::max(1.f, label_w), name_h};
        place_map(layout, *map, scale, position);
    }
    return layout;
}

bool leaves_a_scene(const DungeonHudLayout& layout, int height) {
    const float scene_min = std::max(36.f, std::min(static_cast<float>(height) * 0.36f, 520.f));
    return layout.top_band > 0.f && layout.bottom_band > 0.f && layout.board.y >= layout.top_band &&
           layout.top_band + layout.bottom_band <= static_cast<float>(height) - scene_min;
}

void clip_box(Rect& box, Rect bounds) {
    if (box.w <= 0.f || box.h <= 0.f) {
        box = {};
        return;
    }
    const float x0 = std::max(box.x, bounds.x);
    const float y0 = std::max(box.y, bounds.y);
    const float x1 = std::min(box.x + box.w, bounds.x + bounds.w);
    const float y1 = std::min(box.y + box.h, bounds.y + bounds.h);
    if (x1 <= x0 || y1 <= y0) {
        box = {};
        return;
    }
    box = {x0, y0, x1 - x0, y1 - y0};
}

void contain_layout(DungeonHudLayout& layout, int width, int height) {
    const float margin = std::min(10.f, std::max(4.f, static_cast<float>(width) * 0.008f));
    const Rect window{0.f, 0.f, static_cast<float>(width), static_cast<float>(height)};
    const float scene_keep = std::max(16.f, std::min(48.f, window.h * 0.2f));
    if (layout.top_band + scene_keep > window.h - margin - 24.f) {
        layout.top_bar.h = std::max(8.f, window.h * 0.16f);
        layout.top_band = layout.top_bar.y + layout.top_bar.h;
    }
    const float board_bottom = window.h - margin;
    const float board_top = std::min(board_bottom - 8.f, std::max(layout.board.y, layout.top_band + scene_keep));
    layout.board.y = board_top;
    layout.board.h = std::max(1.f, board_bottom - board_top);
    layout.bottom_band = window.h - layout.board.y;
    const Rect top_inner{
        layout.top_bar.x + 2.f,
        layout.top_bar.y + 2.f,
        std::max(1.f, layout.top_bar.w - 4.f),
        std::max(1.f, layout.top_bar.h - 4.f),
    };
    const Rect board_inner{
        layout.board.x + 2.f,
        layout.board.y + 2.f,
        std::max(1.f, layout.board.w - 4.f),
        std::max(1.f, layout.board.h - 4.f),
    };
    clip_box(layout.room_name, top_inner);
    clip_box(layout.place_label, top_inner);
    clip_box(layout.track, top_inner);
    clip_box(layout.mark, layout.track);
    clip_box(layout.party_label, board_inner);
    clip_box(layout.skills_label, board_inner);
    clip_box(layout.skills_note, board_inner);
    clip_box(layout.pack_label, board_inner);
    clip_box(layout.hint, board_inner);
    clip_box(layout.map_label, board_inner);
    clip_box(layout.map, board_inner);
    for (int index = 0; index < layout.map_room_count; ++index) {
        clip_box(layout.map_rooms[index], layout.map);
        clip_box(layout.map_digits[index], layout.map_rooms[index]);
    }
    for (int index = 0; index < layout.map_tile_count; ++index) {
        clip_box(layout.map_tiles[index], layout.map);
    }
    int current = -1;
    for (int index = 0; index < layout.map_room_count; ++index) {
        if (layout.map_room_ids[index] == layout.map_current) {
            current = index;
            break;
        }
    }
    if (current >= 0) {
        clip_box(layout.map_mark, layout.map_rooms[current]);
    } else {
        layout.map_mark = {};
    }
    for (int index = 0; index < kPartySlotCount; ++index) {
        clip_box(layout.cards[index], board_inner);
        clip_box(layout.swatches[index], layout.cards[index]);
        clip_box(layout.names[index], layout.cards[index]);
        clip_box(layout.fronts[index], layout.cards[index]);
    }
    for (Rect& slot : layout.skills) {
        clip_box(slot, board_inner);
    }
    for (Rect& slot : layout.pack_slots) {
        clip_box(slot, board_inner);
    }
    clip_box(layout.top_bar, window);
    clip_box(layout.board, window);
}

bool has_area(Rect rect) {
    return rect.w > 0.f && rect.h > 0.f;
}

}  // namespace

DungeonHudLayout layout_dungeon_hud(
    int width, int height, int text_scale, float position, std::string_view room_name, const int* ranks, const DungeonMap* map) {
    DungeonHudLayout empty;
    if (width <= 0 || height <= 0) {
        return empty;
    }
    const int scale = clamp_scale(text_scale);
    const int title_scale = scale + 1;
    const int front = ranks == nullptr ? -1 : front_slot(ranks);
    const float line = static_cast<float>(ui::text_height(scale));
    Fit attempts[] = {
        {true, true, true, std::max(8.f, static_cast<float>(scale) * 4.f), std::max(6.f, static_cast<float>(scale) * 2.f),
         std::max(28.f, line * 1.7f), 2, 0.f},
        {true, true, false, std::max(8.f, static_cast<float>(scale) * 3.f), std::max(4.f, static_cast<float>(scale) * 2.f),
         std::max(22.f, line * 1.4f), 2, 0.f},
        {true, false, false, std::max(6.f, static_cast<float>(scale) * 2.f), std::max(4.f, static_cast<float>(scale) * 1.5f),
         std::max(18.f, line * 1.2f), 2, 0.f},
        {false, false, false, std::max(4.f, static_cast<float>(scale) * 2.f), std::max(4.f, static_cast<float>(scale)), std::max(16.f, line), 2, 0.f},
        {false, false, false, 4.f, 4.f, std::max(12.f, line * 0.8f), 1, 0.f},
    };
    if (map != nullptr && map->rooms != nullptr && map->room_count > 0) {
        const float full = map_well_height(*map, scale);
        const float portion[] = {1.f, 0.84f, 0.7f, 0.54f, 0.4f};
        for (int attempt = 0; attempt < 5; ++attempt) {
            attempts[attempt].map_h = std::max(20.f, full * portion[attempt]);
        }
    }
    DungeonHudLayout chosen;
    bool found = false;
    for (const Fit& fit : attempts) {
        chosen = build_layout(width, height, scale, title_scale, fit, position, room_name, front, map);
        if (leaves_a_scene(chosen, height)) {
            found = true;
            break;
        }
    }
    if (!found) {
        chosen = build_layout(width, height, scale, title_scale, attempts[4], position, room_name, front, map);
        contain_layout(chosen, width, height);
    }
    return chosen;
}

float DungeonHud::top_band(int width, int height, int text_scale) const {
    return layout_dungeon_hud(width, height, text_scale, 0.f, "Tutorial", nullptr, map_).top_band;
}

float DungeonHud::bottom_band(int width, int height, int text_scale) const {
    return layout_dungeon_hud(width, height, text_scale, 0.f, "Tutorial", nullptr, map_).bottom_band;
}

void DungeonHud::draw(
    ICanvas& canvas, std::string_view room_name, const int* ranks, float position, int text_scale, bool high_contrast) {
    if (canvas.width() <= 0 || canvas.height() <= 0) {
        return;
    }
    const ui::Theme theme = ui::theme_from(text_scale, high_contrast);
    const DungeonHudLayout layout =
        layout_dungeon_hud(canvas.width(), canvas.height(), text_scale, position, room_name, ranks, map_);
    const Color well = high_contrast ? theme.button : kWell;
    const Color empty_swatch = high_contrast ? theme.button : kEmptySwatch;
    const Color hall = high_contrast ? theme.button_edge : kHall;

    canvas.fill_rect(layout.top_bar, theme.panel);
    canvas.fill_rect(layout.board, theme.panel);
    canvas.fill_rect({layout.board.x, layout.board.y, layout.board.w, std::min(4.f, layout.board.h)}, theme.status);
    ui::stroke_rect(canvas, layout.top_bar, theme.panel_edge, 2);
    ui::stroke_rect(canvas, layout.board, theme.panel_edge, 2);
    if (layout.board.w > 16.f && layout.board.h > 16.f) {
        ui::stroke_rect(
            canvas, {layout.board.x + 5.f, layout.board.y + 6.f, layout.board.w - 10.f, layout.board.h - 12.f}, theme.button_edge, 1);
    }

    canvas.fill_rect(layout.track, well);
    ui::stroke_rect(canvas, layout.track, theme.button_edge, 1);
    canvas.fill_rect(layout.mark, theme.status);
    ui::draw_text_box(canvas, layout.room_name, room_name, theme.title, theme.title_scale, ui::TextAlign::Left, false);
    ui::draw_text_box(canvas, layout.place_label, kHudPlace, theme.hint, theme.text_scale, ui::TextAlign::Left, false);

    ui::draw_text_box(canvas, layout.party_label, kHudParty, theme.title, theme.text_scale, ui::TextAlign::Left, false);
    for (int index = 0; index < kPartySlotCount; ++index) {
        const bool front = index == layout.front_slot;
        canvas.fill_rect(layout.cards[index], front ? theme.selected : theme.button);
        ui::stroke_rect(canvas, layout.cards[index], front ? theme.selected_edge : theme.button_edge, front ? 3 : 1);
        const ClassDef* def = find_class(ranks == nullptr ? 0 : ranks[index]);
        const Color swatch = def == nullptr ? empty_swatch : def->color;
        canvas.fill_rect(layout.swatches[index], swatch);
        ui::stroke_rect(canvas, layout.swatches[index], theme.panel_edge, 1);
        const char digit[2] = {static_cast<char>('1' + index), '\0'};
        ui::draw_text_box(
            canvas, layout.swatches[index], digit, def == nullptr ? theme.label : ink_for(swatch), theme.text_scale, ui::TextAlign::Center, true);
        ui::draw_text_box(
            canvas, layout.names[index], def == nullptr ? kHudEmpty : def->name, theme.label, theme.text_scale, ui::TextAlign::Center, true);
        if (layout.fronts[index].h > 0.f) {
            ui::draw_text_box(canvas, layout.fronts[index], kHudFront, theme.status, theme.text_scale, ui::TextAlign::Center, true);
        }
    }

    ui::draw_text_box(canvas, layout.skills_label, kHudSkills, theme.label, theme.text_scale, ui::TextAlign::Left, true);
    for (int index = 0; index < kHudSkillSlots; ++index) {
        canvas.fill_rect(layout.skills[index], well);
        ui::stroke_rect(canvas, layout.skills[index], theme.button_edge, 1);
        const char digit[2] = {static_cast<char>('1' + index), '\0'};
        ui::draw_text_box(canvas, layout.skills[index], digit, theme.hint, theme.text_scale, ui::TextAlign::Center, true);
    }
    ui::draw_text_box(canvas, layout.skills_note, kHudSkillsNote, theme.hint, theme.text_scale, ui::TextAlign::Left, false);
    if (layout.pack_label.h > 0.f) {
        ui::draw_text_box(canvas, layout.pack_label, kHudPack, theme.label, theme.text_scale, ui::TextAlign::Left, true);
        for (const Rect& slot : layout.pack_slots) {
            canvas.fill_rect(slot, well);
            ui::stroke_rect(canvas, slot, theme.button_edge, 1);
        }
    }
    if (has_area(layout.map)) {
        ui::draw_text_box(canvas, layout.map_label, kHudMap, theme.label, theme.text_scale, ui::TextAlign::Left, true);
        canvas.fill_rect(layout.map, well);
        ui::stroke_rect(canvas, layout.map, theme.button_edge, 1);
        for (int index = 0; index < layout.map_tile_count; ++index) {
            if (!has_area(layout.map_tiles[index])) {
                continue;
            }
            canvas.fill_rect(layout.map_tiles[index], hall);
        }
        for (int index = 0; index < layout.map_room_count; ++index) {
            if (!has_area(layout.map_rooms[index])) {
                continue;
            }
            const bool current = layout.map_room_ids[index] == layout.map_current;
            const Color fill = current ? theme.status : theme.button;
            canvas.fill_rect(layout.map_rooms[index], fill);
            ui::stroke_rect(canvas, layout.map_rooms[index], current ? theme.panel_edge : theme.button_edge, current ? 2 : 1);
            if (layout.map_digits[index].h > 0.f && layout.map_room_ids[index] >= 0 && layout.map_room_ids[index] <= 9) {
                const char digit[2] = {static_cast<char>('0' + layout.map_room_ids[index]), '\0'};
                ui::draw_text_box(
                    canvas, layout.map_digits[index], digit, ink_for(fill), theme.text_scale, ui::TextAlign::Center, true);
            }
        }
        if (has_area(layout.map_mark)) {
            canvas.fill_rect(layout.map_mark, kDarkInk);
        }
    }
    ui::draw_text_box(canvas, layout.hint, kHudHint, theme.hint, theme.text_scale, ui::TextAlign::Left, false);
}

}  // namespace lla
