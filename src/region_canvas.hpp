#pragma once

#include "frame_view.hpp"

struct Rectangle {
    std::uint32_t x;
    std::uint32_t y;
    std::uint32_t width;
    std::uint32_t height;
};

class RegionCanvas {
public:
    RegionCanvas(const RegionCanvas&) = delete;
    RegionCanvas& operator=(const RegionCanvas&) = delete;

    std::uint32_t width() const noexcept { return bounds_.width; }
    std::uint32_t height() const noexcept { return bounds_.height; }

    void clear(std::uint32_t color) noexcept;
    // Fixed five-by-seven glyphs; lowercase is displayed as uppercase.
    void text(int x, int y, const char* value, std::uint32_t color,
              unsigned scale = 1) noexcept;
    void pixel(int x, int y, std::uint32_t color) noexcept;
    void fillRectangle(Rectangle rectangle, std::uint32_t color) noexcept;

private:
    friend class Renderer;

    RegionCanvas(FrameView frame, Rectangle bounds) noexcept;

    FrameView frame_;
    Rectangle bounds_;
};
