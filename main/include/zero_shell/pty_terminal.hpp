#pragma once

#include "zero_shell/framebuffer_canvas.hpp"
#include "zero_shell/input_device.hpp"

#include <string>

namespace zero_shell {

int run_terminal_page(FramebufferCanvas &canvas,
                      InputDevice &input,
                      const std::string &command,
                      bool sysplause);

} // namespace zero_shell

