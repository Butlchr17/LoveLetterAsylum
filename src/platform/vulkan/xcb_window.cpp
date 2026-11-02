#include "xcb_window.hpp"

#include <dlfcn.h>

#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <memory>
#include <stdexcept>
#include <string>
#include <vector>

namespace lla {
namespace {

[[noreturn]] void fail(const std::string& message) {
    throw std::runtime_error(message);
}

// Minimal XCB ABI used to open a window. The development headers are not
// installed; these layouts match libxcb 1.8 on x86-64.
struct xcb_connection_t;
struct xcb_setup_t;

struct xcb_screen_t {
    std::uint32_t root;
    std::uint32_t default_colormap;
    std::uint32_t white_pixel;
    std::uint32_t black_pixel;
    std::uint32_t current_input_masks;
    std::uint16_t width_in_pixels;
    std::uint16_t height_in_pixels;
    std::uint16_t width_in_millimeters;
    std::uint16_t height_in_millimeters;
    std::uint16_t min_installed_maps;
    std::uint16_t max_installed_maps;
    std::uint32_t root_visual;
    std::uint8_t backing_stores;
    std::uint8_t save_unders;
    std::uint8_t root_depth;
    std::uint8_t allowed_depths_len;
};

struct xcb_screen_iterator_t {
    xcb_screen_t* data;
    int rem;
    int index;
};

struct xcb_generic_event_t {
    std::uint8_t response_type;
    std::uint8_t pad0;
    std::uint16_t sequence;
    std::uint32_t length;
};

struct xcb_input_event_t {
    std::uint8_t response_type;
    std::uint8_t detail;
    std::uint16_t sequence;
    std::uint32_t time;
    std::uint32_t root;
    std::uint32_t event;
    std::uint32_t child;
    std::int16_t root_x;
    std::int16_t root_y;
    std::int16_t event_x;
    std::int16_t event_y;
    std::uint16_t state;
    std::uint8_t same_screen;
    std::uint8_t pad0;
};

struct xcb_configure_notify_event_t {
    std::uint8_t response_type;
    std::uint8_t pad0;
    std::uint16_t sequence;
    std::uint32_t event;
    std::uint32_t window;
    std::uint32_t above_sibling;
    std::int16_t x;
    std::int16_t y;
    std::uint16_t width;
    std::uint16_t height;
    std::uint16_t border_width;
    std::uint8_t override_redirect;
    std::uint8_t pad1;
};

struct xcb_client_message_event_t {
    std::uint8_t response_type;
    std::uint8_t format;
    std::uint16_t sequence;
    std::uint32_t window;
    std::uint32_t type;
    std::uint32_t data32[5];
};

struct xcb_void_cookie_t {
    unsigned int sequence;
};

struct xcb_intern_atom_cookie_t {
    unsigned int sequence;
};

struct xcb_intern_atom_reply_t {
    std::uint8_t response_type;
    std::uint8_t pad0;
    std::uint16_t sequence;
    std::uint32_t length;
    std::uint32_t atom;
};

struct xcb_generic_error_t {
    std::uint8_t response_type;
    std::uint8_t error_code;
    std::uint16_t sequence;
    std::uint32_t resource_id;
    std::uint16_t minor_code;
    std::uint8_t major_code;
    std::uint8_t pad0;
};

static_assert(sizeof(xcb_screen_t) == 40);
static_assert(sizeof(xcb_screen_iterator_t) == 16);
static_assert(sizeof(xcb_input_event_t) == 32);
static_assert(sizeof(xcb_configure_notify_event_t) == 28);
static_assert(sizeof(xcb_client_message_event_t) == 32);

constexpr std::uint32_t kEventMask = 1u | 2u | 4u | 64u | (1u << 17) | (1u << 21);
constexpr std::uint32_t kBackPixel = 2u;
constexpr std::uint32_t kEventMaskBit = 2048u;
constexpr std::uint16_t kConfigureGeometry = 1u | 2u | 4u | 8u;
constexpr std::uint32_t kSubstructureMask = (1u << 19) | (1u << 20);
constexpr std::uint8_t kKeyKpEnter = 104;

struct XcbProcs {
    void* library = nullptr;
    xcb_connection_t* (*connect)(const char*, int*) = nullptr;
    void (*disconnect)(xcb_connection_t*) = nullptr;
    int (*has_error)(xcb_connection_t*) = nullptr;
    const xcb_setup_t* (*get_setup)(xcb_connection_t*) = nullptr;
    xcb_screen_iterator_t (*roots)(const xcb_setup_t*) = nullptr;
    void (*next_screen)(xcb_screen_iterator_t*) = nullptr;
    std::uint32_t (*generate_id)(xcb_connection_t*) = nullptr;
    xcb_void_cookie_t (*create_window)(
        xcb_connection_t*, std::uint8_t, std::uint32_t, std::uint32_t, std::int16_t, std::int16_t,
        std::uint16_t, std::uint16_t, std::uint16_t, std::uint16_t, std::uint32_t, std::uint32_t, const void*) = nullptr;
    xcb_void_cookie_t (*map_window)(xcb_connection_t*, std::uint32_t) = nullptr;
    xcb_void_cookie_t (*destroy_window)(xcb_connection_t*, std::uint32_t) = nullptr;
    int (*flush)(xcb_connection_t*) = nullptr;
    xcb_generic_event_t* (*poll_event)(xcb_connection_t*) = nullptr;
    xcb_intern_atom_cookie_t (*intern_atom)(xcb_connection_t*, std::uint8_t, std::uint16_t, const char*) = nullptr;
    xcb_intern_atom_reply_t* (*intern_atom_reply)(xcb_connection_t*, xcb_intern_atom_cookie_t, xcb_generic_error_t**) = nullptr;
    xcb_void_cookie_t (*change_property)(
        xcb_connection_t*, std::uint8_t, std::uint32_t, std::uint32_t, std::uint32_t, std::uint8_t, std::uint32_t, const void*) = nullptr;
    xcb_void_cookie_t (*configure_window)(xcb_connection_t*, std::uint32_t, std::uint16_t, const void*) = nullptr;
    xcb_void_cookie_t (*send_event)(xcb_connection_t*, std::uint8_t, std::uint32_t, std::uint32_t, const char*) = nullptr;
};

template <typename T>
T load_symbol(void* library, const char* name) {
    void* symbol = dlsym(library, name);
    if (symbol == nullptr) {
        fail(std::string("Missing libxcb symbol ") + name);
    }
    return reinterpret_cast<T>(symbol);
}

XcbProcs load_xcb() {
    XcbProcs procs;
    procs.library = dlopen("libxcb.so.1", RTLD_NOW | RTLD_LOCAL);
    if (procs.library == nullptr) {
        fail("Could not load libxcb.so.1");
    }
    procs.connect = load_symbol<decltype(procs.connect)>(procs.library, "xcb_connect");
    procs.disconnect = load_symbol<decltype(procs.disconnect)>(procs.library, "xcb_disconnect");
    procs.has_error = load_symbol<decltype(procs.has_error)>(procs.library, "xcb_connection_has_error");
    procs.get_setup = load_symbol<decltype(procs.get_setup)>(procs.library, "xcb_get_setup");
    procs.roots = load_symbol<decltype(procs.roots)>(procs.library, "xcb_setup_roots_iterator");
    procs.next_screen = load_symbol<decltype(procs.next_screen)>(procs.library, "xcb_screen_next");
    procs.generate_id = load_symbol<decltype(procs.generate_id)>(procs.library, "xcb_generate_id");
    procs.create_window = load_symbol<decltype(procs.create_window)>(procs.library, "xcb_create_window");
    procs.map_window = load_symbol<decltype(procs.map_window)>(procs.library, "xcb_map_window");
    procs.destroy_window = load_symbol<decltype(procs.destroy_window)>(procs.library, "xcb_destroy_window");
    procs.flush = load_symbol<decltype(procs.flush)>(procs.library, "xcb_flush");
    procs.poll_event = load_symbol<decltype(procs.poll_event)>(procs.library, "xcb_poll_for_event");
    procs.intern_atom = load_symbol<decltype(procs.intern_atom)>(procs.library, "xcb_intern_atom");
    procs.intern_atom_reply = load_symbol<decltype(procs.intern_atom_reply)>(procs.library, "xcb_intern_atom_reply");
    procs.change_property = load_symbol<decltype(procs.change_property)>(procs.library, "xcb_change_property");
    procs.configure_window = load_symbol<decltype(procs.configure_window)>(procs.library, "xcb_configure_window");
    procs.send_event = load_symbol<decltype(procs.send_event)>(procs.library, "xcb_send_event");
    return procs;
}

}  // namespace

struct XcbWindow::Impl {
    XcbProcs procs{};
    xcb_connection_t* connection = nullptr;
    std::uint32_t window = 0;
    std::uint32_t protocols = 0;
    std::uint32_t delete_window = 0;
    std::uint32_t root = 0;
    std::uint32_t net_wm_state = 0;
    std::uint32_t net_wm_fullscreen = 0;
    int width = 0;
    int height = 0;
    int screen_w = 0;
    int screen_h = 0;
    float mouse_x = -1;
    float mouse_y = -1;
    bool open = false;
    bool fullscreen = false;
    bool held[256] = {};

    ~Impl() { close(); }

    void close() {
        if (connection != nullptr) {
            if (window != 0) {
                procs.destroy_window(connection, window);
                procs.flush(connection);
            }
            procs.disconnect(connection);
            connection = nullptr;
            window = 0;
        }
        if (procs.library != nullptr) {
            dlclose(procs.library);
            procs.library = nullptr;
        }
        open = false;
    }

    std::uint32_t intern(const char* name) const {
        const auto cookie = procs.intern_atom(connection, 0, static_cast<std::uint16_t>(std::strlen(name)), name);
        xcb_generic_error_t* error = nullptr;
        xcb_intern_atom_reply_t* reply = procs.intern_atom_reply(connection, cookie, &error);
        if (error != nullptr) {
            const int code = error->error_code;
            std::free(error);
            fail("xcb failed to intern atom " + std::string(name) + " (" + std::to_string(code) + ")");
        }
        if (reply == nullptr) {
            fail("xcb failed to intern atom " + std::string(name));
        }
        const std::uint32_t atom = reply->atom;
        std::free(reply);
        return atom;
    }

    void set_title(const char* title) const {
        const std::uint32_t utf8 = intern("UTF8_STRING");
        const std::uint32_t net_name = intern("_NET_WM_NAME");
        const std::uint32_t wm_name = intern("WM_NAME");
        const std::uint32_t string_atom = intern("STRING");
        const auto length = static_cast<std::uint32_t>(std::strlen(title));
        procs.change_property(connection, 0, window, net_name, utf8, 8, length, title);
        procs.change_property(connection, 0, window, wm_name, string_atom, 8, length, title);
    }

    void open_window(int requested_width, int requested_height, const char* title) {
        procs = load_xcb();
        int screen_index = 0;
        connection = procs.connect(nullptr, &screen_index);
        if (connection == nullptr || procs.has_error(connection) != 0) {
            fail("xcb could not open the display. Is DISPLAY set?");
        }

        const xcb_setup_t* setup = procs.get_setup(connection);
        xcb_screen_iterator_t screens = procs.roots(setup);
        for (int index = 0; index < screen_index && screens.data != nullptr; ++index) {
            procs.next_screen(&screens);
        }
        if (screens.data == nullptr) {
            fail("xcb did not report a screen");
        }

        const xcb_screen_t screen = *screens.data;
        root = screen.root;
        screen_w = screen.width_in_pixels;
        screen_h = screen.height_in_pixels;
        window = procs.generate_id(connection);
        const std::uint32_t values[] = {screen.black_pixel, kEventMask};
        procs.create_window(
            connection,
            screen.root_depth,
            window,
            screen.root,
            80,
            80,
            static_cast<std::uint16_t>(requested_width),
            static_cast<std::uint16_t>(requested_height),
            0,
            1,
            screen.root_visual,
            kBackPixel | kEventMaskBit,
            values);

        protocols = intern("WM_PROTOCOLS");
        delete_window = intern("WM_DELETE_WINDOW");
        net_wm_state = intern("_NET_WM_STATE");
        net_wm_fullscreen = intern("_NET_WM_STATE_FULLSCREEN");
        const std::uint32_t atom_type = intern("ATOM");
        procs.change_property(connection, 0, window, protocols, atom_type, 32, 1, &delete_window);
        set_title(title);
        procs.map_window(connection, window);
        procs.flush(connection);
        width = requested_width;
        height = requested_height;
        open = true;
    }

    void configure_geometry(int x, int y, int next_width, int next_height) {
        const std::uint32_t values[] = {
            static_cast<std::uint32_t>(x),
            static_cast<std::uint32_t>(y),
            static_cast<std::uint32_t>(next_width),
            static_cast<std::uint32_t>(next_height),
        };
        procs.configure_window(connection, window, kConfigureGeometry, values);
    }

    // Fullscreen is an EWMH client message. The window manager may then
    // resize the window, and ConfigureNotify keeps the swapchain in step.
    void send_fullscreen(bool enabled) {
        xcb_client_message_event_t event{};
        event.response_type = 33;
        event.format = 32;
        event.window = window;
        event.type = net_wm_state;
        event.data32[0] = enabled ? 1u : 0u;
        event.data32[1] = net_wm_fullscreen;
        event.data32[3] = 1u;
        procs.send_event(connection, 0, root, kSubstructureMask, reinterpret_cast<const char*>(&event));
    }

    void apply_display(bool next_fullscreen, int next_width, int next_height) {
        if (!open || connection == nullptr) {
            return;
        }
        if (next_width < 320) {
            next_width = 320;
        } else if (next_width > 7680) {
            next_width = 7680;
        }
        if (next_height < 240) {
            next_height = 240;
        } else if (next_height > 4320) {
            next_height = 4320;
        }
        if (next_fullscreen != fullscreen) {
            send_fullscreen(next_fullscreen);
            fullscreen = next_fullscreen;
        }
        int x = 0;
        int y = 0;
        if (!next_fullscreen) {
            if (screen_w > next_width) {
                x = (screen_w - next_width) / 2;
            }
            if (screen_h > next_height) {
                y = (screen_h - next_height) / 2;
            }
        }
        configure_geometry(x, y, next_width, next_height);
        width = next_width;
        height = next_height;
        procs.flush(connection);
    }

    bool pump(InputFrame& input, const KeyBinds& binds) {
        input = {};
        input.mouse_x = mouse_x;
        input.mouse_y = mouse_y;
        input.window_width = width;
        input.window_height = height;
        if (!open || connection == nullptr) {
            return false;
        }
        if (procs.has_error(connection) != 0) {
            return false;
        }

        std::vector<xcb_generic_event_t*> events;
        while (xcb_generic_event_t* event = procs.poll_event(connection)) {
            events.push_back(event);
        }

        const auto press_key = [&](std::uint8_t detail, bool repeat) {
            if (detail == binds.menu_up || detail == binds.menu_up_alt) {
                input.up = true;
            } else if (detail == binds.menu_down || detail == binds.menu_down_alt) {
                input.down = true;
            }
            if (detail == binds.confirm || detail == binds.confirm_alt || detail == kKeyKpEnter) {
                input.confirm = true;
            }
            if (!repeat) {
                const bool was_down = held[detail];
                if (!was_down && (detail == binds.move_left || detail == binds.move_left_alt)) {
                    input.left_pressed = true;
                }
                if (!was_down && (detail == binds.move_right || detail == binds.move_right_alt)) {
                    input.right_pressed = true;
                }
                if (!was_down && detail == binds.pause) {
                    input.pause = true;
                }
                input.key_code = detail;
            }
            held[detail] = true;
        };

        for (std::size_t index = 0; index < events.size(); ++index) {
            xcb_generic_event_t* event = events[index];
            const auto type = static_cast<std::uint8_t>(event->response_type & 0x7fu);
            if (type == 2) {
                const auto* key = reinterpret_cast<xcb_input_event_t*>(event);
                press_key(key->detail, false);
            } else if (type == 3) {
                const auto* key = reinterpret_cast<xcb_input_event_t*>(event);
                bool repeat = false;
                if (index + 1 < events.size()) {
                    xcb_generic_event_t* next = events[index + 1];
                    const auto next_type = static_cast<std::uint8_t>(next->response_type & 0x7fu);
                    if (next_type == 2) {
                        const auto* pressed = reinterpret_cast<xcb_input_event_t*>(next);
                        repeat = pressed->detail == key->detail;
                    }
                }
                if (repeat) {
                    const auto* pressed = reinterpret_cast<xcb_input_event_t*>(events[index + 1]);
                    press_key(pressed->detail, true);
                    ++index;
                } else {
                    held[key->detail] = false;
                }
            } else if (type == 4) {
                const auto* button = reinterpret_cast<xcb_input_event_t*>(event);
                mouse_x = button->event_x;
                mouse_y = button->event_y;
                input.mouse_x = mouse_x;
                input.mouse_y = mouse_y;
                input.mouse_moved = true;
                if (button->detail == 1) {
                    input.mouse_pressed = true;
                }
            } else if (type == 6) {
                const auto* motion = reinterpret_cast<xcb_input_event_t*>(event);
                mouse_x = motion->event_x;
                mouse_y = motion->event_y;
                input.mouse_x = mouse_x;
                input.mouse_y = mouse_y;
                input.mouse_moved = true;
            } else if (type == 10) {
                for (bool& down : held) {
                    down = false;
                }
            } else if (type == 22) {
                const auto* configure = reinterpret_cast<xcb_configure_notify_event_t*>(event);
                if (configure->window == window && configure->width > 0 && configure->height > 0) {
                    width = configure->width;
                    height = configure->height;
                    input.window_width = width;
                    input.window_height = height;
                }
            } else if (type == 17 || type == 33) {
                if (type == 33) {
                    const auto* message = reinterpret_cast<xcb_client_message_event_t*>(event);
                    if (message->type == protocols && message->data32[0] == delete_window) {
                        input.quit = true;
                    }
                } else {
                    input.quit = true;
                }
            }
        }
        for (xcb_generic_event_t* event : events) {
            std::free(event);
        }
        const auto held_key = [&](int code) { return code >= 0 && code <= 255 && held[code]; };
        input.left = held_key(binds.move_left) || held_key(binds.move_left_alt);
        input.right = held_key(binds.move_right) || held_key(binds.move_right_alt);
        return procs.has_error(connection) == 0;
    }
};

XcbWindow::XcbWindow(int width, int height, const char* title) : impl_(std::make_unique<Impl>()) {
    impl_->open_window(width, height, title);
}

XcbWindow::~XcbWindow() = default;

void* XcbWindow::connection() const { return impl_->connection; }

std::uint32_t XcbWindow::window_id() const { return impl_->window; }

int XcbWindow::width() const { return impl_->width; }

int XcbWindow::height() const { return impl_->height; }

bool XcbWindow::pump(InputFrame& input, const KeyBinds& binds) { return impl_->pump(input, binds); }

void XcbWindow::apply_display(bool fullscreen, int width, int height) {
    impl_->apply_display(fullscreen, width, height);
}

int XcbWindow::screen_width() const { return impl_->screen_w; }

int XcbWindow::screen_height() const { return impl_->screen_h; }

}  // namespace lla
