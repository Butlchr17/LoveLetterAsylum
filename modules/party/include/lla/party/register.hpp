#pragma once

#include "lla/contracts.hpp"

namespace lla {

void register_party(
    ModuleRegistry& registry,
    ISaveSlots* saves,
    IPlayPhase* phase,
    ISettingsScreen** settings = nullptr,
    IDeviceSettings* devices = nullptr);

}  // namespace lla
