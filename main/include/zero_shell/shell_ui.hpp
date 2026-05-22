#pragma once

#include "zero_shell/app_catalog.hpp"
#include "zero_shell/framebuffer_canvas.hpp"
#include "zero_shell/input_device.hpp"
#include "zero_shell/status.hpp"

#include <chrono>
#include <filesystem>
#include <string>
#include <vector>

namespace zero_shell {

class ShellUi {
public:
    ShellUi(FramebufferCanvas &canvas, InputDevice &input, AppCatalog catalog);

    int run();

private:
    enum class PowerMenuState {
        Hidden,
        Open,
    };

    void reload_apps();
    void render();
    void render_home();
    void render_empty();
    void render_power_menu();
    void launch_current();
    void move_left();
    void move_right();
    void handle_key(Key key);
    void update_status_if_needed(bool force = false);
    const AppEntry *current_app() const;

    FramebufferCanvas &canvas_;
    InputDevice &input_;
    AppCatalog catalog_;
    std::vector<AppEntry> apps_;
    StatusSnapshot status_;
    std::chrono::steady_clock::time_point last_status_update_{};
    std::filesystem::file_time_type last_catalog_mtime_{};
    size_t current_index_ = 0;
    PowerMenuState power_menu_ = PowerMenuState::Hidden;
    int power_selection_ = 0;
    bool running_ = true;
};

} // namespace zero_shell

