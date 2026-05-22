#include "zero_shell/pty_terminal.hpp"

#include <algorithm>
#include <cerrno>
#include <cstring>
#include <fcntl.h>
#include <iostream>
#include <pty.h>
#include <signal.h>
#include <sys/ioctl.h>
#include <sys/wait.h>
#include <termios.h>
#include <unistd.h>
#include <vector>

namespace zero_shell {
namespace {

constexpr Color kBg{0, 0, 0};
constexpr Color kFg{80, 255, 120};
constexpr Color kDim{68, 88, 74};
constexpr int kCharW = 6;
constexpr int kCharH = 8;

class TerminalBuffer {
public:
    TerminalBuffer(int cols, int rows)
        : cols_(std::max(1, cols)),
          rows_(std::max(1, rows)),
          cells_(static_cast<size_t>(cols_) * static_cast<size_t>(rows_), ' ')
    {
    }

    void put(char ch)
    {
        if (ch == '\r') {
            col_ = 0;
            return;
        }
        if (ch == '\n') {
            newline();
            return;
        }
        if (ch == '\b' || ch == 0x7f) {
            if (col_ > 0) {
                --col_;
                set(row_, col_, ' ');
            }
            return;
        }
        if (ch == '\t') {
            int spaces = 4 - (col_ % 4);
            while (spaces-- > 0) {
                put(' ');
            }
            return;
        }
        if (static_cast<unsigned char>(ch) < 0x20) {
            return;
        }

        set(row_, col_, ch);
        ++col_;
        if (col_ >= cols_) {
            newline();
        }
    }

    void clear()
    {
        std::fill(cells_.begin(), cells_.end(), ' ');
        row_ = 0;
        col_ = 0;
    }

    std::string line(int row) const
    {
        if (row < 0 || row >= rows_) {
            return {};
        }
        std::string out(cells_.begin() + row * cols_, cells_.begin() + (row + 1) * cols_);
        while (!out.empty() && out.back() == ' ') {
            out.pop_back();
        }
        return out;
    }

    int rows() const { return rows_; }

private:
    void set(int row, int col, char ch)
    {
        if (row < 0 || col < 0 || row >= rows_ || col >= cols_) {
            return;
        }
        cells_[static_cast<size_t>(row) * cols_ + col] = ch;
    }

    void newline()
    {
        col_ = 0;
        ++row_;
        if (row_ < rows_) {
            return;
        }
        for (int r = 1; r < rows_; ++r) {
            for (int c = 0; c < cols_; ++c) {
                cells_[static_cast<size_t>(r - 1) * cols_ + c] =
                    cells_[static_cast<size_t>(r) * cols_ + c];
            }
        }
        std::fill(cells_.begin() + (rows_ - 1) * cols_, cells_.end(), ' ');
        row_ = rows_ - 1;
    }

    int cols_ = 0;
    int rows_ = 0;
    int row_ = 0;
    int col_ = 0;
    std::vector<char> cells_;
};

void render_terminal(FramebufferCanvas &canvas, const TerminalBuffer &buffer, const std::string &title)
{
    canvas.clear(kBg);
    canvas.fill_rect(0, 0, canvas.width(), 16, {8, 14, 10});
    canvas.draw_text(4, 4, title.empty() ? "Terminal" : title, kFg, 1);
    canvas.draw_text(canvas.width() - 58, 4, "ESC back", kDim, 1);

    for (int row = 0; row < buffer.rows(); ++row) {
        canvas.draw_text(2, 20 + row * kCharH, buffer.line(row), kFg, 1);
    }
    canvas.present();
}

std::string key_to_terminal_sequence(const KeyEvent &event)
{
    if (!event.text.empty()) {
        return event.text;
    }

    switch (event.key) {
    case Key::Enter:
        return "\r";
    case Key::Backspace:
        return "\x7f";
    case Key::Tab:
        return "\t";
    case Key::Left:
        return "\x1b[D";
    case Key::Right:
        return "\x1b[C";
    case Key::Up:
        return "\x1b[A";
    case Key::Down:
        return "\x1b[B";
    default:
        return {};
    }
}

} // namespace

int run_terminal_page(FramebufferCanvas &canvas,
                      InputDevice &input,
                      const std::string &command,
                      bool sysplause)
{
    const int cols = std::max(20, canvas.width() / kCharW);
    const int rows = std::max(8, (canvas.height() - 22) / kCharH);
    TerminalBuffer buffer(cols, rows);

    int master = -1;
    winsize ws{};
    ws.ws_col = static_cast<unsigned short>(cols);
    ws.ws_row = static_cast<unsigned short>(rows);
    ws.ws_xpixel = static_cast<unsigned short>(canvas.width());
    ws.ws_ypixel = static_cast<unsigned short>(canvas.height());

    pid_t pid = forkpty(&master, nullptr, nullptr, &ws);
    if (pid < 0) {
        buffer.put('E'); buffer.put('r'); buffer.put('r'); buffer.put('o'); buffer.put('r');
        render_terminal(canvas, buffer, "Terminal");
        return -1;
    }

    if (pid == 0) {
        setenv("TERM", "vt100", 1);
        execlp("/bin/sh", "sh", "-lc", command.c_str(), static_cast<char *>(nullptr));
        _exit(127);
    }

    fcntl(master, F_SETFL, fcntl(master, F_GETFL, 0) | O_NONBLOCK);
    bool active = true;
    int exit_code = -1;
    int status = 0;

    render_terminal(canvas, buffer, command);

    while (active) {
        char data[512];
        bool changed = false;
        while (true) {
            ssize_t n = read(master, data, sizeof(data));
            if (n > 0) {
                for (ssize_t i = 0; i < n; ++i) {
                    buffer.put(data[i]);
                }
                changed = true;
            } else {
                break;
            }
        }

        pid_t rc = waitpid(pid, &status, WNOHANG);
        if (rc == pid) {
            if (WIFEXITED(status)) {
                exit_code = WEXITSTATUS(status);
            } else if (WIFSIGNALED(status)) {
                exit_code = 128 + WTERMSIG(status);
            }
            active = false;
            changed = true;
        }

        KeyEvent event = input.poll_event(std::chrono::milliseconds(30));
        if (event.key == Key::Escape) {
            kill(pid, SIGTERM);
            waitpid(pid, &status, 0);
            exit_code = -1;
            active = false;
            changed = true;
        } else {
            std::string seq = key_to_terminal_sequence(event);
            if (!seq.empty()) {
                write(master, seq.data(), seq.size());
            }
        }

        if (changed) {
            render_terminal(canvas, buffer, command);
        }
    }

    if (sysplause) {
        std::string msg = "\n[ZeroShell] exit " + std::to_string(exit_code) + ". Enter/ESC back.";
        for (char ch : msg) {
            buffer.put(ch);
        }
        render_terminal(canvas, buffer, command);
        while (true) {
            KeyEvent event = input.poll_event(std::chrono::milliseconds(100));
            if (event.key == Key::Enter || event.key == Key::Escape) {
                break;
            }
        }
    }

    close(master);
    return exit_code;
}

} // namespace zero_shell

