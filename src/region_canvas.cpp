#include "region_canvas.hpp"

#include <algorithm>
#include <array>
#include <cstring>

RegionCanvas::RegionCanvas(FrameView frame, Rectangle bounds) noexcept
    : frame_(frame), bounds_(bounds)
{
}

void RegionCanvas::pixel(int x, int y, std::uint32_t color) noexcept
{
    if (x < 0 || y < 0 || static_cast<unsigned>(x) >= width() ||
        static_cast<unsigned>(y) >= height()) return;
    fillRectangle({static_cast<unsigned>(x), static_cast<unsigned>(y), 1, 1}, color);
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

    // Map portrait coordinates into the framebuffer with the fixed 90-degree transform.
    rectangle = {rectangle.y, frame_.height - rectangle.x - rectangle.width,
                 rectangle.height, rectangle.width};

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

namespace {
std::array<unsigned char,7> glyph(char c) noexcept
{
    if (c>='a' && c<='z') c-=32;
    switch(c) {
    case 'A': return {14,17,17,31,17,17,17};
    case 'B': return {30,17,17,30,17,17,30};
    case 'C': return {14,17,16,16,16,17,14};
    case 'D': return {30,17,17,17,17,17,30};
    case 'E': return {31,16,16,30,16,16,31};
    case 'F': return {31,16,16,30,16,16,16};
    case 'G': return {14,17,16,23,17,17,15};
    case 'H': return {17,17,17,31,17,17,17};
    case 'I': return {14,4,4,4,4,4,14};
    case 'J': return {7,2,2,2,2,18,12};
    case 'K': return {17,18,20,24,20,18,17};
    case 'L': return {16,16,16,16,16,16,31};
    case 'M': return {17,27,21,21,17,17,17};
    case 'N': return {17,25,21,19,17,17,17};
    case 'O': return {14,17,17,17,17,17,14};
    case 'P': return {30,17,17,30,16,16,16};
    case 'Q': return {14,17,17,17,21,18,13};
    case 'R': return {30,17,17,30,20,18,17};
    case 'S': return {15,16,16,14,1,1,30};
    case 'T': return {31,4,4,4,4,4,4};
    case 'U': return {17,17,17,17,17,17,14};
    case 'V': return {17,17,17,17,17,10,4};
    case 'W': return {17,17,17,21,21,21,10};
    case 'X': return {17,17,10,4,10,17,17};
    case 'Y': return {17,17,10,4,4,4,4};
    case 'Z': return {31,1,2,4,8,16,31};
    case '0': return {14,17,19,21,25,17,14};
    case '1': return {4,12,4,4,4,4,14};
    case '2': return {14,17,1,2,4,8,31};
    case '3': return {30,1,1,14,1,1,30};
    case '4': return {2,6,10,18,31,2,2};
    case '5': return {31,16,16,30,1,1,30};
    case '6': return {14,16,16,30,17,17,14};
    case '7': return {31,1,2,4,8,8,8};
    case '8': return {14,17,17,14,17,17,14};
    case '9': return {14,17,17,15,1,1,14};
    case '.': return {0,0,0,0,0,12,12};
    case ':': return {0,12,12,0,12,12,0};
    case '/': return {1,2,2,4,8,8,16};
    case '%': return {25,25,2,4,8,19,19};
    case '-': return {0,0,0,31,0,0,0};
    case '+': return {0,4,4,31,4,4,0};
    case '=': return {0,31,0,31,0,0,0};
    case '[': return {14,8,8,8,8,8,14};
    case ']': return {14,2,2,2,2,2,14};
    default: return {};
    }
}
}

void RegionCanvas::text(int x, int y, const char* value, std::uint32_t color,
                        unsigned scale) noexcept
{
    if (!value || !scale || scale>8) return;
    for (;*value && x<int(width());++value,x+=6*int(scale)) {
        const auto rows=glyph(*value);
        for (int row=0;row<7;++row)
            for (int col=0;col<5;++col)
                if (rows[row] & (1u<<(4-col)))
                    for (unsigned dy=0;dy<scale;++dy)
                        for (unsigned dx=0;dx<scale;++dx)
                            pixel(x+col*int(scale)+int(dx),y+row*int(scale)+int(dy),color);
    }
}
