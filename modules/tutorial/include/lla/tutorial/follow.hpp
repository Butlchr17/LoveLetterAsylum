#pragma once

#include "lla/party/party.hpp"

namespace lla {

// One body in the walking line. x is the left edge. facing is +1 right, -1 left.
// The controlled character is the rightmost occupied slot, kept last in this list.
struct FollowMember {
    int slot = 0;
    int class_id = 0;
    float x = 0.f;
    int facing = 1;
};

struct FollowLine {
    FollowMember members[kPartySlotCount]{};
    int count = 0;
};

// Distance from one character's left edge to the next character's left edge.
// Empty slots do not add another gap. The extra past the body width is a short
// gap so the bodies stay readable.
inline float follow_distance(float character_w) {
    if (!(character_w > 0.f)) {
        return 0.f;
    }
    const float gap = character_w * 0.4f > 8.f ? character_w * 0.4f : 8.f;
    return character_w + gap;
}

// Last direction of travel. Holding neither key, or both, keeps the current facing.
inline int held_facing(int current, bool left, bool right) {
    if (left && !right) {
        return -1;
    }
    if (right && !left) {
        return 1;
    }
    return current == -1 ? -1 : 1;
}

// Map a part drawn for a right-facing body onto the body. Facing left mirrors it
// across the body's vertical center.
inline float mirrored_x(float body_x, float body_w, float local_x, float local_w, int facing) {
    if (facing >= 0) {
        return body_x + local_x;
    }
    return body_x + (body_w - local_x - local_w);
}

// Line up occupied characters behind the controlled character. Back is index 0.
inline FollowLine arrange_follow(const int* ranks, float leader_x, int facing, float step) {
    FollowLine line;
    if (ranks == nullptr || !(step >= 0.f)) {
        return line;
    }
    if (facing != -1) {
        facing = 1;
    }
    int slots[kPartySlotCount];
    int ids[kPartySlotCount];
    int count = 0;
    for (int slot = 0; slot < kPartySlotCount; ++slot) {
        if (!is_starter_class(ranks[slot])) {
            continue;
        }
        slots[count] = slot;
        ids[count] = ranks[slot];
        ++count;
    }
    line.count = count;
    float x = leader_x;
    for (int index = count - 1; index >= 0; --index) {
        line.members[index].slot = slots[index];
        line.members[index].class_id = ids[index];
        line.members[index].x = x;
        line.members[index].facing = facing;
        x -= static_cast<float>(facing) * step;
    }
    return line;
}

inline bool follow_roster_same(const FollowLine& line, const int* ranks) {
    if (ranks == nullptr) {
        return line.count == 0;
    }
    int count = 0;
    for (int slot = 0; slot < kPartySlotCount; ++slot) {
        if (!is_starter_class(ranks[slot])) {
            continue;
        }
        if (count >= line.count || line.members[count].slot != slot) {
            return false;
        }
        ++count;
    }
    return count == line.count;
}

// Move the controlled character to leader_x. Each other character stays put until
// the one ahead has gone one follow distance past it, then that character turns
// and keeps the same distance behind the one ahead.
inline FollowLine advance_follow(
    const int* ranks, float leader_x, int leader_facing, float step, const FollowLine& previous) {
    if (!follow_roster_same(previous, ranks) || !(step >= 0.f)) {
        return arrange_follow(ranks, leader_x, leader_facing, step);
    }
    if (leader_facing != -1) {
        leader_facing = 1;
    }
    FollowLine line = previous;
    if (line.count <= 0) {
        return line;
    }
    constexpr float kPass = 0.001f;
    const int leader = line.count - 1;
    line.members[leader].x = leader_x;
    line.members[leader].facing = leader_facing;
    line.members[leader].class_id = ranks[line.members[leader].slot];
    for (int index = leader - 1; index >= 0; --index) {
        const FollowMember ahead = line.members[index + 1];
        const FollowMember standing = previous.members[index];
        const float needed = standing.x + static_cast<float>(ahead.facing) * step;
        const bool passed = ahead.facing > 0 ? ahead.x + kPass >= needed : ahead.x - kPass <= needed;
        line.members[index].slot = standing.slot;
        line.members[index].class_id = ranks[standing.slot];
        if (passed) {
            line.members[index].x = ahead.x - static_cast<float>(ahead.facing) * step;
            line.members[index].facing = ahead.facing;
        } else {
            line.members[index].x = standing.x;
            line.members[index].facing = standing.facing == -1 ? -1 : 1;
        }
    }
    return line;
}

}  // namespace lla
