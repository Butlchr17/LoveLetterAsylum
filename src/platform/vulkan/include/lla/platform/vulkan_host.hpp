#pragma once

#include "lla/contracts.hpp"

namespace lla {

// Opens a window and presents the registered modules with Vulkan.
// Returns 0 when the player closes the window or chooses Quit.
// Device settings, when present, choose the window and the bound keys.
int run_vulkan_host(ModuleRegistry& modules, IDeviceSettings* devices = nullptr);

}  // namespace lla
