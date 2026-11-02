#include "lla/platform/vulkan_host.hpp"

#include "renderer.hpp"
#include "xcb_window.hpp"

#include <chrono>
#include <iostream>
#include <thread>

namespace lla {
namespace {

struct ModuleLifetime {
    ModuleRegistry& modules;

    explicit ModuleLifetime(ModuleRegistry& registry) : modules(registry) { modules.startup_all(); }

    ~ModuleLifetime() { modules.shutdown_all(); }
};

struct DisplayBinding {
    IDeviceSettings* devices = nullptr;

    DisplayBinding(IDeviceSettings* settings, IDisplayApplier& applier) : devices(settings) {
        if (devices == nullptr) {
            return;
        }
        devices->bind_display(&applier);
        const DeviceSettings& saved = devices->get();
        applier.apply_display(saved.fullscreen, saved.width, saved.height);
    }

    ~DisplayBinding() {
        if (devices != nullptr) {
            devices->bind_display(nullptr);
        }
    }
};

}  // namespace

int run_vulkan_host(ModuleRegistry& modules, IDeviceSettings* devices) {
    XcbWindow window(1280, 720, "Love Letter Asylum");
    const DisplayBinding binding(devices, window);
    Renderer renderer(window);
    const ModuleLifetime lifetime(modules);
    bool announced = false;
    auto previous_frame = std::chrono::steady_clock::now();

    while (true) {
        InputFrame input;
        const KeyBinds binds = devices != nullptr ? devices->get().binds : KeyBinds{};
        if (!window.pump(input, binds)) {
            break;
        }
        const auto now = std::chrono::steady_clock::now();
        const float frame_dt = std::chrono::duration<float>(now - previous_frame).count();
        previous_frame = now;
        input.dt = frame_dt < 0.f ? 0.f : (frame_dt > 0.1f ? 0.1f : frame_dt);
        if (input.quit) {
            break;
        }

        ICanvas& canvas = renderer.begin_frame(window.width(), window.height());
        input.window_width = canvas.width();
        input.window_height = canvas.height();
        if (input.window_width > 0 && input.window_height > 0) {
            modules.update_all(input);
            const AppCommand command = modules.poll_command();
            if (command != AppCommand::None) {
                std::cout << "menu command: " << command_name(command) << '\n' << std::flush;
            }
            if (command == AppCommand::Quit) {
                break;
            }
            modules.draw_all(canvas);
        }

        if (!renderer.present()) {
            std::this_thread::sleep_for(std::chrono::milliseconds(16));
            continue;
        }
        if (!announced) {
            std::cout << "Love Letter Asylum main menu is ready.\n" << std::flush;
            announced = true;
        }
    }
    return 0;
}

}  // namespace lla
