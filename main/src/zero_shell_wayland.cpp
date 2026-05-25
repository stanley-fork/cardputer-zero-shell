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
#include <set>
#include <string>
#include <sys/socket.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/syscall.h>
#include <sys/types.h>
#include <sys/un.h>
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
    std::string id;
    std::string app_id;
    std::string title;
    bool activated = false;
    bool minimized = false;
    bool maximized = false;
    bool fullscreen = false;
};

bool same_tasks(const std::vector<WindowTask> &left, const std::vector<WindowTask> &right)
{
    if (left.size() != right.size()) {
        return false;
    }
    for (size_t i = 0; i < left.size(); ++i) {
        if (left[i].id != right[i].id ||
            left[i].app_id != right[i].app_id ||
            left[i].title != right[i].title ||
            left[i].activated != right[i].activated ||
            left[i].minimized != right[i].minimized ||
            left[i].maximized != right[i].maximized ||
            left[i].fullscreen != right[i].fullscreen) {
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

std::string category_label(std::string value)
{
    value = display_ascii(std::move(value));
    value = uppercase_ascii(value);
    if (value.empty()) {
        value = "OTHER";
    }
    if (value.size() > 12) {
        value.resize(12);
    }
    return value;
}

std::string category_key(std::string value)
{
    value.erase(value.begin(), std::find_if(value.begin(), value.end(), [](unsigned char ch) {
        return !std::isspace(ch);
    }));
    value.erase(std::find_if(value.rbegin(), value.rend(), [](unsigned char ch) {
        return !std::isspace(ch);
    }).base(), value.end());
    return value;
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

bool set_nonblock(int fd)
{
    int flags = fcntl(fd, F_GETFL, 0);
    if (flags < 0) {
        return false;
    }
    return fcntl(fd, F_SETFL, flags | O_NONBLOCK) == 0;
}

std::string window_agent_socket_path()
{
    const char *path = std::getenv("ZERO_WINDOW_AGENT_SOCKET");
    if (path && *path) {
        return path;
    }
    const char *runtime = std::getenv("XDG_RUNTIME_DIR");
    if (runtime && *runtime) {
        return std::string(runtime) + "/cardputer-zero/window-agent.sock";
    }
    return {};
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
    return display == "wayland" || display == "xwayland";
}

uint16_t normalize_wayland_key(uint32_t key)
{
    // wl_keyboard.key is already a Linux evdev keycode, unlike X11 keycodes.
    return static_cast<uint16_t>(key);
}

int hex_value(char ch)
{
    if (ch >= '0' && ch <= '9') {
        return ch - '0';
    }
    if (ch >= 'A' && ch <= 'F') {
        return ch - 'A' + 10;
    }
    if (ch >= 'a' && ch <= 'f') {
        return ch - 'a' + 10;
    }
    return -1;
}

std::string decode_agent_field(const std::string &value)
{
    std::string output;
    output.reserve(value.size());
    for (size_t i = 0; i < value.size(); ++i) {
        if (value[i] == '%' && i + 2 < value.size()) {
            int hi = hex_value(value[i + 1]);
            int lo = hex_value(value[i + 2]);
            if (hi >= 0 && lo >= 0) {
                output.push_back(static_cast<char>((hi << 4) | lo));
                i += 2;
                continue;
            }
        }
        output.push_back(value[i]);
    }
    return output;
}

std::vector<std::string> split_agent_fields(const std::string &line)
{
    std::vector<std::string> fields;
    size_t start = 0;
    while (start <= line.size()) {
        size_t tab = line.find('\t', start);
        if (tab == std::string::npos) {
            fields.push_back(decode_agent_field(line.substr(start)));
            break;
        }
        fields.push_back(decode_agent_field(line.substr(start, tab - start)));
        start = tab + 1;
    }
    return fields;
}

bool state_has(const std::string &states, const std::string &needle)
{
    size_t start = 0;
    while (start <= states.size()) {
        size_t comma = states.find(',', start);
        std::string value = comma == std::string::npos
            ? states.substr(start)
            : states.substr(start, comma - start);
        if (value == needle) {
            return true;
        }
        if (comma == std::string::npos) {
            break;
        }
        start = comma + 1;
    }
    return false;
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
    void rebuild_categories();
    void apply_category_filter();
    const zero_shell::AppEntry *selected_app() const;
    void init_task_backend();
    void reconnect_task_backend();
    void read_task_backend();
    void flush_task_backend();
    void handle_agent_line(const std::string &line);
    void apply_agent_snapshot();
    void send_agent_command(const std::string &command);
    void process_commands();
    void update_launch_status();
    void set_status(std::string message, std::chrono::milliseconds duration);
    void handle_key(uint16_t key);
    void focus_task(size_t task_index);
    void render();
    void draw_frame(uint32_t *pixels);
    void draw_task_panel(uint32_t *pixels);
    void draw_category_drawer(uint32_t *pixels);
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
    bool category_view_ = false;
    std::string status_message_;
    std::chrono::steady_clock::time_point status_until_{};
    std::FILE *debug_file_ = nullptr;
    std::filesystem::path applications_dir_;
    std::filesystem::file_time_type last_catalog_mtime_{};
    std::vector<zero_shell::AppEntry> apps_;
    std::vector<const zero_shell::AppEntry *> visible_apps_;
    std::vector<std::string> categories_{"All"};
    size_t category_selection_ = 0;
    std::vector<WindowTask> tasks_;
    std::vector<WindowTask> pending_tasks_;
    bool reading_task_snapshot_ = false;
    bool task_backend_online_ = false;
    int task_backend_fd_ = -1;
    std::string task_backend_input_;
    std::string task_backend_output_;
    std::chrono::steady_clock::time_point next_task_backend_reconnect_{};
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
    init_task_backend();
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
    rebuild_categories();
    apply_category_filter();
    last_catalog_mtime_ = catalog_mtime(applications_dir_);
    dirty_ = true;
}

void WaylandShell::rebuild_categories()
{
    std::string selected = category_selection_ < categories_.size() ? categories_[category_selection_] : "All";
    std::set<std::string> seen;
    std::vector<std::string> next{"All"};
    for (const auto &app : apps_) {
        if (app.categories.empty()) {
            if (seen.insert("Other").second) {
                next.push_back("Other");
            }
            continue;
        }
        for (const auto &category : app.categories) {
            std::string key = category_key(category);
            if (key.empty() || lowercase_ascii(key) == "all") {
                continue;
            }
            if (seen.insert(key).second) {
                next.push_back(key);
            }
        }
    }

    categories_ = std::move(next);
    auto it = std::find(categories_.begin(), categories_.end(), selected);
    category_selection_ = it == categories_.end()
        ? 0
        : static_cast<size_t>(std::distance(categories_.begin(), it));
}

void WaylandShell::apply_category_filter()
{
    visible_apps_.clear();
    std::string selected = category_selection_ < categories_.size() ? categories_[category_selection_] : "All";
    for (const auto &app : apps_) {
        bool include = selected == "All";
        if (!include && app.categories.empty()) {
            include = selected == "Other";
        }
        if (!include) {
            include = std::any_of(app.categories.begin(), app.categories.end(), [&](const std::string &category) {
                return category_key(category) == selected;
            });
        }
        if (include) {
            visible_apps_.push_back(&app);
        }
    }
    if (current_index_ >= visible_apps_.size()) {
        current_index_ = visible_apps_.empty() ? 0 : visible_apps_.size() - 1;
    }
}

const zero_shell::AppEntry *WaylandShell::selected_app() const
{
    if (current_index_ >= visible_apps_.size()) {
        return nullptr;
    }
    return visible_apps_[current_index_];
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

void WaylandShell::init_task_backend()
{
    reconnect_task_backend();
}

void WaylandShell::reconnect_task_backend()
{
    auto now = std::chrono::steady_clock::now();
    if (task_backend_fd_ >= 0 || now < next_task_backend_reconnect_) {
        return;
    }
    next_task_backend_reconnect_ = now + std::chrono::seconds(1);

    int fd = socket(AF_UNIX, SOCK_STREAM | SOCK_CLOEXEC, 0);
    if (fd < 0) {
        task_backend_online_ = false;
        return;
    }

    std::string path = window_agent_socket_path();
    sockaddr_un addr{};
    addr.sun_family = AF_UNIX;
    if (path.size() >= sizeof(addr.sun_path)) {
        close(fd);
        task_backend_online_ = false;
        return;
    }
    std::strncpy(addr.sun_path, path.c_str(), sizeof(addr.sun_path) - 1);

    if (connect(fd, reinterpret_cast<sockaddr *>(&addr), sizeof(addr)) < 0) {
        close(fd);
        if (task_backend_online_ || !tasks_.empty()) {
            task_backend_online_ = false;
            tasks_.clear();
            dirty_ = true;
        }
        return;
    }

    if (!set_nonblock(fd)) {
        close(fd);
        task_backend_online_ = false;
        return;
    }

    task_backend_fd_ = fd;
    task_backend_online_ = true;
    task_backend_input_.clear();
    task_backend_output_.clear();
    reading_task_snapshot_ = false;
    pending_tasks_.clear();
    send_agent_command("hello");
    send_agent_command("subscribe");
    dirty_ = true;
}

void WaylandShell::send_agent_command(const std::string &command)
{
    if (task_backend_fd_ < 0) {
        return;
    }
    task_backend_output_ += command;
    task_backend_output_.push_back('\n');
}

void WaylandShell::flush_task_backend()
{
    while (task_backend_fd_ >= 0 && !task_backend_output_.empty()) {
        ssize_t count = write(task_backend_fd_, task_backend_output_.data(), task_backend_output_.size());
        if (count > 0) {
            task_backend_output_.erase(0, static_cast<size_t>(count));
        } else if (count < 0 && errno == EINTR) {
            continue;
        } else if (count < 0 && (errno == EAGAIN || errno == EWOULDBLOCK)) {
            return;
        } else {
            close(task_backend_fd_);
            task_backend_fd_ = -1;
            task_backend_online_ = false;
            tasks_.clear();
            dirty_ = true;
            return;
        }
    }
}

void WaylandShell::read_task_backend()
{
    reconnect_task_backend();
    if (task_backend_fd_ < 0) {
        return;
    }

    char buffer[1024];
    while (true) {
        ssize_t count = read(task_backend_fd_, buffer, sizeof(buffer));
        if (count > 0) {
            task_backend_input_.append(buffer, static_cast<size_t>(count));
            size_t newline = std::string::npos;
            while ((newline = task_backend_input_.find('\n')) != std::string::npos) {
                std::string line = task_backend_input_.substr(0, newline);
                if (!line.empty() && line.back() == '\r') {
                    line.pop_back();
                }
                task_backend_input_.erase(0, newline + 1);
                handle_agent_line(line);
            }
        } else if (count == 0) {
            close(task_backend_fd_);
            task_backend_fd_ = -1;
            task_backend_online_ = false;
            tasks_.clear();
            dirty_ = true;
            return;
        } else {
            if (errno == EINTR) {
                continue;
            }
            if (errno == EAGAIN || errno == EWOULDBLOCK) {
                return;
            }
            close(task_backend_fd_);
            task_backend_fd_ = -1;
            task_backend_online_ = false;
            tasks_.clear();
            dirty_ = true;
            return;
        }
    }
}

void WaylandShell::handle_agent_line(const std::string &line)
{
    auto fields = split_agent_fields(line);
    if (fields.empty()) {
        return;
    }
    if (fields[0] == "snapshot-begin") {
        reading_task_snapshot_ = true;
        pending_tasks_.clear();
    } else if (fields[0] == "task" && reading_task_snapshot_ && fields.size() >= 5) {
        WindowTask task;
        task.id = fields[1];
        task.app_id = fields[2];
        task.title = fields[3];
        task.activated = state_has(fields[4], "activated");
        task.minimized = state_has(fields[4], "minimized");
        task.maximized = state_has(fields[4], "maximized");
        task.fullscreen = state_has(fields[4], "fullscreen");
        if (task.app_id != "cardputer-zero-shell" && task.title != "Cardputer Zero Shell") {
            pending_tasks_.push_back(std::move(task));
        }
    } else if (fields[0] == "snapshot-end" && reading_task_snapshot_) {
        apply_agent_snapshot();
    }
}

void WaylandShell::apply_agent_snapshot()
{
    reading_task_snapshot_ = false;
    bool changed = !same_tasks(tasks_, pending_tasks_);
    size_t old_selection = task_selection_;
    tasks_ = std::move(pending_tasks_);
    pending_tasks_.clear();
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
            task_view_ = true;
            category_view_ = false;
            dirty_ = true;
        } else if (command == "toggle-tasks") {
            task_view_ = !task_view_;
            if (task_view_) {
                category_view_ = false;
            }
            dirty_ = true;
        } else if (command == "hide-tasks") {
            if (task_view_) {
                task_view_ = false;
                dirty_ = true;
            }
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

    if (is_app_running(launch_pending_app_)) {
        launch_pending_ = false;
        status_message_.clear();
        dirty_ = true;
        return;
    }

    if (now >= launch_deadline_) {
        launch_pending_ = false;
        if (status_message_ == "LOADING") {
            if (task_backend_online_) {
                status_message_.clear();
            } else {
                status_message_ = "WINDOW AGENT OFFLINE";
                status_until_ = now + std::chrono::seconds(2);
            }
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
            task_view_ = false;
            dirty_ = true;
            return;
        case KEY_LEFT:
        case KEY_UP:
            if (!tasks_.empty()) {
                task_selection_ = (task_selection_ + tasks_.size() - 1) % tasks_.size();
            }
            dirty_ = true;
            return;
        case KEY_RIGHT:
        case KEY_DOWN:
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

    if (category_view_) {
        switch (key) {
        case KEY_C:
        case KEY_ESC:
            category_view_ = false;
            dirty_ = true;
            return;
        case KEY_LEFT:
        case KEY_UP:
            if (!categories_.empty()) {
                category_selection_ = (category_selection_ + categories_.size() - 1) % categories_.size();
                current_index_ = 0;
                apply_category_filter();
            }
            dirty_ = true;
            return;
        case KEY_RIGHT:
        case KEY_DOWN:
            if (!categories_.empty()) {
                category_selection_ = (category_selection_ + 1) % categories_.size();
                current_index_ = 0;
                apply_category_filter();
            }
            dirty_ = true;
            return;
        case KEY_ENTER:
            category_view_ = false;
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
        category_view_ = false;
        if (!visible_apps_.empty()) {
            current_index_ = (current_index_ + visible_apps_.size() - 1) % visible_apps_.size();
            dirty_ = true;
        }
        break;
    case KEY_RIGHT:
        if (debug_) {
            debug_log("action next");
        }
        task_view_ = false;
        category_view_ = false;
        if (!visible_apps_.empty()) {
            current_index_ = (current_index_ + 1) % visible_apps_.size();
            dirty_ = true;
        }
        break;
    case KEY_ENTER:
        if (debug_) {
            debug_log("action launch");
        }
        task_view_ = false;
        category_view_ = false;
        if (const zero_shell::AppEntry *selected = selected_app()) {
            const auto &app = *selected;
            if (can_launch_in_wayland(app)) {
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
        task_view_ = !task_view_;
        category_view_ = false;
        dirty_ = true;
        break;
    case KEY_C:
        task_view_ = false;
        category_view_ = !category_view_;
        dirty_ = true;
        break;
    case KEY_R:
        task_view_ = false;
        category_view_ = false;
        load_apps();
        break;
    case KEY_ESC:
        break;
    default:
        break;
    }
}

void WaylandShell::focus_task(size_t task_index)
{
    if (task_index >= tasks_.size()) {
        return;
    }

    const auto &task = tasks_[task_index];
    if (!task.id.empty()) {
        send_agent_command("activate\t" + task.id);
        flush_task_backend();
    }
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

    fill_rect(pixels, 1, y + 1, 100, kBarHeight - 2, kTaskButton);
    fill_rect(pixels, 6, y + 4, 24, 11, kIconWell);
    draw_rect(pixels, 6, y + 4, 24, 11, kInk);
    draw_text(pixels, 9, y + 6, "TAB", kAccent, 1);
    draw_text(pixels, 36, y + 6, "TASK", kInk, 1);
    fill_rect(pixels, 72, y + 4, 17, 11, kAccent);
    draw_rect(pixels, 72, y + 4, 17, 11, kInk);
    draw_text(pixels, 76, y + 6, std::to_string(tasks_.size()), kInk, 1);

    fill_rect(pixels, 102, y + 1, 90, kBarHeight - 2, kPanel);
    fill_rect(pixels, 101, y, 1, kBarHeight, kLine);
    fill_rect(pixels, 110, y + 4, 12, 11, kIconWell);
    draw_rect(pixels, 110, y + 4, 12, 11, kInk);
    draw_text(pixels, 114, y + 6, "C", kAccent, 1);
    draw_text(pixels, 129, y + 6, "CATEGORY", kInk, 1);

    fill_rect(pixels, 193, y + 1, 126, kBarHeight - 2, kPanel);
    fill_rect(pixels, 192, y, 1, kBarHeight, kLine);
    std::string category = category_selection_ < categories_.size() ? categories_[category_selection_] : "All";
    category = category_label(category);
    if (category.size() > 13) {
        category.resize(13);
    }
    draw_text(pixels, 205, y + 6, category, kAccent, 1);
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
        draw_text(pixels, x + 8, y + 24,
                  task_backend_online_ ? "NO TASKS" : "AGENT OFFLINE",
                  task_backend_online_ ? kMuted : kWarn, 1);
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

void WaylandShell::draw_category_drawer(uint32_t *pixels)
{
    int w = 124;
    int h = categories_.empty() ? 38 : std::min(116, 20 + static_cast<int>(categories_.size()) * 15);
    int x = kWidth - w;
    int y = kHeight - kBarHeight - h;

    fill_rect(pixels, x + 3, y + 3, w, h, kShadow);
    fill_rect(pixels, x, y, w, h, kPanel);
    draw_rect(pixels, x, y, w, h, kLine);
    fill_rect(pixels, x, y, w, 17, kInk);
    draw_text(pixels, x + 7, y + 5, "CATEGORIES", kPanel, 1);

    if (categories_.empty()) {
        draw_text(pixels, x + 8, y + 24, "NO CATEGORIES", kMuted, 1);
        return;
    }

    size_t visible = std::min<size_t>(categories_.size(), 6);
    size_t start = 0;
    if (category_selection_ >= visible) {
        start = category_selection_ - visible + 1;
    }
    if (start + visible > categories_.size()) {
        start = categories_.size() - visible;
    }

    for (size_t row = 0; row < visible; ++row) {
        size_t index = start + row;
        int item_y = y + 20 + static_cast<int>(row) * 15;
        bool selected = index == category_selection_;
        std::string label = category_label(categories_[index]);
        if (label.size() > 12) {
            label.resize(12);
        }
        size_t count = 0;
        if (categories_[index] == "All") {
            count = apps_.size();
        } else {
            count = std::count_if(apps_.begin(), apps_.end(), [&](const zero_shell::AppEntry &app) {
                if (app.categories.empty()) {
                    return categories_[index] == "Other";
                }
                return std::any_of(app.categories.begin(), app.categories.end(), [&](const std::string &category) {
                    return category_key(category) == categories_[index];
                });
            });
        }
        fill_rect(pixels, x + 6, item_y, w - 12, 13, selected ? kAccent : kPanel);
        draw_rect(pixels, x + 6, item_y, w - 12, 13, selected ? kInk : kSoftLine);
        draw_text(pixels, x + 10, item_y + 3, selected ? ">" : " ", selected ? kInk : kMuted, 1);
        draw_text(pixels, x + 21, item_y + 3, label, selected ? kInk : kMuted, 1);
        draw_text(pixels, x + w - 24, item_y + 3, std::to_string(count), selected ? kInk : kMuted, 1);
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

    draw_frame(buffer->pixels);
    if (apps_.empty()) {
        draw_text_centered(buffer->pixels, kWidth / 2, 58, "NO APPS", kInk, 1);
        draw_text_centered(buffer->pixels, kWidth / 2, 78, "APPLaunch/applications", kMuted, 1);
        draw_text_centered(buffer->pixels, kWidth / 2, 104, "PRESS R", kAccent, 1);
    } else if (visible_apps_.empty()) {
        draw_text_centered(buffer->pixels, kWidth / 2, 58, "NO APPS", kInk, 1);
        draw_text_centered(buffer->pixels, kWidth / 2, 78,
                           category_selection_ < categories_.size() ? category_label(categories_[category_selection_]) : "CATEGORY",
                           kAccent, 1);
        draw_text_centered(buffer->pixels, kWidth / 2, 104, "PRESS C", kMuted, 1);
    } else {
        const int xs[3] = {42, 119, 196};
        const int ys[3] = {50, 42, 50};
        if (visible_apps_.size() == 1) {
            const zero_shell::AppEntry &app = *visible_apps_[0];
            draw_app_card(buffer->pixels, xs[1], ys[1], app, true, is_app_running(app));
        } else if (visible_apps_.size() == 2) {
            const int slot_order[3] = {1, 0, 2};
            const int neighbor_slot = current_index_ == 0 ? 2 : 0;
            for (int wanted_slot : slot_order) {
                if (wanted_slot == 1) {
                    const zero_shell::AppEntry &app = *visible_apps_[current_index_];
                    draw_app_card(buffer->pixels, xs[1], ys[1], app, true, is_app_running(app));
                } else if (wanted_slot == neighbor_slot) {
                    const size_t neighbor_index = current_index_ == 0 ? 1 : 0;
                    const zero_shell::AppEntry &app = *visible_apps_[neighbor_index];
                    draw_app_card(buffer->pixels, xs[neighbor_slot], ys[neighbor_slot], app, false,
                                  is_app_running(app));
                }
            }
        } else {
            const int slot_order[3] = {1, 0, 2};
            for (int wanted_slot : slot_order) {
                const int offset = wanted_slot - 1;
                const int count = static_cast<int>(visible_apps_.size());
                const size_t index = static_cast<size_t>((static_cast<int>(current_index_) + count + offset) % count);
                const zero_shell::AppEntry &app = *visible_apps_[index];
                draw_app_card(buffer->pixels, xs[wanted_slot], ys[wanted_slot], app, wanted_slot == 1,
                              is_app_running(app));
            }
        }
        auto now = std::chrono::steady_clock::now();
        if (!status_message_.empty() && now < status_until_) {
            draw_text_centered(buffer->pixels, kWidth / 2, 128, status_message_, kWarn, 1);
        } else {
            if (!status_message_.empty()) {
                status_message_.clear();
            }
            draw_text_centered(buffer->pixels, kWidth / 2, 128, label_for(*visible_apps_[current_index_], 12), kInk, 1);
        }
    }
    if (task_view_) {
        draw_task_panel(buffer->pixels);
    }
    if (category_view_) {
        draw_category_drawer(buffer->pixels);
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
        read_task_backend();
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
        flush_task_backend();
        wl_display_flush(display_);

        pollfd pfds[2]{};
        pfds[0].fd = wl_display_get_fd(display_);
        pfds[0].events = POLLIN;
        int nfds = 1;
        if (task_backend_fd_ >= 0) {
            pfds[1].fd = task_backend_fd_;
            pfds[1].events = POLLIN;
            if (!task_backend_output_.empty()) {
                pfds[1].events |= POLLOUT;
            }
            nfds = 2;
        }

        int rc = poll(pfds, nfds, 100);
        if (rc > 0 && (pfds[0].revents & POLLIN)) {
            wl_display_dispatch(display_);
        }
        if (rc > 0 && nfds > 1) {
            if (pfds[1].revents & POLLIN) {
                read_task_backend();
            }
            if (pfds[1].revents & POLLOUT) {
                flush_task_backend();
            }
            if (pfds[1].revents & (POLLERR | POLLHUP | POLLNVAL)) {
                close(task_backend_fd_);
                task_backend_fd_ = -1;
                task_backend_online_ = false;
                tasks_.clear();
                dirty_ = true;
            }
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
