#include "zero_shell/app_catalog.hpp"
#include "zero_shell/framebuffer_canvas.hpp"
#include "zero_shell/input_device.hpp"
#include "zero_shell/shell_ui.hpp"

#include <cstdlib>
#include <filesystem>
#include <iostream>
#include <string>
#include <unistd.h>

namespace {

std::string getenv_or(const char *name, const std::string &fallback)
{
    const char *value = std::getenv(name);
    return value && *value ? value : fallback;
}

} // namespace

int main()
{
    if (geteuid() == 0) {
        std::cerr << "zero-shell: refusing to run as root. Start it from a logged-in user session.\n";
        return 77;
    }

    std::string fbdev = zero_shell::find_internal_framebuffer();
    zero_shell::FramebufferCanvas canvas;
    if (!canvas.open(fbdev)) {
        return 1;
    }

    std::string keyboard = zero_shell::find_keyboard_device();
    zero_shell::InputDevice input;
    if (!input.open(keyboard)) {
        canvas.clear({7, 10, 14});
        canvas.draw_text_centered(canvas.width() / 2, 64, "Keyboard unavailable", {238, 242, 246}, 1);
        canvas.draw_text_centered(canvas.width() / 2, 84, keyboard, {128, 146, 166}, 1);
        canvas.present();
    }

    std::string applications_dir =
        getenv_or("ZEROSHELL_APPLICATIONS_DIR", "/usr/share/APPLaunch/applications");

    zero_shell::AppCatalog catalog(applications_dir);
    zero_shell::ShellUi ui(canvas, input, std::move(catalog));
    return ui.run();
}

