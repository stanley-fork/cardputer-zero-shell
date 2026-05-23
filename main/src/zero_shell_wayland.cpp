#include "zero_shell/app_catalog.hpp"
#include "zero_shell/image.hpp"
#include "zero_shell/status.hpp"

#include "xdg-shell-client-protocol.h"

#include <algorithm>
#include <cerrno>
#include <chrono>
#include <cctype>
#include <cstdio>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <ctime>
#include <fcntl.h>
#include <filesystem>
#include <iostream>
#include <linux/input-event-codes.h>
#include <poll.h>
#include <string>
#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/syscall.h>
#include <sys/types.h>
#include <unistd.h>
#include <vector>
#include <wayland-client.h>

#ifndef MFD_CLOEXEC
#define MFD_CLOEXEC 0x0001U
#endif

namespace {

constexpr int kWidth = 320;
constexpr int kHeight = 170;
constexpr int kStride = kWidth * 4;
constexpr int kBufferSize = kStride * kHeight;
constexpr int kBarHeight = 20;

struct Color {
    uint8_t r;
    uint8_t g;
    uint8_t b;
};

constexpr Color kZeroBg{0xE9, 0xE4, 0xD5};
constexpr Color kPanel{0xF4, 0xF0, 0xE6};
constexpr Color kTaskButton{0xEF, 0xE8, 0xD9};
constexpr Color kTile{0xDC, 0xD5, 0xC3};
constexpr Color kTileActive{0xF6, 0xF0, 0xDF};
constexpr Color kIconWell{0xF8, 0xF4, 0xEA};
constexpr Color kLabelStrip{0xEE, 0xE6, 0xD5};
constexpr Color kInk{0x17, 0x17, 0x17};
constexpr Color kLine{0x2A, 0x2A, 0x2A};
constexpr Color kMuted{0x6E, 0x6A, 0x61};
constexpr Color kSoftLine{0xBB, 0xB1, 0x9E};
constexpr Color kGridDot{0xC9, 0xC1, 0xAE};
constexpr Color kAccent{0xE6, 0x6A, 0x2C};
constexpr Color kOk{0x3A, 0x7D, 0x44};
constexpr Color kWarn{0xB9, 0x4A, 0x2C};
constexpr Color kShadow{0xBD, 0xB5, 0xA4};

struct Glyph {
    char ch;
    uint8_t rows[7];
};

struct WindowTask {
    std::string app_id;
    std::string title;
};

bool same_tasks(const std::vector<WindowTask> &left, const std::vector<WindowTask> &right)
{
    if (left.size() != right.size()) {
        return false;
    }
    for (size_t i = 0; i < left.size(); ++i) {
        if (left[i].app_id != right[i].app_id || left[i].title != right[i].title) {
            return false;
        }
    }
    return true;
}

constexpr Glyph kGlyphs[] = {
    {' ', {0x00,0x00,0x00,0x00,0x00,0x00,0x00}},
    {'!', {0x04,0x04,0x04,0x04,0x00,0x04,0x00}},
    {'%', {0x11,0x02,0x04,0x08,0x11,0x00,0x00}},
    {'-', {0x00,0x00,0x00,0x0E,0x00,0x00,0x00}},
    {'_', {0x00,0x00,0x00,0x00,0x00,0x00,0x1F}},
    {'.', {0x00,0x00,0x00,0x00,0x00,0x04,0x00}},
    {'/', {0x01,0x02,0x04,0x08,0x10,0x00,0x00}},
    {':', {0x00,0x04,0x00,0x00,0x04,0x00,0x00}},
    {'<', {0x02,0x04,0x08,0x10,0x08,0x04,0x02}},
    {'>', {0x08,0x04,0x02,0x01,0x02,0x04,0x08}},
    {'0', {0x0E,0x11,0x13,0x15,0x19,0x11,0x0E}},
    {'1', {0x04,0x0C,0x04,0x04,0x04,0x04,0x0E}},
    {'2', {0x0E,0x11,0x01,0x02,0x04,0x08,0x1F}},
    {'3', {0x1F,0x02,0x04,0x02,0x01,0x11,0x0E}},
    {'4', {0x02,0x06,0x0A,0x12,0x1F,0x02,0x02}},
    {'5', {0x1F,0x10,0x1E,0x01,0x01,0x11,0x0E}},
    {'6', {0x06,0x08,0x10,0x1E,0x11,0x11,0x0E}},
    {'7', {0x1F,0x01,0x02,0x04,0x08,0x08,0x08}},
    {'8', {0x0E,0x11,0x11,0x0E,0x11,0x11,0x0E}},
    {'9', {0x0E,0x11,0x11,0x0F,0x01,0x02,0x0C}},
    {'A', {0x0E,0x11,0x11,0x1F,0x11,0x11,0x11}},
    {'B', {0x1E,0x11,0x11,0x1E,0x11,0x11,0x1E}},
    {'C', {0x0E,0x11,0x10,0x10,0x10,0x11,0x0E}},
    {'D', {0x1E,0x11,0x11,0x11,0x11,0x11,0x1E}},
    {'E', {0x1F,0x10,0x10,0x1E,0x10,0x10,0x1F}},
    {'F', {0x1F,0x10,0x10,0x1E,0x10,0x10,0x10}},
    {'G', {0x0E,0x11,0x10,0x17,0x11,0x11,0x0E}},
    {'H', {0x11,0x11,0x11,0x1F,0x11,0x11,0x11}},
    {'I', {0x0E,0x04,0x04,0x04,0x04,0x04,0x0E}},
    {'J', {0x01,0x01,0x01,0x01,0x11,0x11,0x0E}},
    {'K', {0x11,0x12,0x14,0x18,0x14,0x12,0x11}},
    {'L', {0x10,0x10,0x10,0x10,0x10,0x10,0x1F}},
    {'M', {0x11,0x1B,0x15,0x15,0x11,0x11,0x11}},
    {'N', {0x11,0x19,0x15,0x13,0x11,0x11,0x11}},
    {'O', {0x0E,0x11,0x11,0x11,0x11,0x11,0x0E}},
    {'P', {0x1E,0x11,0x11,0x1E,0x10,0x10,0x10}},
    {'Q', {0x0E,0x11,0x11,0x11,0x15,0x12,0x0D}},
    {'R', {0x1E,0x11,0x11,0x1E,0x14,0x12,0x11}},
    {'S', {0x0F,0x10,0x10,0x0E,0x01,0x01,0x1E}},
    {'T', {0x1F,0x04,0x04,0x04,0x04,0x04,0x04}},
    {'U', {0x11,0x11,0x11,0x11,0x11,0x11,0x0E}},
    {'V', {0x11,0x11,0x11,0x11,0x11,0x0A,0x04}},
    {'W', {0x11,0x11,0x11,0x15,0x15,0x15,0x0A}},
    {'X', {0x11,0x11,0x0A,0x04,0x0A,0x11,0x11}},
    {'Y', {0x11,0x11,0x0A,0x04,0x04,0x04,0x04}},
    {'Z', {0x1F,0x01,0x02,0x04,0x08,0x10,0x1F}},
};

constexpr uint8_t kQuestionRows[7] = {0x0E,0x11,0x01,0x02,0x04,0x00,0x04};

const uint8_t *glyph_rows(char ch)
{
    for (const auto &glyph : kGlyphs) {
        if (glyph.ch == ch) {
            return glyph.rows;
        }
    }
    return kQuestionRows;
}

int create_anonymous_file(size_t size)
{
    int fd = -1;
#ifdef SYS_memfd_create
    fd = static_cast<int>(syscall(SYS_memfd_create, "zero-shell-wayland", MFD_CLOEXEC));
    if (fd >= 0) {
        if (ftruncate(fd, static_cast<off_t>(size)) == 0) {
            return fd;
        }
        close(fd);
    }
#endif

    char name[] = "/zero-shell-wayland-XXXXXX";
    fd = shm_open(name, O_RDWR | O_CREAT | O_EXCL, 0600);
    if (fd < 0) {
        return -1;
    }
    shm_unlink(name);
    if (ftruncate(fd, static_cast<off_t>(size)) < 0) {
        close(fd);
        return -1;
    }
    return fd;
}

uint32_t pixel(Color color)
{
    return 0xFF000000u |
           (static_cast<uint32_t>(color.r) << 16) |
           (static_cast<uint32_t>(color.g) << 8) |
           static_cast<uint32_t>(color.b);
}

std::string uppercase_ascii(std::string text)
{
    for (char &ch : text) {
        ch = static_cast<char>(std::toupper(static_cast<unsigned char>(ch)));
    }
    return text;
}

std::string lowercase_ascii(std::string text)
{
    for (char &ch : text) {
        ch = static_cast<char>(std::tolower(static_cast<unsigned char>(ch)));
    }
    return text;
}

std::string display_ascii(std::string text)
{
    std::string output;
    output.reserve(text.size());
    bool last_was_space = false;

    for (unsigned char ch : text) {
        if (ch >= 0x80) {
            continue;
        }
        if (std::isalnum(ch)) {
            output.push_back(static_cast<char>(std::toupper(ch)));
            last_was_space = false;
        } else if ((ch == ' ' || ch == '-' || ch == '_') && !output.empty() && !last_was_space) {
            output.push_back(' ');
            last_was_space = true;
        }
    }

    while (!output.empty() && output.back() == ' ') {
        output.pop_back();
    }
    return output;
}

std::string label_for(const zero_shell::AppEntry &app, size_t max_chars = 8)
{
    std::string label = app.short_name.empty() ? display_ascii(app.name) : display_ascii(app.short_name);
    if (label.empty()) {
        label = display_ascii(app.id);
    }
    label = uppercase_ascii(label);
    if (label.size() > max_chars) {
        label.resize(max_chars);
    }
    return label;
}

std::string current_time_label()
{
    std::time_t now = std::time(nullptr);
    std::tm local{};
    localtime_r(&now, &local);
    char buf[8] = {};
    std::strftime(buf, sizeof(buf), "%H:%M", &local);
    return buf;
}

bool launch_detached(const std::string &command)
{
    if (command.empty()) {
        return false;
    }

    pid_t pid = fork();
    if (pid < 0) {
        return false;
    }

    if (pid == 0) {
        setpgid(0, 0);
        execlp("/bin/sh", "sh", "-lc", command.c_str(), static_cast<char *>(nullptr));
        _exit(127);
    }

    setpgid(pid, pid);
    return true;
}

std::string shell_quote(const std::string &value)
{
    std::string quoted = "'";
    for (char ch : value) {
        if (ch == '\'') {
            quoted += "'\\''";
        } else {
            quoted.push_back(ch);
        }
    }
    quoted += "'";
    return quoted;
}

bool run_shell_command(const std::string &command)
{
    if (command.empty()) {
        return false;
    }
    return std::system(command.c_str()) == 0;
}

std::string runtime_command_path()
{
    const char *runtime = std::getenv("XDG_RUNTIME_DIR");
    if (runtime && *runtime) {
        return std::string(runtime) + "/zero-shell-command";
    }
    return "/tmp/zero-shell-command";
}

std::filesystem::file_time_type catalog_mtime(const std::filesystem::path &dir)
{
    std::error_code ec;
    if (!std::filesystem::is_directory(dir, ec)) {
        return {};
    }

    std::filesystem::file_time_type latest = std::filesystem::last_write_time(dir, ec);
    if (ec) {
        latest = {};
        ec.clear();
    }

    for (const auto &entry : std::filesystem::directory_iterator(dir, ec)) {
        if (ec) {
            break;
        }
        if (!entry.is_regular_file(ec) || entry.path().extension() != ".desktop") {
            ec.clear();
            continue;
        }
        auto mtime = std::filesystem::last_write_time(entry.path(), ec);
        if (ec) {
            ec.clear();
            continue;
        }
        if (mtime > latest) {
            latest = mtime;
        }
    }

    return latest;
}

bool can_launch_in_wayland(const zero_shell::AppEntry &app)
{
    std::string display = lowercase_ascii(app.zero_display);
    if (display == "wayland" || display == "xwayland") {
        return true;
    }
    if (display == "framebuffer" || display == "fbdev" || display == "direct-fb") {
        return false;
    }

    return false;
}

uint16_t normalize_wayland_key(uint32_t key)
{
    // wl_keyboard.key is already a Linux evdev keycode, unlike X11 keycodes.
    return static_cast<uint16_t>(key);
}

class WaylandShell {
public:
    bool init();
    int run();
    struct Buffer {
        wl_buffer *buffer = nullptr;
        uint32_t *pixels = nullptr;
        bool busy = false;
    };

    static void registry_global(void *data, wl_registry *registry, uint32_t name,
                                const char *interface, uint32_t version);
    static void registry_remove(void *, wl_registry *, uint32_t) {}
    static void wm_ping(void *data, xdg_wm_base *wm, uint32_t serial);
    static void surface_configure(void *data, xdg_surface *surface, uint32_t serial);
    static void toplevel_configure(void *, xdg_toplevel *, int32_t, int32_t, wl_array *) {}
    static void toplevel_close(void *data, xdg_toplevel *);
    static void seat_capabilities(void *data, wl_seat *, uint32_t capabilities);
    static void seat_name(void *, wl_seat *, const char *) {}
    static void keyboard_keymap(void *, wl_keyboard *, uint32_t, int32_t fd, uint32_t) { close(fd); }
    static void keyboard_enter(void *data, wl_keyboard *, uint32_t, wl_surface *, wl_array *);
    static void keyboard_leave(void *data, wl_keyboard *, uint32_t, wl_surface *);
    static void keyboard_key(void *data, wl_keyboard *, uint32_t, uint32_t, uint32_t key, uint32_t state);
    static void keyboard_modifiers(void *, wl_keyboard *, uint32_t, uint32_t, uint32_t, uint32_t, uint32_t) {}
    static void keyboard_repeat(void *, wl_keyboard *, int32_t, int32_t) {}
    static void buffer_release(void *data, wl_buffer *);

private:
    bool create_buffer(Buffer &buffer);
    Buffer *next_buffer();
    void load_apps();
    void reload_apps_if_changed();
    void refresh_tasks(bool force = false);
    void process_commands();
    void update_launch_status();
    void set_status(std::string message, std::chrono::milliseconds duration);
    void handle_key(uint16_t key);
    void focus_task(size_t task_index);
    void render();
    void draw_frame(uint32_t *pixels);
    void draw_task_panel(uint32_t *pixels);
    void clear(uint32_t *pixels, Color color);
    void fill_rect(uint32_t *pixels, int x, int y, int w, int h, Color color);
    void draw_rect(uint32_t *pixels, int x, int y, int w, int h, Color color);
    void draw_text(uint32_t *pixels, int x, int y, const std::string &text, Color color, int scale = 1);
    void draw_text_centered(uint32_t *pixels, int cx, int y, const std::string &text, Color color, int scale = 1);
    void draw_image_fit(uint32_t *pixels, const zero_shell::Image &image, int x, int y, int w, int h);
    void draw_app_card(uint32_t *pixels, int x, int y, const zero_shell::AppEntry &app, bool selected, bool running);
    void draw_running_badge(uint32_t *pixels, int x, int y);
    void draw_battery(uint32_t *pixels);
    void debug_log(const std::string &message);
    bool is_app_running(const zero_shell::AppEntry &app) const;
    bool task_matches_app(const WindowTask &task, const zero_shell::AppEntry &app) const;
    bool focus_app_task(const zero_shell::AppEntry &app);

    wl_display *display_ = nullptr;
    wl_registry *registry_ = nullptr;
    wl_compositor *compositor_ = nullptr;
    wl_shm *shm_ = nullptr;
    wl_seat *seat_ = nullptr;
    wl_keyboard *keyboard_ = nullptr;
    wl_surface *surface_ = nullptr;
    xdg_wm_base *wm_ = nullptr;
    xdg_surface *xdg_surface_ = nullptr;
    xdg_toplevel *toplevel_ = nullptr;
    Buffer buffers_[2]{};
    bool configured_ = false;
    bool running_ = true;
    bool dirty_ = true;
    bool debug_ = false;
    bool keyboard_focused_ = false;
    bool task_view_ = false;
    std::string status_message_;
    std::chrono::steady_clock::time_point status_until_{};
    std::FILE *debug_file_ = nullptr;
    std::filesystem::path applications_dir_;
    std::filesystem::file_time_type last_catalog_mtime_{};
    std::vector<zero_shell::AppEntry> apps_;
    std::vector<WindowTask> tasks_;
    std::chrono::steady_clock::time_point tasks_last_refresh_{};
    bool launch_pending_ = false;
    zero_shell::AppEntry launch_pending_app_;
    std::chrono::steady_clock::time_point launch_deadline_{};
    size_t current_index_ = 0;
    size_t task_selection_ = 0;
};

const wl_registry_listener kRegistryListener{WaylandShell::registry_global, WaylandShell::registry_remove};
const xdg_wm_base_listener kWmListener{WaylandShell::wm_ping};
const xdg_surface_listener kSurfaceListener{WaylandShell::surface_configure};
const xdg_toplevel_listener kToplevelListener{
    WaylandShell::toplevel_configure,
    WaylandShell::toplevel_close,
    nullptr,
    nullptr,
};
const wl_seat_listener kSeatListener{WaylandShell::seat_capabilities, WaylandShell::seat_name};
const wl_keyboard_listener kKeyboardListener{
    WaylandShell::keyboard_keymap,
    WaylandShell::keyboard_enter,
    WaylandShell::keyboard_leave,
    WaylandShell::keyboard_key,
    WaylandShell::keyboard_modifiers,
    WaylandShell::keyboard_repeat,
};
const wl_buffer_listener kBufferListener{WaylandShell::buffer_release};

void WaylandShell::registry_global(void *data, wl_registry *registry, uint32_t name,
                                   const char *interface, uint32_t version)
{
    auto *self = static_cast<WaylandShell *>(data);
    if (std::strcmp(interface, wl_compositor_interface.name) == 0) {
        self->compositor_ = static_cast<wl_compositor *>(
            wl_registry_bind(registry, name, &wl_compositor_interface, std::min(version, 4u)));
    } else if (std::strcmp(interface, wl_shm_interface.name) == 0) {
        self->shm_ = static_cast<wl_shm *>(
            wl_registry_bind(registry, name, &wl_shm_interface, 1));
    } else if (std::strcmp(interface, wl_seat_interface.name) == 0) {
        self->seat_ = static_cast<wl_seat *>(
            wl_registry_bind(registry, name, &wl_seat_interface, std::min(version, 7u)));
        wl_seat_add_listener(self->seat_, &kSeatListener, self);
    } else if (std::strcmp(interface, xdg_wm_base_interface.name) == 0) {
        self->wm_ = static_cast<xdg_wm_base *>(
            wl_registry_bind(registry, name, &xdg_wm_base_interface, 1));
        xdg_wm_base_add_listener(self->wm_, &kWmListener, self);
    }
}

void WaylandShell::wm_ping(void *, xdg_wm_base *wm, uint32_t serial)
{
    xdg_wm_base_pong(wm, serial);
}

void WaylandShell::surface_configure(void *data, xdg_surface *surface, uint32_t serial)
{
    auto *self = static_cast<WaylandShell *>(data);
    xdg_surface_ack_configure(surface, serial);
    self->configured_ = true;
    self->dirty_ = true;
}

void WaylandShell::toplevel_close(void *data, xdg_toplevel *)
{
    static_cast<WaylandShell *>(data)->running_ = false;
}

void WaylandShell::seat_capabilities(void *data, wl_seat *, uint32_t capabilities)
{
    auto *self = static_cast<WaylandShell *>(data);
    if (self->debug_) {
        self->debug_log(
            "seat capabilities=" + std::to_string(capabilities) +
            " keyboard=" + ((capabilities & WL_SEAT_CAPABILITY_KEYBOARD) ? "yes" : "no") +
            " pointer=" + ((capabilities & WL_SEAT_CAPABILITY_POINTER) ? "yes" : "no") +
            " touch=" + ((capabilities & WL_SEAT_CAPABILITY_TOUCH) ? "yes" : "no"));
    }
    if ((capabilities & WL_SEAT_CAPABILITY_KEYBOARD) && self->keyboard_ == nullptr) {
        self->keyboard_ = wl_seat_get_keyboard(self->seat_);
        wl_keyboard_add_listener(self->keyboard_, &kKeyboardListener, self);
    } else if (!(capabilities & WL_SEAT_CAPABILITY_KEYBOARD) && self->keyboard_ != nullptr) {
        wl_keyboard_destroy(self->keyboard_);
        self->keyboard_ = nullptr;
    }
}

void WaylandShell::keyboard_enter(void *data, wl_keyboard *, uint32_t serial,
                                  wl_surface *, wl_array *)
{
    auto *self = static_cast<WaylandShell *>(data);
    self->keyboard_focused_ = true;
    if (self->debug_) {
        self->debug_log("keyboard enter serial=" + std::to_string(serial));
    }
}

void WaylandShell::keyboard_leave(void *data, wl_keyboard *, uint32_t serial,
                                  wl_surface *)
{
    auto *self = static_cast<WaylandShell *>(data);
    self->keyboard_focused_ = false;
    if (self->debug_) {
        self->debug_log("keyboard leave serial=" + std::to_string(serial));
    }
}

void WaylandShell::keyboard_key(void *data, wl_keyboard *, uint32_t, uint32_t,
                                uint32_t key, uint32_t state)
{
    auto *self = static_cast<WaylandShell *>(data);
    if (self->debug_) {
        self->debug_log(
            "keyboard key raw=" + std::to_string(key) +
            " normalized=" + std::to_string(normalize_wayland_key(key)) +
            " state=" + std::to_string(state) +
            " focused=" + (self->keyboard_focused_ ? "yes" : "no"));
    }
    if (state != WL_KEYBOARD_KEY_STATE_PRESSED) {
        return;
    }
    self->handle_key(normalize_wayland_key(key));
}

void WaylandShell::buffer_release(void *data, wl_buffer *)
{
    static_cast<Buffer *>(data)->busy = false;
}

void WaylandShell::debug_log(const std::string &message)
{
    if (!debug_) {
        return;
    }

    if (debug_file_) {
        std::fprintf(debug_file_, "zero-shell-wayland: %s\n", message.c_str());
        std::fflush(debug_file_);
        return;
    }

    std::cerr << "zero-shell-wayland: " << message << "\n";
}

bool WaylandShell::create_buffer(Buffer &buffer)
{
    int fd = create_anonymous_file(kBufferSize);
    if (fd < 0) {
        std::cerr << "zero-shell-wayland: cannot create shm file: " << std::strerror(errno) << "\n";
        return false;
    }

    void *data = mmap(nullptr, kBufferSize, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
    if (data == MAP_FAILED) {
        std::cerr << "zero-shell-wayland: cannot mmap shm buffer: " << std::strerror(errno) << "\n";
        close(fd);
        return false;
    }

    wl_shm_pool *pool = wl_shm_create_pool(shm_, fd, kBufferSize);
    buffer.buffer = wl_shm_pool_create_buffer(pool, 0, kWidth, kHeight, kStride, WL_SHM_FORMAT_XRGB8888);
    buffer.pixels = static_cast<uint32_t *>(data);
    buffer.busy = false;
    wl_buffer_add_listener(buffer.buffer, &kBufferListener, &buffer);
    wl_shm_pool_destroy(pool);
    close(fd);
    return true;
}

WaylandShell::Buffer *WaylandShell::next_buffer()
{
    for (auto &buffer : buffers_) {
        if (!buffer.busy) {
            return &buffer;
        }
    }
    return nullptr;
}

bool WaylandShell::init()
{
    debug_ = std::getenv("ZEROSHELL_WAYLAND_DEBUG") != nullptr;
    if (debug_) {
        debug_file_ = std::fopen("/tmp/zero-shell-wayland.keys.log", "a");
        if (debug_file_) {
            std::setvbuf(debug_file_, nullptr, _IOLBF, 0);
        }
        debug_log("debug logging enabled");
    }

    if (geteuid() == 0) {
        std::cerr << "zero-shell-wayland: refusing to run as root\n";
        return false;
    }

    display_ = wl_display_connect(nullptr);
    if (!display_) {
        std::cerr << "zero-shell-wayland: cannot connect to Wayland display\n";
        return false;
    }

    registry_ = wl_display_get_registry(display_);
    wl_registry_add_listener(registry_, &kRegistryListener, this);
    wl_display_roundtrip(display_);
    wl_display_roundtrip(display_);

    if (!compositor_ || !shm_ || !wm_) {
        std::cerr << "zero-shell-wayland: missing compositor/shm/xdg-shell globals\n";
        return false;
    }

    surface_ = wl_compositor_create_surface(compositor_);
    xdg_surface_ = xdg_wm_base_get_xdg_surface(wm_, surface_);
    xdg_surface_add_listener(xdg_surface_, &kSurfaceListener, this);
    toplevel_ = xdg_surface_get_toplevel(xdg_surface_);
    xdg_toplevel_add_listener(toplevel_, &kToplevelListener, this);
    xdg_toplevel_set_title(toplevel_, "Cardputer Zero Shell");
    xdg_toplevel_set_app_id(toplevel_, "cardputer-zero-shell");
    xdg_toplevel_set_min_size(toplevel_, kWidth, kHeight);
    xdg_toplevel_set_max_size(toplevel_, kWidth, kHeight);
    xdg_toplevel_set_fullscreen(toplevel_, nullptr);

    if (!create_buffer(buffers_[0]) || !create_buffer(buffers_[1])) {
        return false;
    }

    load_apps();
    wl_surface_commit(surface_);
    wl_display_flush(display_);
    return true;
}

void WaylandShell::load_apps()
{
    const char *env = std::getenv("ZEROSHELL_APPLICATIONS_DIR");
    applications_dir_ = env && *env ? env : "/usr/share/APPLaunch/applications";
    zero_shell::AppCatalog catalog(applications_dir_);
    apps_ = catalog.load();
    if (current_index_ >= apps_.size()) {
        current_index_ = apps_.empty() ? 0 : apps_.size() - 1;
    }
    last_catalog_mtime_ = catalog_mtime(applications_dir_);
    dirty_ = true;
}

void WaylandShell::reload_apps_if_changed()
{
    if (applications_dir_.empty()) {
        return;
    }

    auto current_mtime = catalog_mtime(applications_dir_);
    if (current_mtime != last_catalog_mtime_) {
        load_apps();
    }
}

void WaylandShell::refresh_tasks(bool force)
{
    auto now = std::chrono::steady_clock::now();
    if (!force && now - tasks_last_refresh_ < std::chrono::milliseconds(750)) {
        return;
    }
    tasks_last_refresh_ = now;

    std::vector<WindowTask> tasks;
    FILE *pipe = popen("wlrctl toplevel list 2>/dev/null", "r");
    if (!pipe) {
        if (!tasks_.empty() || task_selection_ != 0) {
            tasks_.clear();
            task_selection_ = 0;
            dirty_ = true;
        }
        return;
    }

    char line[512];
    while (std::fgets(line, sizeof(line), pipe)) {
        std::string value(line);
        while (!value.empty() && (value.back() == '\n' || value.back() == '\r')) {
            value.pop_back();
        }
        if (value.empty()) {
            continue;
        }

        WindowTask task;
        size_t sep = value.find(": ");
        if (sep == std::string::npos) {
            task.title = value;
        } else {
            task.app_id = value.substr(0, sep);
            task.title = value.substr(sep + 2);
        }

        if (task.app_id == "cardputer-zero-shell" || task.title == "Cardputer Zero Shell") {
            continue;
        }
        if (task.app_id.empty() && task.title.empty()) {
            continue;
        }
        tasks.push_back(std::move(task));
    }
    pclose(pipe);

    bool changed = !same_tasks(tasks_, tasks);
    size_t old_selection = task_selection_;
    tasks_ = std::move(tasks);
    if (task_selection_ >= tasks_.size()) {
        task_selection_ = tasks_.empty() ? 0 : tasks_.size() - 1;
    }
    if (changed || old_selection != task_selection_) {
        dirty_ = true;
    }
}

void WaylandShell::process_commands()
{
    std::string path = runtime_command_path();
    FILE *file = std::fopen(path.c_str(), "r");
    if (!file) {
        return;
    }

    char line[128];
    while (std::fgets(line, sizeof(line), file)) {
        std::string command(line);
        while (!command.empty() && (command.back() == '\n' || command.back() == '\r')) {
            command.pop_back();
        }

        if (command == "show-tasks") {
            refresh_tasks(true);
            task_view_ = true;
            dirty_ = true;
        } else if (command == "toggle-tasks") {
            refresh_tasks(true);
            task_view_ = !task_view_;
            dirty_ = true;
        } else if (command == "hide-tasks") {
            task_view_ = false;
            dirty_ = true;
        } else if (command == "reload-apps") {
            load_apps();
        }
    }

    std::fclose(file);
    std::remove(path.c_str());
}

void WaylandShell::update_launch_status()
{
    if (!launch_pending_) {
        return;
    }

    auto now = std::chrono::steady_clock::now();
    refresh_tasks(false);

    if (is_app_running(launch_pending_app_)) {
        launch_pending_ = false;
        status_message_.clear();
        dirty_ = true;
        return;
    }

    if (now >= launch_deadline_) {
        launch_pending_ = false;
        if (status_message_ == "LOADING") {
            status_message_.clear();
        }
        dirty_ = true;
        return;
    }

    if (status_message_ != "LOADING") {
        status_message_ = "LOADING";
        dirty_ = true;
    }
    status_until_ = launch_deadline_;
}

void WaylandShell::set_status(std::string message, std::chrono::milliseconds duration)
{
    status_message_ = std::move(message);
    status_until_ = std::chrono::steady_clock::now() + duration;
    dirty_ = true;
}

void WaylandShell::handle_key(uint16_t key)
{
    if (debug_) {
        debug_log("handle key=" + std::to_string(key));
    }

    if (task_view_) {
        switch (key) {
        case KEY_TAB:
        case KEY_ESC:
            refresh_tasks(true);
            task_view_ = false;
            dirty_ = true;
            return;
        case KEY_LEFT:
        case KEY_UP:
            refresh_tasks(true);
            if (!tasks_.empty()) {
                task_selection_ = (task_selection_ + tasks_.size() - 1) % tasks_.size();
            }
            dirty_ = true;
            return;
        case KEY_RIGHT:
        case KEY_DOWN:
            refresh_tasks(true);
            if (!tasks_.empty()) {
                task_selection_ = (task_selection_ + 1) % tasks_.size();
            }
            dirty_ = true;
            return;
        case KEY_ENTER:
            focus_task(task_selection_);
            task_view_ = false;
            dirty_ = true;
            return;
        default:
            return;
        }
    }

    switch (key) {
    case KEY_LEFT:
        if (debug_) {
            debug_log("action previous");
        }
        task_view_ = false;
        if (!apps_.empty()) {
            current_index_ = (current_index_ + apps_.size() - 1) % apps_.size();
            dirty_ = true;
        }
        break;
    case KEY_RIGHT:
        if (debug_) {
            debug_log("action next");
        }
        task_view_ = false;
        if (!apps_.empty()) {
            current_index_ = (current_index_ + 1) % apps_.size();
            dirty_ = true;
        }
        break;
    case KEY_ENTER:
        if (debug_) {
            debug_log("action launch");
        }
        task_view_ = false;
        if (!apps_.empty()) {
            const auto &app = apps_[current_index_];
            if (can_launch_in_wayland(app)) {
                refresh_tasks(true);
                if (focus_app_task(app)) {
                    set_status("OPEN", std::chrono::milliseconds(900));
                    break;
                }
                if (launch_pending_ && app.id == launch_pending_app_.id) {
                    set_status("LOADING", std::chrono::seconds(2));
                    break;
                }
                launch_pending_ = true;
                launch_pending_app_ = app;
                launch_deadline_ = std::chrono::steady_clock::now() + std::chrono::seconds(8);
                status_message_ = "LOADING";
                status_until_ = launch_deadline_;
                dirty_ = true;
                render();
                wl_display_flush(display_);
                if (!launch_detached(app.exec)) {
                    launch_pending_ = false;
                    set_status("LAUNCH FAILED", std::chrono::seconds(2));
                }
            } else {
                set_status("NEEDS WAYLAND APP", std::chrono::seconds(2));
            }
        }
        break;
    case KEY_TAB:
        refresh_tasks(true);
        task_view_ = !task_view_;
        dirty_ = true;
        break;
    case KEY_R:
        task_view_ = false;
        load_apps();
        break;
    case KEY_ESC:
        refresh_tasks(true);
        task_view_ = false;
        dirty_ = true;
        break;
    default:
        break;
    }
}

void WaylandShell::focus_task(size_t task_index)
{
    refresh_tasks(true);
    if (task_index >= tasks_.size()) {
        return;
    }

    const auto &task = tasks_[task_index];
    std::string command;
    if (!task.app_id.empty()) {
        command = "wlrctl toplevel focus app_id:" + shell_quote(task.app_id);
    } else if (!task.title.empty()) {
        command = "wlrctl toplevel focus title:" + shell_quote(task.title);
    }
    run_shell_command(command);
}

void WaylandShell::clear(uint32_t *pixels, Color color)
{
    std::fill(pixels, pixels + (kWidth * kHeight), pixel(color));
}

void WaylandShell::fill_rect(uint32_t *pixels, int x, int y, int w, int h, Color color)
{
    int x0 = std::clamp(x, 0, kWidth);
    int y0 = std::clamp(y, 0, kHeight);
    int x1 = std::clamp(x + w, 0, kWidth);
    int y1 = std::clamp(y + h, 0, kHeight);
    uint32_t p = pixel(color);
    for (int py = y0; py < y1; ++py) {
        for (int px = x0; px < x1; ++px) {
            pixels[py * kWidth + px] = p;
        }
    }
}

void WaylandShell::draw_rect(uint32_t *pixels, int x, int y, int w, int h, Color color)
{
    fill_rect(pixels, x, y, w, 1, color);
    fill_rect(pixels, x, y + h - 1, w, 1, color);
    fill_rect(pixels, x, y, 1, h, color);
    fill_rect(pixels, x + w - 1, y, 1, h, color);
}

void WaylandShell::draw_text(uint32_t *pixels, int x, int y, const std::string &text, Color color, int scale)
{
    int cursor = x;
    scale = std::max(1, scale);
    for (char ch : text) {
        const uint8_t *rows = ch == '?' ? kQuestionRows : glyph_rows(ch);
        for (int row = 0; row < 7; ++row) {
            for (int col = 0; col < 5; ++col) {
                if (rows[row] & (1 << (4 - col))) {
                    fill_rect(pixels, cursor + col * scale, y + row * scale, scale, scale, color);
                }
            }
        }
        cursor += 6 * scale;
    }
}

void WaylandShell::draw_text_centered(uint32_t *pixels, int cx, int y, const std::string &text, Color color, int scale)
{
    int width = static_cast<int>(text.size()) * 6 * std::max(1, scale);
    draw_text(pixels, cx - width / 2, y, text, color, scale);
}

void WaylandShell::draw_image_fit(uint32_t *pixels, const zero_shell::Image &image, int x, int y, int w, int h)
{
    if (image.empty() || w <= 0 || h <= 0) {
        return;
    }

    double scale = std::min(static_cast<double>(w) / static_cast<double>(image.width),
                            static_cast<double>(h) / static_cast<double>(image.height));
    int dest_w = std::max(1, static_cast<int>(image.width * scale));
    int dest_h = std::max(1, static_cast<int>(image.height * scale));
    int dest_x = x + (w - dest_w) / 2;
    int dest_y = y + (h - dest_h) / 2;

    for (int py = 0; py < dest_h; ++py) {
        int sy = std::clamp(static_cast<int>((static_cast<long long>(py) * image.height) / dest_h),
                            0, image.height - 1);
        for (int px = 0; px < dest_w; ++px) {
            int sx = std::clamp(static_cast<int>((static_cast<long long>(px) * image.width) / dest_w),
                                0, image.width - 1);
            size_t offset = (static_cast<size_t>(sy) * static_cast<size_t>(image.width) +
                             static_cast<size_t>(sx)) * 4;
            uint8_t alpha = image.rgba[offset + 3];
            if (alpha == 0) {
                continue;
            }
            int dx = dest_x + px;
            int dy = dest_y + py;
            if (dx < 0 || dy < 0 || dx >= kWidth || dy >= kHeight) {
                continue;
            }
            Color fg{image.rgba[offset], image.rgba[offset + 1], image.rgba[offset + 2]};
            if (alpha == 255) {
                pixels[dy * kWidth + dx] = pixel(fg);
                continue;
            }
            uint32_t bg = pixels[dy * kWidth + dx];
            auto blend = [alpha](uint8_t f, uint8_t b) {
                return static_cast<uint8_t>((static_cast<int>(f) * alpha +
                                             static_cast<int>(b) * (255 - alpha)) / 255);
            };
            Color out{
                blend(fg.r, static_cast<uint8_t>((bg >> 16) & 0xFF)),
                blend(fg.g, static_cast<uint8_t>((bg >> 8) & 0xFF)),
                blend(fg.b, static_cast<uint8_t>(bg & 0xFF)),
            };
            pixels[dy * kWidth + dx] = pixel(out);
        }
    }
}

void WaylandShell::draw_app_card(uint32_t *pixels, int x, int y, const zero_shell::AppEntry &app, bool selected, bool running)
{
    constexpr int card_w = 72;
    constexpr int card_h = 74;
    Color card = selected ? kTileActive : kTile;
    Color border = selected ? kInk : kMuted;
    fill_rect(pixels, x + 2, y + 2, card_w, card_h, kShadow);
    fill_rect(pixels, x, y, card_w, card_h, card);
    draw_rect(pixels, x, y, card_w, card_h, border);
    if (selected) {
        draw_rect(pixels, x + 1, y + 1, card_w - 2, card_h - 2, kInk);
    }

    int well_x = x + 16;
    int well_y = y + 9;
    fill_rect(pixels, well_x, well_y, 40, 40, kIconWell);
    draw_rect(pixels, well_x, well_y, 40, 40, kInk);

    bool drew_icon = false;
    std::string icon_path = zero_shell::resolve_applaunch_path(app.icon);
    if (!icon_path.empty()) {
        zero_shell::Image image;
        if (zero_shell::load_png_image(icon_path, image)) {
            draw_image_fit(pixels, image, well_x + 4, well_y + 4, 32, 32);
            drew_icon = true;
        }
    }
    if (!drew_icon) {
        draw_rect(pixels, well_x + 9, well_y + 10, 22, 18, kInk);
        fill_rect(pixels, well_x + 11, well_y + 8, 10, 3, kIconWell);
        draw_rect(pixels, well_x + 11, well_y + 8, 10, 5, kInk);
    }

    int strip_x = x + 6;
    int strip_y = y + 55;
    fill_rect(pixels, strip_x, strip_y, card_w - 12, 13, kLabelStrip);
    draw_rect(pixels, strip_x, strip_y, card_w - 12, 13, kSoftLine);
    draw_text_centered(pixels, x + card_w / 2, strip_y + 3, label_for(app), kInk, 1);

    if (selected) {
        fill_rect(pixels, x + 10, y + 70, card_w - 20, 3, kAccent);
    }

    if (running) {
        draw_running_badge(pixels, x + card_w - 25, y + 5);
    }
}

void WaylandShell::draw_running_badge(uint32_t *pixels, int x, int y)
{
    fill_rect(pixels, x + 2, y + 2, 22, 11, kShadow);
    fill_rect(pixels, x, y, 22, 11, kAccent);
    draw_rect(pixels, x, y, 22, 11, kInk);
    draw_text(pixels, x + 3, y + 3, "RUN", kInk, 1);
}

void WaylandShell::draw_battery(uint32_t *pixels)
{
    zero_shell::StatusSnapshot status = zero_shell::read_status();
    int x = kWidth - 78;
    if (status.wifi_connected) {
        int bars = std::clamp(status.wifi_signal / 25 + 1, 1, 4);
        for (int i = 0; i < 4; ++i) {
            Color color = i < bars ? kInk : kSoftLine;
            fill_rect(pixels, x + i * 4, 12 - i * 2, 3, 3 + i * 2, color);
        }
    } else {
        draw_text(pixels, x, 5, "--", kMuted, 1);
    }

    std::string battery = status.battery_valid
        ? std::to_string(std::clamp(status.battery_percent, 0, 100)) + "%"
        : "--";
    draw_text(pixels, x + 21, 5, battery, status.battery_valid ? kInk : kMuted, 1);

    int bx = kWidth - 29;
    int by = 5;
    draw_rect(pixels, bx, by, 22, 9, kLine);
    fill_rect(pixels, bx + 22, by + 3, 2, 3, kLine);
    if (status.battery_valid) {
        int fill = std::clamp((status.battery_percent * 18) / 100, 0, 18);
        fill_rect(pixels, bx + 2, by + 2, fill, 5,
                  status.battery_percent >= 25 ? kOk : kWarn);
    }
}

void WaylandShell::draw_frame(uint32_t *pixels)
{
    clear(pixels, kZeroBg);
    for (int y = 28; y < 138; y += 8) {
        for (int x = 8; x < kWidth; x += 8) {
            fill_rect(pixels, x, y, 1, 1, kGridDot);
        }
    }

    fill_rect(pixels, 0, 0, kWidth, kBarHeight, kPanel);
    draw_rect(pixels, 0, 0, kWidth, kBarHeight, kLine);
    draw_text(pixels, 6, 5, current_time_label(), kInk, 1);
    draw_battery(pixels);

    int y = kHeight - kBarHeight;
    fill_rect(pixels, 0, y, kWidth, kBarHeight, kPanel);
    draw_rect(pixels, 0, y, kWidth, kBarHeight, kLine);
    fill_rect(pixels, 0, y, 82, kBarHeight, kTaskButton);
    draw_rect(pixels, 0, y, 82, kBarHeight, kLine);
    draw_rect(pixels, 6, y + 4, 22, 11, kInk);
    draw_text(pixels, 9, y + 6, "TAB", kInk, 1);
    draw_text(pixels, 34, y + 6, "TASK", kInk, 1);
    fill_rect(pixels, 70, y + 4, 11, 11, kAccent);
    draw_rect(pixels, 70, y + 4, 11, 11, kInk);
    draw_text(pixels, 73, y + 6, std::to_string(tasks_.size()), kInk, 1);
    draw_text(pixels, 94, y + 6, "< >", kMuted, 1);
    draw_text(pixels, 118, y + 6, "SELECT", kMuted, 1);
    draw_text(pixels, 190, y + 6, "ENTER OPEN", kInk, 1);
    draw_text(pixels, 276, y + 6, "ESC", kMuted, 1);
}

void WaylandShell::draw_task_panel(uint32_t *pixels)
{
    int w = 146;
    int h = tasks_.empty() ? 38 : std::min(100, 20 + static_cast<int>(tasks_.size()) * 17);
    int x = 0;
    int y = kHeight - kBarHeight - h;

    fill_rect(pixels, x + 3, y + 3, w, h, kShadow);
    fill_rect(pixels, x, y, w, h, kPanel);
    draw_rect(pixels, x, y, w, h, kLine);
    fill_rect(pixels, x, y, w, 17, kInk);
    draw_text(pixels, x + 7, y + 5, "RUNNING TASKS", kPanel, 1);

    if (tasks_.empty()) {
        draw_text(pixels, x + 8, y + 24, "NO TASKS", kMuted, 1);
        return;
    }

    size_t visible = std::min<size_t>(tasks_.size(), 4);
    for (size_t i = 0; i < visible; ++i) {
        int item_y = y + 21 + static_cast<int>(i) * 17;
        bool selected = i == task_selection_;
        std::string label = display_ascii(tasks_[i].title.empty() ? tasks_[i].app_id : tasks_[i].title);
        label = uppercase_ascii(label);
        if (label.size() > 14) {
            label.resize(14);
        }
        fill_rect(pixels, x + 6, item_y, w - 12, 14, selected ? kAccent : kPanel);
        draw_rect(pixels, x + 6, item_y, w - 12, 14, selected ? kInk : kSoftLine);
        draw_text(pixels, x + 10, item_y + 3, selected ? ">" : " ", selected ? kInk : kMuted, 1);
        draw_text(pixels, x + 21, item_y + 3, label, selected ? kInk : kMuted, 1);
    }
}

bool WaylandShell::is_app_running(const zero_shell::AppEntry &app) const
{
    return std::any_of(tasks_.begin(), tasks_.end(), [&](const WindowTask &task) {
        return task_matches_app(task, app);
    });
}

bool WaylandShell::task_matches_app(const WindowTask &task, const zero_shell::AppEntry &app) const
{
    std::string app_id = lowercase_ascii(app.zero_app_id.empty() ? app.id : app.zero_app_id);
    std::string wm_class = lowercase_ascii(app.startup_wm_class);
    std::string name = display_ascii(app.name);

    std::string task_app_id = lowercase_ascii(task.app_id);
    if (!task_app_id.empty() &&
        (task_app_id == app_id || (!wm_class.empty() && task_app_id == wm_class))) {
        return true;
    }

    std::string title = display_ascii(task.title);
    return !name.empty() && title.find(name) != std::string::npos;
}

bool WaylandShell::focus_app_task(const zero_shell::AppEntry &app)
{
    for (size_t i = 0; i < tasks_.size(); ++i) {
        if (task_matches_app(tasks_[i], app)) {
            focus_task(i);
            return true;
        }
    }
    return false;
}

void WaylandShell::render()
{
    if (!configured_) {
        return;
    }

    Buffer *buffer = next_buffer();
    if (!buffer) {
        return;
    }

    refresh_tasks(task_view_);
    draw_frame(buffer->pixels);
    if (apps_.empty()) {
        draw_text_centered(buffer->pixels, kWidth / 2, 58, "NO APPS", kInk, 1);
        draw_text_centered(buffer->pixels, kWidth / 2, 78, "APPLaunch/applications", kMuted, 1);
        draw_text_centered(buffer->pixels, kWidth / 2, 104, "PRESS R", kAccent, 1);
    } else {
        const int xs[3] = {42, 119, 196};
        const int ys[3] = {50, 42, 50};
        const int slot_order[3] = {1, 0, 2};
        for (int order = 0; order < 3; ++order) {
            int slot = slot_order[order];
            int offset = slot - 1;
            size_t index = (current_index_ + apps_.size() + offset) % apps_.size();
            draw_app_card(buffer->pixels, xs[slot], ys[slot], apps_[index], slot == 1,
                          is_app_running(apps_[index]));
        }
        auto now = std::chrono::steady_clock::now();
        if (!status_message_.empty() && now < status_until_) {
            draw_text_centered(buffer->pixels, kWidth / 2, 128, status_message_, kWarn, 1);
        } else {
            if (!status_message_.empty()) {
                status_message_.clear();
            }
            draw_text_centered(buffer->pixels, kWidth / 2, 128, label_for(apps_[current_index_], 12), kInk, 1);
        }
    }
    if (task_view_) {
        draw_task_panel(buffer->pixels);
    }

    wl_surface_attach(surface_, buffer->buffer, 0, 0);
    wl_surface_damage_buffer(surface_, 0, 0, kWidth, kHeight);
    wl_surface_commit(surface_);
    buffer->busy = true;
    dirty_ = false;
}

int WaylandShell::run()
{
    auto last_tick = std::chrono::steady_clock::now();
    while (running_ && wl_display_get_error(display_) == 0) {
        process_commands();
        reload_apps_if_changed();
        refresh_tasks(false);
        update_launch_status();
        wl_display_dispatch_pending(display_);
        auto now = std::chrono::steady_clock::now();
        if (now - last_tick >= std::chrono::seconds(5)) {
            dirty_ = true;
            last_tick = now;
        }
        if (!status_message_.empty() && now >= status_until_) {
            status_message_.clear();
            dirty_ = true;
        }
        if (dirty_) {
            render();
        }
        wl_display_flush(display_);

        pollfd pfd{};
        pfd.fd = wl_display_get_fd(display_);
        pfd.events = POLLIN;
        int rc = poll(&pfd, 1, 100);
        if (rc > 0 && (pfd.revents & POLLIN)) {
            wl_display_dispatch(display_);
        }
    }
    return 0;
}

} // namespace

int main()
{
    WaylandShell shell;
    if (!shell.init()) {
        return 1;
    }
    return shell.run();
}
