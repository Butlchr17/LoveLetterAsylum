#include "lla/party/party.hpp"

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
    if (lla::find_class(0) != nullptr || lla::find_class(9) != nullptr) {
        return fail("an empty or unknown id resolved to a class");
    }
    const lla::ClassDef* red = lla::find_class(1);
    const lla::ClassDef* white = lla::find_class(5);
    if (red == nullptr || white == nullptr || red->name[0] != 'R' || white->name[0] != 'W') {
        return fail("starter classes were not found by id");
    }
    if (!lla::is_starter_class(3) || lla::is_starter_class(0) || lla::is_starter_class(9)) {
        return fail("starter class check did not match the table");
    }

    const int ready_a[4] = {1, 0, 0, 2};
    const int ready_b[4] = {0, 4, 5, 0};
    const int one[4] = {1, 0, 0, 0};
    const int three[4] = {1, 2, 3, 0};
    const int duplicate[4] = {1, 0, 0, 1};
    const int unknown[4] = {1, 0, 0, 9};
    const int empty[4] = {0, 0, 0, 0};
    if (!lla::starting_party_ready(ready_a) || !lla::starting_party_ready(ready_b)) {
        return fail("two different starter colors were not ready");
    }
    if (lla::starting_party_ready(one) || lla::starting_party_ready(three) || lla::starting_party_ready(duplicate) ||
        lla::starting_party_ready(unknown) || lla::starting_party_ready(empty)) {
        return fail("a party that is not two different starters was ready");
    }
    if (lla::party_count(unknown) != 1 || lla::party_count(three) != 3 || lla::party_count(empty) != 0) {
        return fail("party count included an empty or unknown slot");
    }

    if (lla::front_slot(ready_a) != 3 || lla::front_slot(one) != 0 || lla::front_slot(ready_b) != 2 || lla::front_slot(empty) != -1 ||
        lla::front_slot(unknown) != 0) {
        return fail("the front slot was not the rightmost starter");
    }

    if (lla::step_rank(empty, 0, 0) != 0 || lla::step_rank(empty, 0, 1) != 1 || lla::step_rank(empty, 0, -1) != 5) {
        return fail("an empty slot did not cycle through the starter colors");
    }
    const int white_front[4] = {5, 0, 0, 0};
    if (lla::step_rank(white_front, 0, 1) != 0 || lla::step_rank(white_front, 0, -1) != 4) {
        return fail("the class cycle did not wrap");
    }
    if (lla::step_rank(one, 1, 1) != 2) {
        return fail("a second slot offered a color that was already used");
    }
    if (lla::step_rank(ready_a, 1, 1) != 0 || lla::step_rank(ready_a, 1, -1) != 0 || lla::step_rank(three, 3, 1) != 0) {
        return fail("a third color was offered");
    }
    if (lla::step_rank(ready_a, 0, 1) != 3 || lla::step_rank(ready_a, 0, -1) != 0) {
        return fail("an occupied slot could not move to another open color or to empty");
    }
    int choices[8] = {};
    if (lla::slot_choices(empty, 0, choices, 1) != 1 || choices[0] != 0) {
        return fail("a short choice list did not keep empty");
    }
    if (lla::slot_choices(empty, 4, choices, 8) != 0 || lla::slot_choices(empty, 0, nullptr, 4) != 0) {
        return fail("an invalid choice query returned values");
    }

    lla::MemberPlace members[4];
    const int placed = lla::place_members(ready_a, 100.f, 10.f, members, 4);
    if (placed != 2 || members[0].slot != 0 || members[0].class_id != 1 || !near(members[0].x, 70.f) || members[1].slot != 3 ||
        members[1].class_id != 2 || !near(members[1].x, 100.f)) {
        return fail("members were not placed back to front with empty slots still taking a step");
    }
    const int gapped[4] = {5, 0, 1, 0};
    const int gapped_count = lla::place_members(gapped, 200.f, 15.f, members, 4);
    if (gapped_count != 2 || members[0].class_id != 5 || !near(members[0].x, 170.f) || members[1].class_id != 1 ||
        !near(members[1].x, 200.f)) {
        return fail("a gap between party slots did not move the back member left");
    }
    if (lla::place_members(empty, 100.f, 10.f, members, 4) != 0 || lla::place_members(ready_a, 100.f, -1.f, members, 4) != 0 ||
        lla::place_members(ready_a, 100.f, 10.f, nullptr, 4) != 0) {
        return fail("an empty party or a bad placement query returned members");
    }
    return 0;
}
