#pragma once

#include "lla/contracts.hpp"

namespace lla {

void register_intro(ModuleRegistry& registry, IPlayPhase* phase, IDeviceSettings* devices = nullptr);

}  // namespace lla
