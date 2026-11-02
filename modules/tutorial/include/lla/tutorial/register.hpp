#pragma once

#include "lla/contracts.hpp"

namespace lla {

void register_tutorial(
    ModuleRegistry& registry,
    IPlayPhase* phase,
    ISaveSlots* saves = nullptr,
    ISettingsScreen** settings = nullptr,
    IDeviceSettings* devices = nullptr);

}  // namespace lla
