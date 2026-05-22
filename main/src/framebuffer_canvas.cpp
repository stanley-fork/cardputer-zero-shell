#include "zero_shell/framebuffer_canvas.hpp"

#include <algorithm>
#include <cerrno>
#include <cstdlib>
#include <cstring>
#include <fcntl.h>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <linux/fb.h>
#include <sstream>
#include <stdexcept>
#include <sys/ioctl.h>
#include <unistd.h>

namespace zero_shell {
namespace {

struct Glyph {
    char ch;
    uint8_t rows[7];
};

constexpr Glyph kGlyphs[] = {
    {' ', {0x00,0x00,0x00,0x00,0x00,0x00,0x00}},
    {'!', {0x04,0x04,0x04,0x04,0x00,0x04,0x00}},
    {'%', {0x11,0x02,0x04,0x08,0x11,0x00,0x00}},
    {'-', {0x00,0x00,0x00,0x0E,0x00,0x00,0x00}},
    {'_', {0x00,0x00,0x00,0x00,0x00,0x00,0x1F}},
    {'.', {0x00,0x00,0x00,0x00,0x00,0x04,0x00}},
    {'/', {0x01,0x02,0x04,0x08,0x10,0x00,0x00}},
    {':', {0x00,0x04,0x00,0x00,0x04,0x00,0x00}},
    {'0', {0x0E,0x11,0x13,0x15,0x19,0x11,0x0E}},
    {'1', {0x04,0x0C,0x04,0x04,0x04,0x04,0x0E}},
    {'2', {0x0E,0x11,0x01,0x02,0x04,0x08,0x1F}},
    {'3', {0x1F,0x02,0x04,0x02,0x01,0x11,0x0E}},
    {'4', {0x02,0x06,0x0A,0x12,0x1F,0x02,0x02}},
    {'5', {0x1F,0x10,0x1E,0x01,0x01,0x11,0x0E}},
    {'6', {0x06,0x08,0x10,0x1E,0x11,0x11,0x0E}},
    {'7', {0x1F,0x01,0x02,0x04,0x08,0x08,0x08}},
    {'8', {0x0E,0x11,0x11,0x0E,0x11,0x11,0x0E}},
    {'9', {0x0E,0x11,0x11,0x0F,0x01,0x02,0x0C}},
    {'A', {0x0E,0x11,0x11,0x1F,0x11,0x11,0x11}},
    {'B', {0x1E,0x11,0x11,0x1E,0x11,0x11,0x1E}},
    {'C', {0x0E,0x11,0x10,0x10,0x10,0x11,0x0E}},
    {'D', {0x1E,0x11,0x11,0x11,0x11,0x11,0x1E}},
    {'E', {0x1F,0x10,0x10,0x1E,0x10,0x10,0x1F}},
    {'F', {0x1F,0x10,0x10,0x1E,0x10,0x10,0x10}},
    {'G', {0x0E,0x11,0x10,0x17,0x11,0x11,0x0E}},
    {'H', {0x11,0x11,0x11,0x1F,0x11,0x11,0x11}},
    {'I', {0x0E,0x04,0x04,0x04,0x04,0x04,0x0E}},
    {'J', {0x01,0x01,0x01,0x01,0x11,0x11,0x0E}},
    {'K', {0x11,0x12,0x14,0x18,0x14,0x12,0x11}},
    {'L', {0x10,0x10,0x10,0x10,0x10,0x10,0x1F}},
    {'M', {0x11,0x1B,0x15,0x15,0x11,0x11,0x11}},
    {'N', {0x11,0x19,0x15,0x13,0x11,0x11,0x11}},
    {'O', {0x0E,0x11,0x11,0x11,0x11,0x11,0x0E}},
    {'P', {0x1E,0x11,0x11,0x1E,0x10,0x10,0x10}},
    {'Q', {0x0E,0x11,0x11,0x11,0x15,0x12,0x0D}},
    {'R', {0x1E,0x11,0x11,0x1E,0x14,0x12,0x11}},
    {'S', {0x0F,0x10,0x10,0x0E,0x01,0x01,0x1E}},
    {'T', {0x1F,0x04,0x04,0x04,0x04,0x04,0x04}},
    {'U', {0x11,0x11,0x11,0x11,0x11,0x11,0x0E}},
    {'V', {0x11,0x11,0x11,0x11,0x11,0x0A,0x04}},
    {'W', {0x11,0x11,0x11,0x15,0x15,0x15,0x0A}},
    {'X', {0x11,0x11,0x0A,0x04,0x0A,0x11,0x11}},
    {'Y', {0x11,0x11,0x0A,0x04,0x04,0x04,0x04}},
    {'Z', {0x1F,0x01,0x02,0x04,0x08,0x10,0x1F}},
    {'a', {0x00,0x00,0x0E,0x01,0x0F,0x11,0x0F}},
    {'b', {0x10,0x10,0x16,0x19,0x11,0x11,0x1E}},
    {'c', {0x00,0x00,0x0E,0x10,0x10,0x11,0x0E}},
    {'d', {0x01,0x01,0x0D,0x13,0x11,0x11,0x0F}},
    {'e', {0x00,0x00,0x0E,0x11,0x1F,0x10,0x0E}},
    {'f', {0x06,0x08,0x08,0x1E,0x08,0x08,0x08}},
    {'g', {0x00,0x00,0x0F,0x11,0x0F,0x01,0x0E}},
    {'h', {0x10,0x10,0x16,0x19,0x11,0x11,0x11}},
    {'i', {0x04,0x00,0x0C,0x04,0x04,0x04,0x0E}},
    {'j', {0x02,0x00,0x06,0x02,0x02,0x12,0x0C}},
    {'k', {0x10,0x10,0x12,0x14,0x18,0x14,0x12}},
    {'l', {0x0C,0x04,0x04,0x04,0x04,0x04,0x0E}},
    {'m', {0x00,0x00,0x1A,0x15,0x15,0x15,0x15}},
    {'n', {0x00,0x00,0x16,0x19,0x11,0x11,0x11}},
    {'o', {0x00,0x00,0x0E,0x11,0x11,0x11,0x0E}},
    {'p', {0x00,0x00,0x1E,0x11,0x1E,0x10,0x10}},
    {'q', {0x00,0x00,0x0D,0x13,0x0F,0x01,0x01}},
    {'r', {0x00,0x00,0x16,0x19,0x10,0x10,0x10}},
    {'s', {0x00,0x00,0x0F,0x10,0x0E,0x01,0x1E}},
    {'t', {0x08,0x08,0x1E,0x08,0x08,0x09,0x06}},
    {'u', {0x00,0x00,0x11,0x11,0x11,0x13,0x0D}},
    {'v', {0x00,0x00,0x11,0x11,0x11,0x0A,0x04}},
    {'w', {0x00,0x00,0x11,0x11,0x15,0x15,0x0A}},
    {'x', {0x00,0x00,0x11,0x0A,0x04,0x0A,0x11}},
    {'y', {0x00,0x00,0x11,0x11,0x0F,0x01,0x0E}},
    {'z', {0x00,0x00,0x1F,0x02,0x04,0x08,0x1F}},
};

constexpr uint8_t kQuestionRows[7] = {0x0E,0x11,0x01,0x02,0x04,0x00,0x04};

const uint8_t *glyph_rows(char ch)
{
    for (const auto &glyph : kGlyphs) {
        if (glyph.ch == ch) {
            return glyph.rows;
        }
    }
    return kQuestionRows;
}

std::string read_fb_name(int index)
{
    std::ifstream file("/sys/class/graphics/fb" + std::to_string(index) + "/name");
    std::string name;
    std::getline(file, name);
    return name;
}

} // namespace

FramebufferCanvas::FramebufferCanvas() = default;

FramebufferCanvas::~FramebufferCanvas()
{
    close();
}

bool FramebufferCanvas::open(const std::string &device)
{
    close();

    fd_ = ::open(device.c_str(), O_RDWR | O_CLOEXEC);
    if (fd_ < 0) {
        std::cerr << "zero-shell: cannot open framebuffer " << device << ": "
                  << std::strerror(errno) << "\n";
        return false;
    }

    fb_var_screeninfo var{};
    fb_fix_screeninfo fix{};
    if (ioctl(fd_, FBIOGET_VSCREENINFO, &var) < 0 ||
        ioctl(fd_, FBIOGET_FSCREENINFO, &fix) < 0) {
        std::cerr << "zero-shell: cannot inspect framebuffer " << device << ": "
                  << std::strerror(errno) << "\n";
        close();
        return false;
    }

    width_ = static_cast<int>(var.xres);
    height_ = static_cast<int>(var.yres);
    stride_ = static_cast<int>(fix.line_length);
    bits_per_pixel_ = static_cast<int>(var.bits_per_pixel);

    if (width_ <= 0 || height_ <= 0 || stride_ <= 0 ||
        (bits_per_pixel_ != 16 && bits_per_pixel_ != 24 && bits_per_pixel_ != 32)) {
        std::cerr << "zero-shell: unsupported framebuffer format "
                  << width_ << "x" << height_ << "x" << bits_per_pixel_ << "\n";
        close();
        return false;
    }

    buffer_.assign(static_cast<size_t>(stride_) * static_cast<size_t>(height_), 0);
    return true;
}

void FramebufferCanvas::close()
{
    if (fd_ >= 0) {
        ::close(fd_);
    }
    fd_ = -1;
    width_ = 0;
    height_ = 0;
    stride_ = 0;
    bits_per_pixel_ = 0;
    buffer_.clear();
}

void FramebufferCanvas::clear(Color color)
{
    fill_rect(0, 0, width_, height_, color);
}

void FramebufferCanvas::fill_rect(int x, int y, int w, int h, Color color)
{
    int x0 = std::clamp(x, 0, width_);
    int y0 = std::clamp(y, 0, height_);
    int x1 = std::clamp(x + w, 0, width_);
    int y1 = std::clamp(y + h, 0, height_);

    for (int py = y0; py < y1; ++py) {
        for (int px = x0; px < x1; ++px) {
            put_pixel(px, py, color);
        }
    }
}

void FramebufferCanvas::draw_rect(int x, int y, int w, int h, Color color)
{
    fill_rect(x, y, w, 1, color);
    fill_rect(x, y + h - 1, w, 1, color);
    fill_rect(x, y, 1, h, color);
    fill_rect(x + w - 1, y, 1, h, color);
}

void FramebufferCanvas::draw_text(int x, int y, const std::string &text, Color color, int scale)
{
    int cursor = x;
    for (char ch : text) {
        put_char(cursor, y, ch, color, std::max(1, scale));
        cursor += 6 * std::max(1, scale);
    }
}

void FramebufferCanvas::draw_text_centered(int cx, int y, const std::string &text, Color color, int scale)
{
    int text_width = static_cast<int>(text.size()) * 6 * std::max(1, scale);
    draw_text(cx - text_width / 2, y, text, color, scale);
}

void FramebufferCanvas::draw_icon_tile(int cx, int cy, int size, Color fill, Color border, const std::string &label)
{
    int x = cx - size / 2;
    int y = cy - size / 2;
    fill_rect(x, y, size, size, fill);
    draw_rect(x, y, size, size, border);
    draw_rect(x + 2, y + 2, size - 4, size - 4, {30, 36, 46});

    std::string initials;
    for (char ch : label) {
        if (std::isalnum(static_cast<unsigned char>(ch))) {
            initials.push_back(static_cast<char>(std::toupper(static_cast<unsigned char>(ch))));
        }
        if (initials.size() == 2) {
            break;
        }
    }
    if (initials.empty()) {
        initials = "?";
    }
    draw_text_centered(cx, cy - 7, initials, {245, 245, 245}, size >= 58 ? 2 : 1);
}

void FramebufferCanvas::present()
{
    if (fd_ < 0 || buffer_.empty()) {
        return;
    }

    size_t total = buffer_.size();
    size_t written = 0;
    if (lseek(fd_, 0, SEEK_SET) < 0) {
        return;
    }

    while (written < total) {
        ssize_t n = ::write(fd_, buffer_.data() + written, total - written);
        if (n <= 0) {
            break;
        }
        written += static_cast<size_t>(n);
    }
}

void FramebufferCanvas::put_pixel(int x, int y, Color color)
{
    if (x < 0 || y < 0 || x >= width_ || y >= height_ || buffer_.empty()) {
        return;
    }

    uint8_t *ptr = buffer_.data() + static_cast<size_t>(y) * stride_ +
                   static_cast<size_t>(x) * (bits_per_pixel_ / 8);

    if (bits_per_pixel_ == 16) {
        uint16_t value = static_cast<uint16_t>(((color.r >> 3) << 11) |
                                               ((color.g >> 5) << 5) |
                                               (color.b >> 3));
        ptr[0] = static_cast<uint8_t>(value & 0xFF);
        ptr[1] = static_cast<uint8_t>((value >> 8) & 0xFF);
    } else if (bits_per_pixel_ == 24) {
        ptr[0] = color.b;
        ptr[1] = color.g;
        ptr[2] = color.r;
    } else if (bits_per_pixel_ == 32) {
        ptr[0] = color.b;
        ptr[1] = color.g;
        ptr[2] = color.r;
        ptr[3] = 0xFF;
    }
}

void FramebufferCanvas::put_char(int x, int y, char ch, Color color, int scale)
{
    const uint8_t *rows = nullptr;
    if (ch == '?') {
        rows = kQuestionRows;
    } else {
        rows = glyph_rows(ch);
    }

    for (int row = 0; row < 7; ++row) {
        for (int col = 0; col < 5; ++col) {
            if (rows[row] & (1 << (4 - col))) {
                fill_rect(x + col * scale, y + row * scale, scale, scale, color);
            }
        }
    }
}

uint32_t FramebufferCanvas::pack_color(Color color) const
{
    return (static_cast<uint32_t>(color.r) << 16) |
           (static_cast<uint32_t>(color.g) << 8) |
           static_cast<uint32_t>(color.b);
}

std::string find_internal_framebuffer()
{
    const char *env = std::getenv("ZEROSHELL_FBDEV");
    if (env && *env) {
        return env;
    }

    std::ifstream proc("/proc/fb");
    std::string line;
    while (std::getline(proc, line)) {
        std::istringstream stream(line);
        int index = -1;
        std::string name;
        stream >> index >> name;
        if (index >= 0 && name.find("st7789") != std::string::npos) {
            return "/dev/fb" + std::to_string(index);
        }
    }

    for (int index = 0; index < 8; ++index) {
        std::string name = read_fb_name(index);
        if (name.find("st7789") != std::string::npos) {
            return "/dev/fb" + std::to_string(index);
        }
    }

    return "/dev/fb0";
}

} // namespace zero_shell
