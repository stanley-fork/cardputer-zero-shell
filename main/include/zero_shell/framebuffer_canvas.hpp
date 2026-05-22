#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace zero_shell {

struct Color {
    uint8_t r = 0;
    uint8_t g = 0;
    uint8_t b = 0;
};

class FramebufferCanvas {
public:
    FramebufferCanvas();
    ~FramebufferCanvas();

    FramebufferCanvas(const FramebufferCanvas &) = delete;
    FramebufferCanvas &operator=(const FramebufferCanvas &) = delete;

    bool open(const std::string &device);
    void close();

    int width() const { return width_; }
    int height() const { return height_; }
    int bits_per_pixel() const { return bits_per_pixel_; }
    bool is_open() const { return fd_ >= 0 && !buffer_.empty(); }

    void clear(Color color);
    void fill_rect(int x, int y, int w, int h, Color color);
    void draw_rect(int x, int y, int w, int h, Color color);
    void draw_text(int x, int y, const std::string &text, Color color, int scale = 1);
    void draw_text_centered(int cx, int y, const std::string &text, Color color, int scale = 1);
    void draw_icon_tile(int cx, int cy, int size, Color fill, Color border, const std::string &label);
    void present();

private:
    void put_pixel(int x, int y, Color color);
    void put_char(int x, int y, char ch, Color color, int scale);
    uint32_t pack_color(Color color) const;

    int fd_ = -1;
    int width_ = 0;
    int height_ = 0;
    int stride_ = 0;
    int bits_per_pixel_ = 0;
    std::vector<uint8_t> buffer_;
};

std::string find_internal_framebuffer();

} // namespace zero_shell

