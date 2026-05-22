#pragma once

#include <string>

namespace zero_shell {

int run_blocking_command(const std::string &command);
int run_terminal_command(const std::string &command, bool sysplause);
int run_zero_helper(const std::string &arguments);

} // namespace zero_shell

