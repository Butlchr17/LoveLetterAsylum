#include "lla/tutorial/walk.hpp"

#include <cmath>
#include <iostream>

namespace {

bool near(float actual, float expected) {
    return std::fabs(actual - expected) < 0.0001f;
}

int fail(const char* message) {
    std::cerr << message << '\n';
    return 1;
}

}  // namespace

int main() {
    if (lla::kWalkSpeed != 0.375f) {
        return fail("dungeon walk speed is not 75 percent of 0.5");
    }
    if (!near(lla::move_walk(0.5f, false, false, 0.5f, lla::kWalkSpeed), 0.5f)) {
        return fail("standing still changed position");
    }
    if (!near(lla::move_walk(0.5f, true, true, 0.5f, lla::kWalkSpeed), 0.5f)) {
        return fail("opposite directions did not cancel");
    }
    if (!near(lla::move_walk(0.5f, true, false, 0.f, lla::kWalkSpeed), 0.5f)) {
        return fail("a zero timestep moved the character");
    }
    if (!near(lla::move_walk(0.4f, true, false, 0.2f, 0.5f), 0.3f)) {
        return fail("moving left did not cover speed times time");
    }
    if (!near(lla::move_walk(0.4f, false, true, 0.2f, 0.5f), 0.5f)) {
        return fail("moving right did not cover speed times time");
    }
    if (!near(lla::move_walk(0.05f, true, false, 1.f, 1.f), 0.f)) {
        return fail("the walk did not stop at the left edge");
    }
    if (!near(lla::move_walk(0.95f, false, true, 1.f, 1.f), 1.f)) {
        return fail("the walk did not stop at the right edge");
    }
    if (!near(lla::move_walk(-0.2f, false, false, 0.f, 1.f), 0.f) ||
        !near(lla::move_walk(1.4f, false, false, 0.f, 1.f), 1.f)) {
        return fail("a position outside the room was kept");
    }
    return 0;
}
