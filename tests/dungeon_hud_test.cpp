#include "lla/dungeon_ui/hud.hpp"
#include "lla/ui/text.hpp"

#include <algorithm>
#include <cmath>
#include <iostream>
#include <string>
#include <string_view>
#include <vector>

namespace {

int fail(const std::string& message) {
    std::cerr << message << '\n';
    return 1;
}

bool near(float actual, float expected) {
    return std::fabs(actual - expected) < 0.05f;
}

bool inside(lla::Rect outer, lla::Rect inner) {
    if (inner.w <= 0.f || inner.h <= 0.f) {
        return true;
    }
    return inner.x >= outer.x - 0.05f && inner.y >= outer.y - 0.05f && inner.x + inner.w <= outer.x + outer.w + 0.05f &&
           inner.y + inner.h <= outer.y + outer.h + 0.05f;
}

bool overlaps(lla::Rect a, lla::Rect b) {
    return a.w > 0.5f && a.h > 0.5f && b.w > 0.5f && b.h > 0.5f && a.x < b.x + b.w - 0.05f && b.x < a.x + a.w - 0.05f &&
           a.y < b.y + b.h - 0.05f && b.y < a.y + a.h - 0.05f;
}

bool lines_fit(std::string_view text, int scale, lla::Rect area) {
    if (text.empty() || area.h <= 0.f) {
        return true;
    }
    const int width = std::max(1, static_cast<int>(std::floor(area.w)));
    std::vector<std::string_view> lines;
    lla::ui::wrap_lines(text, scale, width, lines);
    const int block = lla::ui::text_block_height(static_cast<int>(lines.size()), scale);
    if (static_cast<float>(block) > area.h + 0.05f) {
        return false;
    }
    for (std::string_view line : lines) {
        const int line_w = lla::ui::text_width(line, scale);
        if (line_w > width && line.size() > 1) {
            return false;
        }
    }
    return true;
}

std::vector<lla::Rect> text_boxes(const lla::DungeonHudLayout& layout) {
    std::vector<lla::Rect> boxes;
    boxes.push_back(layout.room_name);
    boxes.push_back(layout.place_label);
    boxes.push_back(layout.party_label);
    boxes.push_back(layout.skills_label);
    boxes.push_back(layout.skills_note);
    boxes.push_back(layout.pack_label);
    boxes.push_back(layout.hint);
    boxes.push_back(layout.map_label);
    for (int index = 0; index < layout.map_room_count; ++index) {
        boxes.push_back(layout.map_digits[index]);
    }
    for (const lla::Rect& box : layout.names) {
        boxes.push_back(box);
    }
    for (const lla::Rect& box : layout.fronts) {
        boxes.push_back(box);
    }
    for (const lla::Rect& box : layout.skills) {
        boxes.push_back(box);
    }
    return boxes;
}

}  // namespace

int main() {
    const int ranks[4] = {1, 0, 0, 2};
    const lla::DungeonHudLayout wide = lla::layout_dungeon_hud(1280, 720, 2, 0.f, "Tutorial", ranks);
    if (wide.front_slot != 3 || wide.fronts[3].h <= 0.f || wide.fronts[0].h > 0.f || wide.pack_label.h <= 0.f) {
        return fail("the wide board did not mark the rightmost color or show the pack");
    }
    const int back[4] = {5, 0, 1, 0};
    const lla::DungeonHudLayout back_layout = lla::layout_dungeon_hud(1280, 720, 2, 1.f, "Tutorial", back);
    if (back_layout.front_slot != 2) {
        return fail("the front slot was not the rightmost occupied color");
    }
    const lla::DungeonHudLayout at_left = lla::layout_dungeon_hud(1280, 720, 2, 0.f, "Tutorial", ranks);
    const lla::DungeonHudLayout at_right = lla::layout_dungeon_hud(1280, 720, 2, 1.f, "Tutorial", ranks);
    if (!near(at_left.mark.x, at_left.track.x) || !near(at_right.mark.x + at_right.mark.w, at_right.track.x + at_right.track.w)) {
        return fail("the place mark did not travel the track");
    }
    if (lla::layout_dungeon_hud(1280, 720, 2, 0.5f, "Tutorial", nullptr).front_slot != -1) {
        return fail("an empty party reported a front character");
    }
    const lla::DungeonHudLayout bare = lla::layout_dungeon_hud(1280, 720, 2, 0.4f, "Tutorial", ranks, nullptr);
    if (bare.map.h != 0.f || bare.map_room_count != 0 || bare.map_tile_count != 0) {
        return fail("a dungeon without a chart still reserved a map");
    }
    if (lla::layout_dungeon_hud(0, 720, 2, 0.f, "Tutorial", ranks, &lla::kTutorialMap).top_band != 0.f) {
        return fail("a zero width window produced a chart");
    }

    const auto room_with_id = [](const lla::DungeonHudLayout& layout, int id) {
        for (int index = 0; index < layout.map_room_count; ++index) {
            if (layout.map_room_ids[index] == id) {
                return layout.map_rooms[index];
            }
        }
        return lla::Rect{};
    };
    const lla::DungeonHudLayout walk_start = lla::layout_dungeon_hud(1280, 720, 2, 0.f, "Tutorial", ranks, &lla::kTutorialMap);
    const lla::DungeonHudLayout walk_end = lla::layout_dungeon_hud(1280, 720, 2, 1.f, "Tutorial", ranks, &lla::kTutorialMap);
    const lla::Rect start_room = room_with_id(walk_start, 1);
    const lla::Rect end_room = room_with_id(walk_end, 1);
    if (walk_start.map_current != 1 || start_room.w <= 0.f || !inside(start_room, walk_start.map_mark) ||
        !inside(end_room, walk_end.map_mark) || !near(walk_start.map_mark.x, start_room.x) ||
        !near(walk_end.map_mark.x + walk_end.map_mark.w, end_room.x + end_room.w) || walk_start.map_mark.x >= walk_end.map_mark.x) {
        return fail("the room pip did not travel inside room 1");
    }
    lla::DungeonHud hud{&lla::kTutorialMap};
    const lla::DungeonHudLayout bands = lla::layout_dungeon_hud(1280, 720, 3, 0.f, "Tutorial", nullptr, &lla::kTutorialMap);
    if (bands.top_band <= 0.f || bands.bottom_band <= 0.f || !near(hud.top_band(1280, 720, 3), bands.top_band) ||
        !near(hud.bottom_band(1280, 720, 3), bands.bottom_band)) {
        return fail("the dungeon chart insets did not match the board");
    }

    const int scales[] = {2, 3, 4};
    const int windows[][2] = {{1280, 720}, {2560, 1440}, {320, 240}};
    const lla::DungeonMap* charts[] = {nullptr, &lla::kTutorialMap};
    for (const lla::DungeonMap* chart : charts) {
        const char* chart_name = chart == nullptr ? "no map" : "tutorial map";
        for (int scale : scales) {
        for (const auto& window : windows) {
            const int width = window[0];
            const int height = window[1];
            const bool full_text = width >= 1280 && height >= 720;
            const lla::DungeonHudLayout layout = lla::layout_dungeon_hud(width, height, scale, 0.35f, "Tutorial", ranks, chart);
            const lla::Rect screen{0.f, 0.f, static_cast<float>(width), static_cast<float>(height)};
            const std::string where = std::to_string(width) + "x" + std::to_string(height) + " scale " + std::to_string(scale) + " " + chart_name;
            if (!inside(screen, layout.top_bar) || !inside(screen, layout.board) || overlaps(layout.top_bar, layout.board)) {
                return fail("the dungeon board left the window at " + where);
            }
            if (layout.top_band + layout.bottom_band >= static_cast<float>(height) - 8.f) {
                return fail("the dungeon board covered the room at " + where);
            }
            if (!inside(layout.top_bar, layout.room_name) || !inside(layout.top_bar, layout.place_label) ||
                !inside(layout.top_bar, layout.track) || !inside(layout.track, layout.mark)) {
                return fail("the top chart left its bar at " + where);
            }
            if (!inside(layout.board, layout.party_label) || !inside(layout.board, layout.hint) || !inside(layout.board, layout.skills_note)) {
                return fail("board text left the board at " + where);
            }
            for (int index = 0; index < 4; ++index) {
                if (!inside(layout.board, layout.cards[index]) || !inside(layout.cards[index], layout.swatches[index]) ||
                    !inside(layout.cards[index], layout.names[index]) || !inside(layout.cards[index], layout.fronts[index]) ||
                    overlaps(layout.swatches[index], layout.names[index]) || overlaps(layout.names[index], layout.fronts[index])) {
                    return fail("a party card left its chart at " + where);
                }
                if (!inside(layout.board, layout.skills[index])) {
                    return fail("a skill plate left the board at " + where);
                }
            }
            for (const lla::Rect& slot : layout.pack_slots) {
                if (layout.pack_label.h > 0.f && !inside(layout.board, slot)) {
                    return fail("a pack cell left the board at " + where);
                }
            }
            if (layout.map.h > 0.f &&
                (!inside(layout.board, layout.map) || !inside(layout.board, layout.map_label) || overlaps(layout.map, layout.hint) ||
                 overlaps(layout.map_label, layout.map))) {
                return fail("the map left the board at " + where);
            }
            for (int index = 0; index < layout.map_room_count; ++index) {
                if (!inside(layout.map, layout.map_rooms[index]) || !inside(layout.map_rooms[index], layout.map_digits[index])) {
                    return fail("a chart room left the map at " + where);
                }
            }
            for (int index = 0; index < layout.map_tile_count; ++index) {
                if (!inside(layout.map, layout.map_tiles[index])) {
                    return fail("a hall tile left the map at " + where);
                }
            }
            const lla::Rect current_room = room_with_id(layout, layout.map_current);
            if (layout.map_mark.h > 0.f && !inside(current_room, layout.map_mark)) {
                return fail("the room pip left its room at " + where);
            }
            const std::vector<lla::Rect> boxes = text_boxes(layout);
            for (const lla::Rect& box : boxes) {
                if (!inside(screen, box)) {
                    return fail("text left the window at " + where);
                }
            }
            for (std::size_t i = 0; i < boxes.size(); ++i) {
                for (std::size_t j = i + 1; j < boxes.size(); ++j) {
                    if (overlaps(boxes[i], boxes[j])) {
                        return fail("text boxes overlap at " + where);
                    }
                }
            }
            if (!full_text) {
                continue;
            }
            const int title_scale = scale + 1;
            if (layout.fronts[3].h <= 0.f || layout.pack_label.h <= 0.f) {
                return fail("a large window dropped the front word or the pack at " + where);
            }
            if (!lines_fit("Tutorial", title_scale, layout.room_name) || !lines_fit(lla::kHudPlace, scale, layout.place_label) ||
                !lines_fit(lla::kHudParty, scale, layout.party_label) || !lines_fit("Yellow", scale, layout.names[0]) ||
                !lines_fit(lla::kHudFront, scale, layout.fronts[3]) || !lines_fit(lla::kHudSkills, scale, layout.skills_label) ||
                !lines_fit("4", scale, layout.skills[3]) || !lines_fit(lla::kHudSkillsNote, scale, layout.skills_note) ||
                !lines_fit(lla::kHudPack, scale, layout.pack_label) || !lines_fit(lla::kHudHint, scale, layout.hint)) {
                return fail("text does not fit its chart at " + where);
            }
            if (chart == nullptr) {
                if (layout.map.h != 0.f || layout.map_room_count != 0) {
                    return fail("a large window invented a map at " + where);
                }
                continue;
            }
            const float right_gap = (layout.board.x + layout.board.w) - (layout.map.x + layout.map.w);
            if (layout.map_room_count != 5 || layout.map_tile_count != 8 || layout.map_current != 1 || layout.map.h <= 0.f ||
                layout.map.y + 0.05f < layout.skills_note.y + layout.skills_note.h || layout.map.y + layout.map.h > layout.hint.y + 0.05f ||
                right_gap < 3.f || right_gap > 40.f || layout.map.x < layout.board.x + layout.board.w * 0.4f ||
                !lines_fit(lla::kHudMap, scale, layout.map_label)) {
                return fail("the tutorial chart is not in the bottom right at " + where);
            }
            int digits = 0;
            for (int index = 0; index < layout.map_room_count; ++index) {
                if (layout.map_rooms[index].w + 0.05f < layout.map_tiles[0].w) {
                    return fail("a room square was smaller than a hall tile at " + where);
                }
                for (int other = index + 1; other < layout.map_room_count; ++other) {
                    if (overlaps(layout.map_rooms[index], layout.map_rooms[other])) {
                        return fail("chart rooms overlap at " + where);
                    }
                }
                for (int tile = 0; tile < layout.map_tile_count; ++tile) {
                    if (overlaps(layout.map_rooms[index], layout.map_tiles[tile])) {
                        return fail("a hall tile covers a room at " + where);
                    }
                }
                if (layout.map_digits[index].h <= 0.f) {
                    continue;
                }
                ++digits;
                const char text[2] = {static_cast<char>('0' + layout.map_room_ids[index]), '\0'};
                if (layout.map_room_ids[index] < 1 || layout.map_room_ids[index] > 5 || !lines_fit(text, scale, layout.map_digits[index])) {
                    return fail("a room digit does not fit at " + where);
                }
            }
            if (digits != 5 || layout.map_mark.h <= 0.f) {
                return fail("the tutorial chart dropped a room number or the pip at " + where);
            }
        }
        }
    }
    return 0;
}
