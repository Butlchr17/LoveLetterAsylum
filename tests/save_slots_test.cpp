#include "lla/saves/save_slots.hpp"

#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>
#include <unistd.h>

namespace {

namespace fs = std::filesystem;

struct TempDir {
    fs::path path;

    explicit TempDir(fs::path directory) : path(std::move(directory)) {
        fs::remove_all(path);
    }

    ~TempDir() {
        std::error_code error;
        fs::remove_all(path, error);
    }
};

std::string read_file(const fs::path& path) {
    std::ifstream file(path, std::ios::binary);
    return std::string((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
}

int fail(const char* message) {
    std::cerr << message << '\n';
    return 1;
}

}  // namespace

int main() {
    const TempDir directory(fs::temp_directory_path() / ("lla-save-slots-" + std::to_string(::getpid())));
    lla::JsonSaveSlots saves(directory.path);

    for (int slot = 1; slot <= lla::ISaveSlots::kSlotCount; ++slot) {
        if (saves.occupied(slot)) {
            return fail("a fresh directory reported an occupied slot");
        }
    }

    if (!saves.start_new_game(3)) {
        return fail("start_new_game(3) failed");
    }
    const std::string expected_slot_3 =
        "{\n  \"slot\": 3,\n  \"started\": true,\n  \"position\": 500,\n"
        "  \"rank1\": 0,\n  \"rank2\": 0,\n  \"rank3\": 0,\n  \"rank4\": 0\n}\n";
    if (read_file(directory.path / "slot-3.json") != expected_slot_3) {
        return fail("slot-3.json text did not match");
    }
    if (fs::exists(directory.path / "slot-3.json.tmp")) {
        return fail("temporary save file was left behind");
    }

    for (int slot = 1; slot <= lla::ISaveSlots::kSlotCount; ++slot) {
        if (saves.occupied(slot) != (slot == 3)) {
            return fail("occupied state did not match the written slot");
        }
    }

    lla::SaveGame game;
    if (!saves.read_game(3, game) || game.slot != 3 || !game.started || game.position_millis != 500 || game.ranks[0] != 0 ||
        game.ranks[1] != 0 || game.ranks[2] != 0 || game.ranks[3] != 0) {
        return fail("read_game did not return the started slot");
    }
    int loaded = 0;
    if (!saves.load_progress(3, loaded) || loaded != 500) {
        return fail("load_progress did not default to the center");
    }
    if (!saves.store_progress(3, 250) || !saves.load_progress(3, loaded) || loaded != 250) {
        return fail("store_progress did not keep the position");
    }
    if (!saves.store_progress(3, 500)) {
        return fail("store_progress could not restore the center");
    }

    {
        std::ofstream old_file(directory.path / "slot-3.json", std::ios::binary | std::ios::trunc);
        old_file << "old";
    }
    if (!saves.start_new_game(3)) {
        return fail("overwriting slot 3 failed");
    }
    if (read_file(directory.path / "slot-3.json") != expected_slot_3) {
        return fail("overwrite did not replace the save text");
    }

    const int stored_party[4] = {1, 0, 0, 2};
    if (!saves.store_party(3, stored_party)) {
        return fail("store_party failed");
    }
    if (!saves.store_progress(3, 250)) {
        return fail("store_progress failed after a party was stored");
    }
    int round_trip[4] = {-1, -1, -1, -1};
    int round_position = 0;
    if (!saves.load_party(3, round_trip) || round_trip[0] != 1 || round_trip[1] != 0 || round_trip[2] != 0 || round_trip[3] != 2) {
        return fail("store_progress wiped the party");
    }
    if (!saves.load_progress(3, round_position) || round_position != 250) {
        return fail("store_party did not keep the later position");
    }
    const int negative_party[4] = {1, -1, 0, 2};
    if (saves.store_party(3, negative_party)) {
        return fail("a negative rank was stored");
    }
    if (!saves.load_party(3, round_trip) || round_trip[0] != 1 || round_trip[1] != 0 || round_trip[2] != 0 || round_trip[3] != 2) {
        return fail("a rejected party write changed the save");
    }
    const int future_class[4] = {9, 0, 0, 1};
    if (!saves.store_party(3, future_class) || !saves.load_party(3, round_trip) || round_trip[0] != 9 || round_trip[1] != 0 ||
        round_trip[2] != 0 || round_trip[3] != 1) {
        return fail("an unknown class id was not stored");
    }
    if (!saves.load_progress(3, round_position) || round_position != 250) {
        return fail("storing a party changed the position");
    }

    if (saves.start_new_game(0) || saves.start_new_game(6)) {
        return fail("a slot outside 1..5 was accepted");
    }
    if (fs::exists(directory.path / "slot-0.json") || fs::exists(directory.path / "slot-6.json")) {
        return fail("an out-of-range slot created a file");
    }
    if (saves.occupied(0) || saves.occupied(6) || saves.read_game(0, game) || saves.read_game(6, game)) {
        return fail("an out-of-range slot was readable");
    }

    {
        std::ofstream garbage(directory.path / "slot-2.json", std::ios::binary | std::ios::trunc);
        garbage << "not json";
    }
    if (!saves.occupied(2) || saves.read_game(2, game)) {
        return fail("a corrupt file was not occupied, or it parsed");
    }
    if (!saves.start_new_game(2) || !saves.read_game(2, game) || game.slot != 2 || !game.started) {
        return fail("starting a game did not replace the corrupt file");
    }

    {
        std::ofstream extra(directory.path / "slot-4.json", std::ios::binary | std::ios::trunc);
        extra << "{\n  \"note\": \"hello\",\n  \"slot\": \"no\",\n  \"started\": false,\n"
                 "  \"slot\": 4,\n  \"started\": true\n}\n";
    }
    if (!saves.read_game(4, game) || game.slot != 4 || !game.started || game.position_millis != 500) {
        return fail("extra keys or an earlier duplicate changed the record");
    }
    int missing_position = 0;
    if (!saves.load_progress(4, missing_position) || missing_position != 500) {
        return fail("a save without a position did not resume at the center");
    }
    int missing_ranks[4] = {-1, -1, -1, -1};
    if (!saves.load_party(4, missing_ranks) || missing_ranks[0] != 0 || missing_ranks[1] != 0 || missing_ranks[2] != 0 ||
        missing_ranks[3] != 0) {
        return fail("a save without party fields did not load empty ranks");
    }

    {
        std::ofstream mismatch(directory.path / "slot-1.json", std::ios::binary | std::ios::trunc);
        mismatch << "{\"slot\": \"1\", \"started\": true}\n";
    }
    if (!saves.occupied(1) || saves.read_game(1, game)) {
        return fail("a type mismatch was accepted");
    }

    {
        std::ofstream negative(directory.path / "slot-1.json", std::ios::binary | std::ios::trunc);
        negative << "{\"slot\": 1, \"started\": true, \"rank2\": -3}\n";
    }
    int rejected_ranks[4] = {};
    if (!saves.occupied(1) || saves.read_game(1, game) || saves.load_party(1, rejected_ranks)) {
        return fail("a negative rank was accepted");
    }
    {
        std::ofstream text_rank(directory.path / "slot-1.json", std::ios::binary | std::ios::trunc);
        text_rank << "{\"slot\": 1, \"started\": true, \"rank4\": \"Red\"}\n";
    }
    if (saves.read_game(1, game)) {
        return fail("a rank that is not an int was accepted");
    }

    {
        std::ofstream wrong_slot(directory.path / "slot-5.json", std::ios::binary | std::ios::trunc);
        wrong_slot << "{\"slot\": 1, \"started\": true}\n";
    }
    if (!saves.occupied(5) || saves.read_game(5, game)) {
        return fail("a save whose slot field names another slot was accepted");
    }

    return 0;
}
