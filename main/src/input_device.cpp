#include "zero_shell/input_device.hpp"

#include <cerrno>
#include <cctype>
#include <cstdlib>
#include <cstring>
#include <fcntl.h>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <linux/input.h>
#include <poll.h>
#include <sstream>
#include <unistd.h>

namespace zero_shell {
namespace {

Key map_key_code(uint16_t code)
{
    switch (code) {
    case KEY_LEFT:
        return Key::Left;
    case KEY_RIGHT:
        return Key::Right;
    case KEY_UP:
        return Key::Up;
    case KEY_DOWN:
        return Key::Down;
    case KEY_ENTER:
        return Key::Enter;
    case KEY_ESC:
        return Key::Escape;
    case KEY_BACKSPACE:
    case KEY_DELETE:
        return Key::Backspace;
    case KEY_TAB:
        return Key::Tab;
    case KEY_R:
        return Key::R;
    case KEY_Q:
        return Key::Q;
    case KEY_POWER:
        return Key::Power;
    default:
        return Key::None;
    }
}

bool is_shift(uint16_t code)
{
    return code == KEY_LEFTSHIFT || code == KEY_RIGHTSHIFT;
}

char letter_from_code(uint16_t code)
{
    if (code >= KEY_A && code <= KEY_Z) {
        return static_cast<char>('a' + (code - KEY_A));
    }
    return '\0';
}

std::string text_from_code(uint16_t code, bool shifted, bool caps_lock)
{
    char letter = letter_from_code(code);
    if (letter) {
        bool upper = shifted ^ caps_lock;
        if (upper) {
            letter = static_cast<char>(std::toupper(static_cast<unsigned char>(letter)));
        }
        return std::string(1, letter);
    }

    switch (code) {
    case KEY_SPACE:
        return " ";
    case KEY_1:
        return shifted ? "!" : "1";
    case KEY_2:
        return shifted ? "@" : "2";
    case KEY_3:
        return shifted ? "#" : "3";
    case KEY_4:
        return shifted ? "$" : "4";
    case KEY_5:
        return shifted ? "%" : "5";
    case KEY_6:
        return shifted ? "^" : "6";
    case KEY_7:
        return shifted ? "&" : "7";
    case KEY_8:
        return shifted ? "*" : "8";
    case KEY_9:
        return shifted ? "(" : "9";
    case KEY_0:
        return shifted ? ")" : "0";
    case KEY_MINUS:
        return shifted ? "_" : "-";
    case KEY_EQUAL:
        return shifted ? "+" : "=";
    case KEY_LEFTBRACE:
        return shifted ? "{" : "[";
    case KEY_RIGHTBRACE:
        return shifted ? "}" : "]";
    case KEY_BACKSLASH:
        return shifted ? "|" : "\\";
    case KEY_SEMICOLON:
        return shifted ? ":" : ";";
    case KEY_APOSTROPHE:
        return shifted ? "\"" : "'";
    case KEY_GRAVE:
        return shifted ? "~" : "`";
    case KEY_COMMA:
        return shifted ? "<" : ",";
    case KEY_DOT:
        return shifted ? ">" : ".";
    case KEY_SLASH:
        return shifted ? "?" : "/";
    default:
        return {};
    }
}

std::string read_file(const std::filesystem::path &path)
{
    std::ifstream file(path);
    std::string text;
    std::getline(file, text);
    return text;
}

} // namespace

InputDevice::InputDevice() = default;

InputDevice::~InputDevice()
{
    close();
}

bool InputDevice::open(const std::string &device)
{
    close();

    fd_ = ::open(device.c_str(), O_RDONLY | O_NONBLOCK | O_CLOEXEC);
    if (fd_ < 0) {
        std::cerr << "zero-shell: cannot open input device " << device << ": "
                  << std::strerror(errno) << "\n";
        return false;
    }
    return true;
}

void InputDevice::close()
{
    if (fd_ >= 0) {
        ::close(fd_);
    }
    fd_ = -1;
}

Key InputDevice::poll(std::chrono::milliseconds timeout)
{
    return poll_event(timeout).key;
}

KeyEvent InputDevice::poll_event(std::chrono::milliseconds timeout)
{
    if (fd_ < 0) {
        ::usleep(static_cast<useconds_t>(timeout.count() * 1000));
        return {};
    }

    pollfd pfd{};
    pfd.fd = fd_;
    pfd.events = POLLIN;

    int rc = ::poll(&pfd, 1, static_cast<int>(timeout.count()));
    if (rc <= 0 || !(pfd.revents & POLLIN)) {
        return {};
    }

    input_event event{};
    while (::read(fd_, &event, sizeof(event)) == static_cast<ssize_t>(sizeof(event))) {
        if (event.type != EV_KEY) {
            continue;
        }

        if (is_shift(event.code)) {
            shift_down_ = event.value != 0;
            continue;
        }

        if (event.code == KEY_CAPSLOCK && event.value == 1) {
            caps_lock_ = !caps_lock_;
            continue;
        }

        if (event.value != 1 && event.value != 2) {
            continue;
        }

        Key key = map_key_code(event.code);
        std::string text = text_from_code(event.code, shift_down_, caps_lock_);
        if (key != Key::None || !text.empty()) {
            return KeyEvent{key, text, event.code};
        }
    }

    return {};
}

std::string find_keyboard_device()
{
    const char *env = std::getenv("ZEROSHELL_KEYBOARD_DEVICE");
    if (env && *env) {
        return env;
    }

    const std::filesystem::path by_path = "/dev/input/by-path";
    if (std::filesystem::is_directory(by_path)) {
        for (const auto &entry : std::filesystem::directory_iterator(by_path)) {
            std::string path = entry.path().string();
            if (path.find("3f804000.i2c") != std::string::npos &&
                path.find("event") != std::string::npos) {
                return path;
            }
        }
    }

    const std::filesystem::path input_root = "/sys/class/input";
    if (std::filesystem::is_directory(input_root)) {
        for (const auto &entry : std::filesystem::directory_iterator(input_root)) {
            std::string name = entry.path().filename().string();
            if (name.rfind("event", 0) != 0) {
                continue;
            }

            std::string dev_name = read_file(entry.path() / "device/name");
            std::string lower = dev_name;
            for (char &ch : lower) {
                ch = static_cast<char>(std::tolower(static_cast<unsigned char>(ch)));
            }

            if (lower.find("keyboard") != std::string::npos ||
                lower.find("keypad") != std::string::npos ||
                lower.find("tca8418") != std::string::npos) {
                return "/dev/input/" + name;
            }
        }
    }

    return "/dev/input/event0";
}

} // namespace zero_shell
