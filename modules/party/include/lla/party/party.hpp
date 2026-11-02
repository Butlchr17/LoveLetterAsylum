#pragma once

#include "lla/contracts.hpp"

#include <cstddef>

namespace lla {

// One class is one stable id. Add a later class by appending a ClassDef.
// Party slots store those ids. 0 is an empty slot. Skills and other class
// fields are not here yet. A later hub roster can fill these same four slots.
struct ClassDef {
    int id = 0;
    const char* name = "";
    Color color{};
};

inline constexpr int kPartySlotCount = 4;
inline constexpr int kStartingPartyCount = 2;

inline constexpr ClassDef kStarterClasses[] = {
    {1, "Red", {176, 48, 48, 255}},
    {2, "Blue", {48, 88, 176, 255}},
    {3, "Green", {42, 140, 72, 255}},
    {4, "Yellow", {196, 164, 48, 255}},
    {5, "White", {214, 218, 226, 255}},
};

inline constexpr int kStarterClassCount = static_cast<int>(sizeof(kStarterClasses) / sizeof(kStarterClasses[0]));

static_assert(kStarterClassCount == 5, "the starting roster is five classes");
static_assert(kPartySlotCount == 4, "a party has four positions");

inline const ClassDef* find_class(int id) {
    if (id <= 0) {
        return nullptr;
    }
    for (const ClassDef& def : kStarterClasses) {
        if (def.id == id) {
            return &def;
        }
    }
    return nullptr;
}

inline bool is_starter_class(int id) {
    return find_class(id) != nullptr;
}

inline int party_count(const int* ranks) {
    int count = 0;
    for (int slot = 0; slot < kPartySlotCount; ++slot) {
        if (is_starter_class(ranks[slot])) {
            ++count;
        }
    }
    return count;
}

// The starting scene is ready when two different starter classes are placed.
inline bool starting_party_ready(const int* ranks) {
    if (party_count(ranks) != kStartingPartyCount) {
        return false;
    }
    for (int slot = 0; slot < kPartySlotCount; ++slot) {
        if (ranks[slot] == 0) {
            continue;
        }
        if (!is_starter_class(ranks[slot])) {
            return false;
        }
        for (int other = slot + 1; other < kPartySlotCount; ++other) {
            if (ranks[other] == ranks[slot]) {
                return false;
            }
        }
    }
    return true;
}

// Rightmost occupied slot. That character is the front of the formation.
inline int front_slot(const int* ranks) {
    for (int slot = kPartySlotCount - 1; slot >= 0; --slot) {
        if (is_starter_class(ranks[slot])) {
            return slot;
        }
    }
    return -1;
}

// Legal values for one slot. Empty is always included. A class already used
// in another slot is not. A third class is not offered once two are placed.
inline int slot_choices(const int* ranks, int slot, int* out, int out_cap) {
    if (slot < 0 || slot >= kPartySlotCount || out == nullptr || out_cap <= 0) {
        return 0;
    }
    int count = 0;
    out[count++] = 0;
    int others = 0;
    for (int index = 0; index < kPartySlotCount; ++index) {
        if (index != slot && is_starter_class(ranks[index])) {
            ++others;
        }
    }
    const bool can_add = others < kStartingPartyCount;
    const int current = ranks[slot];
    for (const ClassDef& def : kStarterClasses) {
        bool used_elsewhere = false;
        for (int index = 0; index < kPartySlotCount; ++index) {
            if (index != slot && ranks[index] == def.id) {
                used_elsewhere = true;
                break;
            }
        }
        if (used_elsewhere) {
            continue;
        }
        if (def.id != current && !can_add) {
            continue;
        }
        if (count >= out_cap) {
            break;
        }
        out[count++] = def.id;
    }
    return count;
}

inline int step_rank(const int* ranks, int slot, int direction) {
    int choices[1 + kStarterClassCount] = {};
    const int count = slot_choices(ranks, slot, choices, 1 + kStarterClassCount);
    if (count <= 0 || direction == 0) {
        return slot >= 0 && slot < kPartySlotCount ? ranks[slot] : 0;
    }
    int index = 0;
    const int current = ranks[slot];
    for (int choice = 0; choice < count; ++choice) {
        if (choices[choice] == current) {
            index = choice;
            break;
        }
    }
    const int shift = direction > 0 ? 1 : -1;
    const int next = (index + shift + count) % count;
    return choices[next];
}

struct MemberPlace {
    int slot = 0;
    int class_id = 0;
    float x = 0.f;
};

// Places occupied members from back to front. The front stands at front_x.
// Each slot closer to the back is one step further left, including empty slots.
inline int place_members(const int* ranks, float front_x, float step, MemberPlace* out, int out_cap) {
    const int front = front_slot(ranks);
    if (front < 0 || out == nullptr || out_cap <= 0 || !(step >= 0.f)) {
        return 0;
    }
    int count = 0;
    for (int slot = 0; slot <= front && count < out_cap; ++slot) {
        if (!is_starter_class(ranks[slot])) {
            continue;
        }
        out[count].slot = slot;
        out[count].class_id = ranks[slot];
        out[count].x = front_x - static_cast<float>(front - slot) * step;
        ++count;
    }
    return count;
}

}  // namespace lla
