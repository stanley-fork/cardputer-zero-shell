#include "zero_shell/shell_ui.hpp"

#include "zero_shell/process_runner.hpp"
#include "zero_shell/pty_terminal.hpp"

#include <algorithm>
#include <chrono>
#include <filesystem>
#include <iostream>
#include <thread>

namespace zero_shell {
namespace {

constexpr Color kBg{7, 10, 14};
constexpr Color kHeader{14, 18, 25};
constexpr Color kPanel{24, 30, 40};
constexpr Color kPanelDim{15, 19, 26};
constexpr Color kBorder{78, 90, 110};
constexpr Color kAccent{255, 204, 51};
constexpr Color kText{238, 242, 246};
constexpr Color kMuted{128, 146, 166};
constexpr Color kOk{80, 205, 124};
constexpr Color kWarn{245, 156, 75};
constexpr Color kDanger{238, 80, 80};

std::filesystem::file_time_type catalog_mtime(const std::filesystem::path &dir)
{
    if (!std::filesystem::is_directory(dir)) {
        return {};
    }

    std::filesystem::file_time_type latest = std::filesystem::last_write_time(dir);
    for (const auto &entry : std::filesystem::directory_iterator(dir)) {
        if (!entry.is_regular_file() || entry.path().extension() != ".desktop") {
            continue;
        }
        auto mtime = std::filesystem::last_write_time(entry.path());
        if (mtime > latest) {
            latest = mtime;
        }
    }
    return latest;
}

std::string fit_text(std::string text, size_t max_chars)
{
    if (text.size() <= max_chars) {
        return text;
    }
    if (max_chars <= 3) {
        return text.substr(0, max_chars);
    }
    return text.substr(0, max_chars - 3) + "...";
}

} // namespace

ShellUi::ShellUi(FramebufferCanvas &canvas, InputDevice &input, AppCatalog catalog)
    : canvas_(canvas), input_(input), catalog_(std::move(catalog))
{
}

int ShellUi::run()
{
    reload_apps();
    update_status_if_needed(true);
    render();

    while (running_) {
        auto current_mtime = catalog_mtime(catalog_.applications_dir());
        if (current_mtime != last_catalog_mtime_) {
            reload_apps();
            render();
        }

        update_status_if_needed();
        Key key = input_.poll(std::chrono::milliseconds(100));
        if (key != Key::None) {
            handle_key(key);
            render();
        }
    }

    return 0;
}

void ShellUi::reload_apps()
{
    apps_ = catalog_.load();
    if (current_index_ >= apps_.size()) {
        current_index_ = apps_.empty() ? 0 : apps_.size() - 1;
    }
    last_catalog_mtime_ = catalog_mtime(catalog_.applications_dir());
}

void ShellUi::render()
{
    canvas_.clear(kBg);
    if (apps_.empty()) {
        render_empty();
    } else {
        render_home();
    }
    if (power_menu_ == PowerMenuState::Open) {
        render_power_menu();
    }
    canvas_.present();
}

void ShellUi::render_home()
{
    canvas_.fill_rect(0, 0, canvas_.width(), 22, kHeader);
    canvas_.draw_text(6, 6, "ZeroShell", kText, 1);

    int right_x = canvas_.width() - 112;
    std::string wifi = status_.wifi_connected ? "WiFi" : "----";
    canvas_.draw_text(right_x, 6, wifi, status_.wifi_connected ? kOk : kMuted, 1);

    std::string bat = status_.battery_valid ? ("Bat " + std::to_string(status_.battery_percent) + "%") : "Bat --";
    canvas_.draw_text(right_x + 34, 6, bat, status_.battery_valid ? kText : kMuted, 1);

    canvas_.draw_text(canvas_.width() - 34, 6, status_.time, kMuted, 1);
    canvas_.fill_rect(0, 22, canvas_.width(), 1, kBorder);

    const int cy = 78;
    const int center_x = canvas_.width() / 2;
    const int positions[5] = {center_x - 144, center_x - 78, center_x, center_x + 78, center_x + 144};
    const int sizes[5] = {34, 50, 70, 50, 34};
    const size_t count = apps_.size();

    for (int slot = 0; slot < 5; ++slot) {
        int offset = slot - 2;
        size_t index = (current_index_ + count + offset) % count;
        const auto &app = apps_[index];
        bool center = slot == 2;
        canvas_.draw_icon_tile(positions[slot], cy, sizes[slot],
                               center ? Color{49, 58, 75} : kPanelDim,
                               center ? kAccent : kBorder,
                               app.name);
        if (slot != 0 && slot != 4) {
            canvas_.draw_text_centered(positions[slot], cy + sizes[slot] / 2 + 8,
                                       fit_text(app.name, center ? 14 : 9),
                                       center ? kText : kMuted,
                                       1);
        }
    }

    if (const AppEntry *app = current_app()) {
        std::string mode = app->terminal ? "Terminal" : "External";
        canvas_.draw_text_centered(center_x, 132, mode, app->terminal ? kOk : kWarn, 1);
    }

    canvas_.fill_rect(0, canvas_.height() - 20, canvas_.width(), 1, kBorder);
    canvas_.draw_text(6, canvas_.height() - 14, "LEFT/RIGHT Select", kMuted, 1);
    canvas_.draw_text(132, canvas_.height() - 14, "ENTER Open", kText, 1);
    canvas_.draw_text(canvas_.width() - 62, canvas_.height() - 14, "ESC Power", kMuted, 1);
}

void ShellUi::render_empty()
{
    canvas_.fill_rect(0, 0, canvas_.width(), 22, kHeader);
    canvas_.draw_text(6, 6, "ZeroShell", kText, 1);
    canvas_.draw_text_centered(canvas_.width() / 2, 58, "No APPLaunch apps", kText, 1);
    canvas_.draw_text_centered(canvas_.width() / 2, 78, "/usr/share/APPLaunch/applications", kMuted, 1);
    canvas_.draw_text_centered(canvas_.width() / 2, 106, "Press R to reload", kAccent, 1);
}

void ShellUi::render_power_menu()
{
    int x = 52;
    int y = 40;
    int w = canvas_.width() - 104;
    int h = 94;

    canvas_.fill_rect(x, y, w, h, {10, 12, 18});
    canvas_.draw_rect(x, y, w, h, kAccent);
    canvas_.draw_text_centered(canvas_.width() / 2, y + 10, "Power", kText, 1);

    const char *items[] = {"Cancel", "Reboot", "Shutdown"};
    for (int i = 0; i < 3; ++i) {
        Color color = i == power_selection_ ? kBg : Color{10, 12, 18};
        Color text = i == power_selection_ ? kAccent : kText;
        canvas_.fill_rect(x + 18, y + 28 + i * 18, w - 36, 14, color);
        canvas_.draw_text_centered(canvas_.width() / 2, y + 31 + i * 18, items[i], text, 1);
    }
}

void ShellUi::launch_current()
{
    const AppEntry *app = current_app();
    if (!app) {
        return;
    }

    canvas_.clear(kBg);
    canvas_.draw_text_centered(canvas_.width() / 2, 62, "Launching", kText, 1);
    canvas_.draw_text_centered(canvas_.width() / 2, 82, fit_text(app->name, 18), kAccent, 1);
    canvas_.present();

    int rc = app->terminal
        ? run_terminal_page(canvas_, input_, app->exec, app->sysplause)
        : run_blocking_command(app->exec);

    update_status_if_needed(true);
    canvas_.clear(kBg);
    canvas_.draw_text_centered(canvas_.width() / 2, 68, "Returned", kText, 1);
    canvas_.draw_text_centered(canvas_.width() / 2, 88, "exit " + std::to_string(rc), kMuted, 1);
    canvas_.present();
    std::this_thread::sleep_for(std::chrono::milliseconds(450));
}

void ShellUi::move_left()
{
    if (apps_.empty()) {
        return;
    }
    current_index_ = (current_index_ + apps_.size() - 1) % apps_.size();
}

void ShellUi::move_right()
{
    if (apps_.empty()) {
        return;
    }
    current_index_ = (current_index_ + 1) % apps_.size();
}

void ShellUi::handle_key(Key key)
{
    if (power_menu_ == PowerMenuState::Open) {
        if (key == Key::Escape) {
            power_menu_ = PowerMenuState::Hidden;
            return;
        }
        if (key == Key::Up) {
            power_selection_ = (power_selection_ + 2) % 3;
            return;
        }
        if (key == Key::Down || key == Key::Left || key == Key::Right) {
            power_selection_ = (power_selection_ + 1) % 3;
            return;
        }
        if (key == Key::Enter) {
            if (power_selection_ == 0) {
                power_menu_ = PowerMenuState::Hidden;
            } else if (power_selection_ == 1) {
                run_zero_helper("reboot");
            } else if (power_selection_ == 2) {
                run_zero_helper("shutdown");
            }
            return;
        }
        return;
    }

    switch (key) {
    case Key::Left:
        move_left();
        break;
    case Key::Right:
        move_right();
        break;
    case Key::Enter:
        launch_current();
        break;
    case Key::R:
        reload_apps();
        break;
    case Key::Escape:
    case Key::Power:
        power_menu_ = PowerMenuState::Open;
        power_selection_ = 0;
        break;
    case Key::Q:
        running_ = false;
        break;
    default:
        break;
    }
}

void ShellUi::update_status_if_needed(bool force)
{
    auto now = std::chrono::steady_clock::now();
    if (!force && now - last_status_update_ < std::chrono::seconds(5)) {
        return;
    }
    status_ = read_status();
    last_status_update_ = now;
    if (!force) {
        render();
    }
}

const AppEntry *ShellUi::current_app() const
{
    if (apps_.empty()) {
        return nullptr;
    }
    return &apps_[current_index_];
}

} // namespace zero_shell
