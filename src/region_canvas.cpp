#include "region_canvas.hpp"

#include <algorithm>
#include <array>
#include <cstring>

RegionCanvas::RegionCanvas(FrameView frame, Rectangle bounds,
                           DisplayRotation rotation) noexcept
    : frame_(frame), bounds_(bounds), rotation_(rotation)
{
}

void RegionCanvas::clear(std::uint32_t color) noexcept
{
    fillRectangle({0, 0, width(), height()}, color);
}

void RegionCanvas::fillRectangle(Rectangle rectangle, std::uint32_t color) noexcept
{
    if (rectangle.x >= width() || rectangle.y >= height()) {
        return;
    }

    rectangle.width = std::min(rectangle.width, width() - rectangle.x);
    rectangle.height = std::min(rectangle.height, height() - rectangle.y);
    rectangle.x += bounds_.x;
    rectangle.y += bounds_.y;

    if (rotation_ == DisplayRotation::Clockwise90) {
        rectangle = {rectangle.y, frame_.height - rectangle.x - rectangle.width,
                     rectangle.height, rectangle.width};
    }

    const std::array<std::uint8_t, 4> pixel{
        static_cast<std::uint8_t>(color),
        static_cast<std::uint8_t>(color >> 8),
        static_cast<std::uint8_t>(color >> 16),
        0xff
    };

    for (std::uint32_t y = 0; y < rectangle.height; ++y) {
        auto* row = frame_.pixels +
            std::size_t{rectangle.y + y} * frame_.pitch +
            std::size_t{rectangle.x} * pixel.size();

        for (std::uint32_t x = 0; x < rectangle.width; ++x) {
            std::memcpy(row + std::size_t{x} * pixel.size(),
                        pixel.data(), pixel.size());
        }
    }
}
