#include "lla/tutorial/follow.hpp"
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

    if (!near(lla::follow_distance(100.f), 140.f) || !near(lla::follow_distance(10.f), 18.f) || lla::follow_distance(0.f) != 0.f) {
        return fail("the follow distance was not a short gap past the body");
    }
    if (lla::held_facing(1, true, false) != -1 || lla::held_facing(-1, false, true) != 1 || lla::held_facing(-1, false, false) != -1 ||
        lla::held_facing(1, true, true) != 1) {
        return fail("facing did not follow the held direction");
    }
    const float body = 10.f;
    const float body_w = 20.f;
    const float part = 14.f;
    const float part_w = 6.f;
    if (!near(lla::mirrored_x(body, body_w, part, part_w, 1), 24.f) || !near(lla::mirrored_x(body, body_w, part, part_w, -1), 10.f)) {
        return fail("a body part did not flip across its vertical center");
    }

    const int pair_ranks[4] = {1, 0, 0, 2};
    const float step = 50.f;
    lla::FollowLine pair = lla::arrange_follow(pair_ranks, 200.f, 1, step);
    if (pair.count != 2 || pair.members[0].slot != 0 || !near(pair.members[0].x, 150.f) || pair.members[0].facing != 1 ||
        pair.members[1].slot != 3 || !near(pair.members[1].x, 200.f) || pair.members[1].facing != 1) {
        return fail("two characters did not start one follow distance apart");
    }
    pair = lla::advance_follow(pair_ranks, 190.f, -1, step, pair);
    if (!near(pair.members[0].x, 150.f) || pair.members[0].facing != 1 || !near(pair.members[1].x, 190.f) || pair.members[1].facing != -1) {
        return fail("the party followed before the leader had passed them");
    }
    pair = lla::advance_follow(pair_ranks, 100.f, -1, step, pair);
    if (!near(pair.members[0].x, 150.f) || pair.members[0].facing != -1 || !near(pair.members[1].x, 100.f)) {
        return fail("the follower did not turn once the leader was one follow distance past");
    }
    pair = lla::advance_follow(pair_ranks, 90.f, -1, step, pair);
    if (!near(pair.members[0].x, 140.f) || !near(pair.members[1].x, 90.f) || pair.members[0].facing != -1) {
        return fail("the follower did not keep one follow distance behind the leader");
    }

    const int trio_ranks[4] = {1, 2, 3, 0};
    lla::FollowLine trio = lla::arrange_follow(trio_ranks, 300.f, 1, 40.f);
    if (trio.count != 3 || !near(trio.members[0].x, 220.f) || !near(trio.members[1].x, 260.f) || !near(trio.members[2].x, 300.f)) {
        return fail("three characters were not spaced from character to character");
    }
    for (int step_index = 1; step_index <= 20; ++step_index) {
        trio = lla::advance_follow(trio_ranks, 300.f + static_cast<float>(step_index), 1, 40.f, trio);
    }
    if (!near(trio.members[0].x, 240.f) || !near(trio.members[1].x, 280.f) || !near(trio.members[2].x, 320.f)) {
        return fail("followers did not keep their own gap while walking right");
    }
    const float back_start = trio.members[0].x;
    const float middle_start = trio.members[1].x;
    float leader = trio.members[2].x;
    bool middle_turned = false;
    bool back_moved = false;
    for (int step_index = 0; step_index < 400; ++step_index) {
        leader -= 1.f;
        const float middle_before = trio.members[1].x;
        const float back_before = trio.members[0].x;
        trio = lla::advance_follow(trio_ranks, leader, -1, 40.f, trio);
        if (!middle_turned) {
            if (trio.members[1].x != middle_before || trio.members[1].facing == -1) {
                if (!(leader <= middle_start - 40.f) || trio.members[0].x != back_before) {
                    return fail("the middle character moved before the leader had passed by one follow distance");
                }
                middle_turned = true;
            } else if (trio.members[0].x != back_before) {
                return fail("the back character moved before the middle character");
            }
        } else if (!back_moved && trio.members[0].x != back_before) {
            if (!(trio.members[1].x <= back_start - 40.f)) {
                return fail("the back character followed the leader instead of the character ahead");
            }
            back_moved = true;
            break;
        }
    }
    if (!middle_turned || !back_moved) {
        return fail("the party did not pick up the new direction one character at a time");
    }
    if (!near(trio.members[1].x - trio.members[2].x, 40.f) || !near(trio.members[0].x - trio.members[1].x, 40.f)) {
        return fail("the follow distance was not kept between each character");
    }
    return 0;
}
