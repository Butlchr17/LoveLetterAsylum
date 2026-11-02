#pragma once

#include "lla/contracts.hpp"

#include "xcb_window.hpp"

#include <memory>

namespace lla {

class Renderer {
public:
    explicit Renderer(XcbWindow& window);
    ~Renderer();

    Renderer(const Renderer&) = delete;
    Renderer& operator=(const Renderer&) = delete;

    // Resizes the swapchain when the window size changed, then clears the
    // draw list. The returned canvas is valid until the next begin_frame.
    ICanvas& begin_frame(int window_width, int window_height);
    bool present();

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

}  // namespace lla
