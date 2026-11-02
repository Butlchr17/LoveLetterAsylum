#pragma once

#include "lla/contracts.hpp"

namespace lla {

void register_frontend(
    ModuleRegistry& registry,
    ISaveSlots* saves = nullptr,
    IPlayPhase* phase = nullptr,
    ISettingsScreen** settings = nullptr,
    IDeviceSettings* devices = nullptr);

}  // namespace lla
