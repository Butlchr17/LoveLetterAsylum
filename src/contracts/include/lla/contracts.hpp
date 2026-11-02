#pragma once

#include <cstdint>
#include <string_view>
#include <utility>
#include <vector>
#include <memory>

namespace lla {

struct Color {
    std::uint8_t r = 0;
    std::uint8_t g = 0;
    std::uint8_t b = 0;
    std::uint8_t a = 255;
};

struct Rect {
    float x = 0;
    float y = 0;
    float w = 0;
    float h = 0;

    bool contains(float px, float py) const {
        return px >= x && py >= y && px < x + w && py < y + h;
    }
};

// Draw target for one frame. Coordinates are pixels, origin at the top left.
class ICanvas {
public:
    virtual ~ICanvas() = default;
    virtual int width() const = 0;
    virtual int height() const = 0;
    virtual void clear(Color color) = 0;
    virtual void fill_rect(Rect rect, Color color) = 0;
};

struct InputFrame {
    bool up = false;
    bool down = false;
    bool confirm = false;
    bool mouse_moved = false;
    bool mouse_pressed = false;
    bool quit = false;
    bool left = false;
    bool right = false;
    bool left_pressed = false;
    bool right_pressed = false;
    bool pause = false;
    int key_code = 0;
    float mouse_x = -1;
    float mouse_y = -1;
    int window_width = 0;
    int window_height = 0;
    float dt = 0;
};

// Escape still closes menus after Pause is rebound.
inline constexpr int kMenuBackKey = 9;

struct KeyBinds {
    int move_left = 113;
    int move_left_alt = 38;
    int move_right = 114;
    int move_right_alt = 40;
    int menu_up = 111;
    int menu_up_alt = 25;
    int menu_down = 116;
    int menu_down_alt = 39;
    int confirm = 36;
    int confirm_alt = 65;
    int pause = 9;
};

struct DeviceSettings {
    bool fullscreen = false;
    int width = 1280;
    int height = 720;
    bool mute = false;
    int master = 100;
    int music = 100;
    int effects = 100;
    int menu_volume = 100;
    int text_scale = 2;
    bool high_contrast = false;
    KeyBinds binds{};
};

class IDisplayApplier {
public:
    virtual ~IDisplayApplier() = default;
    virtual void apply_display(bool fullscreen, int width, int height) = 0;
    virtual int screen_width() const = 0;
    virtual int screen_height() const = 0;
};

class IDeviceSettings {
public:
    virtual ~IDeviceSettings() = default;
    virtual const DeviceSettings& get() const = 0;
    virtual void replace(const DeviceSettings& next) = 0;
    virtual void preview_display(bool fullscreen, int width, int height) = 0;
    virtual void commit_display(bool fullscreen, int width, int height) = 0;
    virtual void bind_display(IDisplayApplier* applier) = 0;
    virtual int screen_width() const = 0;
    virtual int screen_height() const = 0;
};

class ISettingsScreen {
public:
    virtual ~ISettingsScreen() = default;
    virtual bool is_open() const = 0;
    virtual void open() = 0;
    virtual void close() = 0;
};

enum class AppCommand : std::uint8_t {
    None,
    StartNewGame,
    Continue,
    OpenOptions,
    OpenCredits,
    Quit,
};

constexpr std::string_view command_name(AppCommand command) {
    switch (command) {
        case AppCommand::None:
            return "None";
        case AppCommand::StartNewGame:
            return "StartNewGame";
        case AppCommand::Continue:
            return "Continue";
        case AppCommand::OpenOptions:
            return "OpenOptions";
        case AppCommand::OpenCredits:
            return "OpenCredits";
        case AppCommand::Quit:
            return "Quit";
    }
    return "Unknown";
}

// One playthrough file per slot. Slots are numbered from 1.
class ISaveSlots {
public:
    static constexpr int kSlotCount = 5;

    virtual ~ISaveSlots() = default;
    virtual bool occupied(int slot) const = 0;
    virtual bool start_new_game(int slot) = 0;
    // position_millis is 0 at the left edge and 1000 at the right edge.
    virtual bool load_progress(int slot, int& position_millis) const = 0;
    virtual bool store_progress(int slot, int position_millis) = 0;
};

// Which full-screen mode is showing. Modules draw only their own phase.
enum class PlayPhase : std::uint8_t {
    Frontend,
    Intro,
    Tutorial,
};

class IPlayPhase {
public:
    virtual ~IPlayPhase() = default;
    virtual PlayPhase phase() const = 0;
    virtual int active_slot() const = 0;
    virtual float resume_position() const = 0;
    virtual void begin_intro(int slot) = 0;
    virtual void begin_tutorial() = 0;
    virtual void continue_game(int slot, float position) = 0;
    virtual void return_to_menu() = 0;
    virtual void note_position(float position) = 0;
};

class PlayPhaseState final : public IPlayPhase {
public:
    PlayPhase phase() const override { return phase_; }
    int active_slot() const override { return slot_; }
    float resume_position() const override { return position_; }
    void begin_intro(int slot) override {
        slot_ = slot;
        position_ = 0.5f;
        phase_ = PlayPhase::Intro;
    }
    void begin_tutorial() override { phase_ = PlayPhase::Tutorial; }
    void continue_game(int slot, float position) override {
        slot_ = slot;
        position_ = position < 0.f ? 0.f : (position > 1.f ? 1.f : position);
        phase_ = PlayPhase::Tutorial;
    }
    void return_to_menu() override { phase_ = PlayPhase::Frontend; }
    void note_position(float position) override {
        position_ = position < 0.f ? 0.f : (position > 1.f ? 1.f : position);
    }

private:
    PlayPhase phase_ = PlayPhase::Frontend;
    int slot_ = 0;
    float position_ = 0.5f;
};

class IModule {
public:
    virtual ~IModule() = default;
    virtual std::string_view name() const = 0;
    virtual void startup() {}
    virtual void shutdown() {}
    virtual void update(const InputFrame&) {}
    virtual void draw(ICanvas&) {}
    virtual AppCommand poll_command() { return AppCommand::None; }
};

class ModuleRegistry {
public:
    template <typename T, typename... Args>
    T& add(Args&&... args) {
        auto owned = std::make_unique<T>(std::forward<Args>(args)...);
        T& ref = *owned;
        modules_.push_back(std::move(owned));
        return ref;
    }

    void startup_all() {
        for (const auto& module : modules_) {
            module->startup();
        }
    }

    void shutdown_all() {
        for (auto it = modules_.rbegin(); it != modules_.rend(); ++it) {
            (*it)->shutdown();
        }
    }

    void update_all(const InputFrame& input) {
        for (const auto& module : modules_) {
            module->update(input);
        }
    }

    void draw_all(ICanvas& canvas) {
        for (const auto& module : modules_) {
            module->draw(canvas);
        }
    }

    AppCommand poll_command() {
        for (const auto& module : modules_) {
            if (const AppCommand command = module->poll_command(); command != AppCommand::None) {
                return command;
            }
        }
        return AppCommand::None;
    }

private:
    std::vector<std::unique_ptr<IModule>> modules_;
};

}  // namespace lla
