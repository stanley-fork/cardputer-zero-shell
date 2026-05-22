#include "zero_shell/status.hpp"

#include <algorithm>
#include <chrono>
#include <cstdio>
#include <ctime>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <string>

namespace zero_shell {
namespace {

bool read_int_file(const std::filesystem::path &path, int &out)
{
    std::ifstream file(path);
    if (!file) {
        return false;
    }
    file >> out;
    return file.good() || file.eof();
}

int read_battery_percent()
{
    const std::filesystem::path root = "/sys/class/power_supply";
    if (!std::filesystem::is_directory(root)) {
        return -1;
    }

    for (const auto &entry : std::filesystem::directory_iterator(root)) {
        int capacity = -1;
        if (read_int_file(entry.path() / "capacity", capacity)) {
            return std::clamp(capacity, 0, 100);
        }
    }
    return -1;
}

bool run_command_line(const char *command, std::string &out)
{
    FILE *pipe = popen(command, "r");
    if (!pipe) {
        return false;
    }

    char buffer[256];
    out.clear();
    while (fgets(buffer, sizeof(buffer), pipe)) {
        out += buffer;
    }
    int rc = pclose(pipe);
    return rc == 0;
}

int read_wifi_signal()
{
    std::ifstream file("/proc/net/wireless");
    std::string line;
    int row = 0;
    while (std::getline(file, line)) {
        if (++row <= 2) {
            continue;
        }
        auto colon = line.find(':');
        if (colon == std::string::npos) {
            continue;
        }

        std::istringstream stream(line.substr(colon + 1));
        std::string status;
        float link = 0.0f;
        stream >> status >> link;
        if (stream) {
            return std::clamp(static_cast<int>((link / 70.0f) * 100.0f), 0, 100);
        }
    }

    std::string iwgetid;
    if (run_command_line("iwgetid -r 2>/dev/null", iwgetid) && !iwgetid.empty()) {
        return 50;
    }

    return -1;
}

std::string current_time()
{
    auto now = std::chrono::system_clock::now();
    std::time_t raw = std::chrono::system_clock::to_time_t(now);
    std::tm local{};
    localtime_r(&raw, &local);
    char buffer[16];
    std::snprintf(buffer, sizeof(buffer), "%02d:%02d", local.tm_hour, local.tm_min);
    return buffer;
}

} // namespace

StatusSnapshot read_status()
{
    StatusSnapshot status;
    status.time = current_time();

    status.wifi_signal = read_wifi_signal();
    status.wifi_connected = status.wifi_signal >= 0;

    status.battery_percent = read_battery_percent();
    status.battery_valid = status.battery_percent >= 0;

    return status;
}

} // namespace zero_shell
