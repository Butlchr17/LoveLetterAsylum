#pragma once

#include "lla/contracts.hpp"

#include <filesystem>

namespace lla {

// Whole game record for one slot. Fields grow only when a mechanic stores them.
struct SaveGame {
    int slot = 0;
    bool started = false;
    int position_millis = 500;
};

class JsonSaveSlots final : public ISaveSlots {
public:
    explicit JsonSaveSlots(std::filesystem::path directory);

    bool occupied(int slot) const override;
    bool start_new_game(int slot) override;
    bool load_progress(int slot, int& position_millis) const override;
    bool store_progress(int slot, int position_millis) override;
    bool read_game(int slot, SaveGame& game) const;

private:
    std::filesystem::path path_for(int slot) const;
    bool write_slot(int slot, int position_millis);

    std::filesystem::path directory_;
};

}  // namespace lla
