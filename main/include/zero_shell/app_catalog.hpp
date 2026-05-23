#pragma once

#include <filesystem>
#include <string>
#include <vector>

namespace zero_shell {

struct AppEntry {
    std::string id;
    std::string name;
    std::string exec;
    std::string icon;
    std::string try_exec;
    std::string short_name;
    std::string startup_wm_class;
    std::string zero_app_id;
    std::string zero_display;
};

class AppCatalog {
public:
    explicit AppCatalog(std::filesystem::path applications_dir);

    std::vector<AppEntry> load() const;
    const std::filesystem::path &applications_dir() const { return applications_dir_; }

private:
    std::filesystem::path applications_dir_;
};

std::string applaunch_data_dir();
std::string applaunch_images_dir();
std::string resolve_applaunch_path(const std::string &path);
bool command_available(const std::string &command);

} // namespace zero_shell
