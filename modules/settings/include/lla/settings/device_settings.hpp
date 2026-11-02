#pragma once

#include "lla/contracts.hpp"

#include <filesystem>

namespace lla {

class JsonDeviceSettings final : public IDeviceSettings {
public:
    explicit JsonDeviceSettings(std::filesystem::path file);

    const DeviceSettings& get() const override;
    void replace(const DeviceSettings& next) override;
    void preview_display(bool fullscreen, int width, int height) override;
    void commit_display(bool fullscreen, int width, int height) override;
    void bind_display(IDisplayApplier* applier) override;
    int screen_width() const override;
    int screen_height() const override;

private:
    bool save() const;
    void apply_current_display() const;

    std::filesystem::path file_;
    DeviceSettings current_{};
    IDisplayApplier* applier_ = nullptr;
};

}  // namespace lla
