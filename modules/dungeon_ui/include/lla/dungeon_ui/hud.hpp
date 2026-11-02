#pragma once

#include "lla/contracts.hpp"

#include <string_view>

namespace lla {

// In-dungeon expedition board. Darkest Dungeon's dungeon screen also has a light
// meter, quest banner, health, stress, enemy ranks, and combat actions. Those stay
// off this board until the game has those mechanics. The board does draw a room
// chart: larger squares for rooms, smaller squares for the halls between them.
// The chart does not move the party from room to room.
// Rank 1 is the left slot. Rank 4 is the right slot. The front character is the
// rightmost occupied slot.
inline constexpr const char* kHudPlace = "Place";
inline constexpr const char* kHudParty = "Party";
inline constexpr const char* kHudFront = "Front";
inline constexpr const char* kHudSkills = "Skills";
inline constexpr const char* kHudSkillsNote = "No skills yet";
inline constexpr const char* kHudPack = "Pack";
inline constexpr const char* kHudEmpty = "Empty";
inline constexpr const char* kHudMap = "Map";
inline constexpr const char* kHudHint = "Arrows or A/D move    Esc pause";
inline constexpr int kHudSkillSlots = 4;
inline constexpr int kHudPackSlots = 8;
inline constexpr int kMaxMappedRooms = 16;
inline constexpr int kMaxHallTiles = 48;

struct DungeonRoom {
    int id = 0;
    int column = 0;
    int row = 0;
};

// Orthogonal hall. segments is how many small squares sit between the two rooms.
struct DungeonHall {
    int from_id = 0;
    int to_id = 0;
    int segments = 2;
};

struct DungeonMap {
    const DungeonRoom* rooms = nullptr;
    int room_count = 0;
    const DungeonHall* halls = nullptr;
    int hall_count = 0;
    int current_room = 0;
};

// Placeholder tutorial chart. Only room 1 is playable. The links are a stand-in
// until a real layout is chosen:
//   1 - 2 - 3
//       |
//       4 - 5
// Another dungeon passes its own DungeonMap. Change this table to change the shape.
inline constexpr DungeonRoom kTutorialRooms[] = {
    {1, 0, 0},
    {2, 1, 0},
    {3, 2, 0},
    {4, 1, 1},
    {5, 2, 1},
};
inline constexpr DungeonHall kTutorialHalls[] = {
    {1, 2, 2},
    {2, 3, 2},
    {2, 4, 2},
    {4, 5, 2},
};
inline constexpr DungeonMap kTutorialMap{kTutorialRooms, 5, kTutorialHalls, 4, 1};

struct DungeonHudLayout {
    float top_band = 0.f;
    float bottom_band = 0.f;
    int front_slot = -1;
    Rect top_bar{};
    Rect room_name{};
    Rect place_label{};
    Rect track{};
    Rect mark{};
    Rect board{};
    Rect party_label{};
    Rect cards[4]{};
    Rect swatches[4]{};
    Rect names[4]{};
    Rect fronts[4]{};
    Rect skills_label{};
    Rect skills[kHudSkillSlots]{};
    Rect skills_note{};
    Rect pack_label{};
    Rect pack_slots[kHudPackSlots]{};
    Rect hint{};
    Rect map_label{};
    Rect map{};
    Rect map_rooms[kMaxMappedRooms]{};
    Rect map_digits[kMaxMappedRooms]{};
    int map_room_ids[kMaxMappedRooms]{};
    int map_room_count = 0;
    Rect map_tiles[kMaxHallTiles]{};
    int map_tile_count = 0;
    Rect map_mark{};
    int map_current = 0;
};

// A null map leaves the board without a chart. The same layout drives the room
// insets, so the scene and the board stay aligned.
DungeonHudLayout layout_dungeon_hud(
    int width,
    int height,
    int text_scale,
    float position,
    std::string_view room_name,
    const int* ranks,
    const DungeonMap* map = nullptr);

class DungeonHud final : public IDungeonHud {
public:
    explicit DungeonHud(const DungeonMap* map = nullptr) : map_(map) {}

    float top_band(int width, int height, int text_scale) const override;
    float bottom_band(int width, int height, int text_scale) const override;
    void draw(
        ICanvas& canvas, std::string_view room_name, const int* ranks, float position, int text_scale, bool high_contrast) override;

private:
    const DungeonMap* map_ = nullptr;
};

}  // namespace lla
