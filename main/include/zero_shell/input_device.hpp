#pragma once

#include <chrono>
#include <cstdint>
#include <string>

namespace zero_shell {

enum class Key {
    None,
    Left,
    Right,
    Up,
    Down,
    Enter,
    Escape,
    Backspace,
    Tab,
    R,
    Q,
    Power,
};

struct KeyEvent {
    Key key = Key::None;
    std::string text;
    uint16_t code = 0;
};

class InputDevice {
public:
    InputDevice();
    ~InputDevice();

    InputDevice(const InputDevice &) = delete;
    InputDevice &operator=(const InputDevice &) = delete;

    bool open(const std::string &device);
    void close();
    bool is_open() const { return fd_ >= 0; }

    Key poll(std::chrono::milliseconds timeout);
    KeyEvent poll_event(std::chrono::milliseconds timeout);

private:
    int fd_ = -1;
    bool shift_down_ = false;
    bool caps_lock_ = false;
};

std::string find_keyboard_device();

} // namespace zero_shell
