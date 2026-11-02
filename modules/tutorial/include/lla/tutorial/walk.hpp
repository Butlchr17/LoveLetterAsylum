#pragma once

namespace lla {

// Normalized position across one room. 0 is the left edge and 1 is the right edge.
inline constexpr float kWalkSpeed = 0.5f;

inline float move_walk(float position, bool left, bool right, float dt, float speed) {
    if (position < 0.f) {
        position = 0.f;
    } else if (position > 1.f) {
        position = 1.f;
    }
    if (!(dt > 0.f) || !(speed > 0.f)) {
        return position;
    }
    float direction = 0.f;
    if (left && !right) {
        direction = -1.f;
    } else if (right && !left) {
        direction = 1.f;
    }
    const float next = position + direction * speed * dt;
    if (next < 0.f) {
        return 0.f;
    }
    if (next > 1.f) {
        return 1.f;
    }
    return next;
}

}  // namespace lla
