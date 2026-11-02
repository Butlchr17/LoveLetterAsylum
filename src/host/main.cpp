#include "lla/frontend/register.hpp"
#include "lla/intro/register.hpp"
#include "lla/platform/vulkan_host.hpp"
#include "lla/saves/save_slots.hpp"
#include "lla/settings/device_settings.hpp"
#include "lla/settings/register.hpp"
#include "lla/tutorial/register.hpp"

#include <exception>
#include <iostream>

int main() {
    std::cout << "Love Letter Asylum\n"
              << "Up and Down select a row. Enter confirms. Click a row.\n"
              << "New Game asks for one of five save slots, then plays the intro.\n"
              << "Continue resumes a saved slot in the tutorial room.\n"
              << "Options changes display, audio, controls, and access.\n"
              << "Audio levels are saved. No sound plays yet.\n"
              << "Arrows or A/D move. Escape pauses.\n"
              << "Quit, or the window close button, exits.\n"
              << std::flush;
    try {
        lla::JsonSaveSlots saves{"saves"};
        lla::JsonDeviceSettings devices{"settings/device.json"};
        lla::PlayPhaseState phase;
        lla::ISettingsScreen* settings_screen = nullptr;
        lla::ModuleRegistry registry;
        lla::register_frontend(registry, &saves, &phase, &settings_screen, &devices);
        lla::register_intro(registry, &phase, &devices);
        lla::register_tutorial(registry, &phase, &saves, &settings_screen, &devices);
        lla::register_settings(registry, &devices, &phase, settings_screen);
        return lla::run_vulkan_host(registry, &devices);
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
