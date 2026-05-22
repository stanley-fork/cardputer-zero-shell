#pragma once

#include <string>

namespace zero_shell {

struct StatusSnapshot {
    std::string time;
    bool wifi_connected = false;
    int wifi_signal = -1;
    bool battery_valid = false;
    int battery_percent = -1;
};

StatusSnapshot read_status();

} // namespace zero_shell

