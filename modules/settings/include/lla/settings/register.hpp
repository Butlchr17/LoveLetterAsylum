#pragma once

#include "lla/contracts.hpp"

namespace lla {

void register_settings(ModuleRegistry& registry, IDeviceSettings* devices, IPlayPhase* phase, ISettingsScreen*& screen);

}  // namespace lla
