#pragma once

#include "lla/contracts.hpp"

#include <cstdint>
#include <memory>

namespace lla {

class XcbWindow : public IDisplayApplier {
public:
    XcbWindow(int width, int height, const char* title);
    ~XcbWindow() override;

    XcbWindow(const XcbWindow&) = delete;
    XcbWindow& operator=(const XcbWindow&) = delete;

    void* connection() const;
    std::uint32_t window_id() const;
    int width() const;
    int height() const;

    // Fills one frame of input from the bound keys. Returns false when the
    // connection has closed.
    bool pump(InputFrame& input, const KeyBinds& binds);

    void apply_display(bool fullscreen, int width, int height) override;
    int screen_width() const override;
    int screen_height() const override;

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

}  // namespace lla
