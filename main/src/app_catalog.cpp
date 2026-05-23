#include "zero_shell/app_catalog.hpp"

#include <algorithm>
#include <cctype>
#include <cstdlib>
#include <fstream>
#include <set>
#include <sstream>
#include <unordered_map>

namespace zero_shell {
namespace {

constexpr const char *kDefaultAppDir = "/usr/share/APPLaunch/applications";
constexpr const char *kDefaultDataDir = "/usr/share/APPLaunch";

std::string trim(std::string value)
{
    auto not_space = [](unsigned char ch) { return !std::isspace(ch); };
    value.erase(value.begin(), std::find_if(value.begin(), value.end(), not_space));
    value.erase(std::find_if(value.rbegin(), value.rend(), not_space).base(), value.end());
    return value;
}

std::string first_token(const std::string &command)
{
    std::istringstream stream(command);
    std::string token;
    stream >> token;
    return token;
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

bool parse_desktop_file(const std::filesystem::path &path, AppEntry &entry)
{
    std::ifstream file(path);
    if (!file) {
        return false;
    }

    bool in_desktop_entry = false;
    std::unordered_map<std::string, std::string> values;
    std::string line;

    while (std::getline(file, line)) {
        if (!line.empty() && line.back() == '\r') {
            line.pop_back();
        }

        line = trim(line);
        if (line.empty() || line[0] == '#' || line[0] == ';') {
            continue;
        }

        if (line.front() == '[' && line.back() == ']') {
            in_desktop_entry = (line == "[Desktop Entry]");
            continue;
        }

        if (!in_desktop_entry) {
            continue;
        }

        auto eq = line.find('=');
        if (eq == std::string::npos) {
            continue;
        }

        std::string key = trim(line.substr(0, eq));
        std::string value = trim(line.substr(eq + 1));
        values[key] = value;
    }

    entry.id = path.stem().string();
    entry.name = values["Name"];
    entry.exec = values["Exec"];
    entry.icon = values["Icon"];
    entry.try_exec = values["TryExec"];
    entry.short_name = values["X-Zero-ShortName"];
    entry.startup_wm_class = values["StartupWMClass"];
    entry.zero_app_id = values["X-Zero-AppId"];
    entry.zero_display = values["X-Zero-Display"];

    return !entry.name.empty() && !entry.exec.empty();
}

} // namespace

AppCatalog::AppCatalog(std::filesystem::path applications_dir)
    : applications_dir_(std::move(applications_dir))
{
    if (applications_dir_.empty()) {
        applications_dir_ = kDefaultAppDir;
    }
}

std::vector<AppEntry> AppCatalog::load() const
{
    std::vector<AppEntry> apps;
    std::set<std::string> seen_exec;

    if (!std::filesystem::is_directory(applications_dir_)) {
        return apps;
    }

    std::vector<std::filesystem::path> files;
    for (const auto &item : std::filesystem::directory_iterator(applications_dir_)) {
        if (item.is_regular_file() && item.path().extension() == ".desktop") {
            files.push_back(item.path());
        }
    }
    std::sort(files.begin(), files.end());

    for (const auto &file : files) {
        AppEntry entry;
        if (!parse_desktop_file(file, entry)) {
            continue;
        }

        std::string try_exec = entry.try_exec.empty() ? first_token(entry.exec) : entry.try_exec;
        if (!try_exec.empty() && !command_available(try_exec)) {
            continue;
        }

        if (!seen_exec.insert(entry.exec).second) {
            continue;
        }

        apps.push_back(std::move(entry));
    }

    return apps;
}

std::string applaunch_data_dir()
{
    const char *env = std::getenv("ZEROSHELL_APPLAUNCH_DIR");
    return env && *env ? env : kDefaultDataDir;
}

std::string applaunch_images_dir()
{
    return applaunch_data_dir() + "/share/images";
}

std::string resolve_applaunch_path(const std::string &path)
{
    if (path.empty()) {
        return {};
    }
    if (path.front() == '/') {
        return path;
    }
    if (path.rfind("A:", 0) == 0) {
        return applaunch_data_dir() + "/" + path.substr(2);
    }
    return applaunch_data_dir() + "/" + path;
}

bool command_available(const std::string &command)
{
    if (command.empty()) {
        return false;
    }

    if (command.find('/') != std::string::npos) {
        return std::filesystem::exists(command) && std::filesystem::is_regular_file(command);
    }

    std::string test = "command -v -- " + shell_quote(command) + " >/dev/null 2>&1";
    return std::system(test.c_str()) == 0;
}

} // namespace zero_shell
