#include "region_canvas.hpp"

#include <algorithm>
#include <array>
#include <cmath>
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

void RegionCanvas::image(int x, int y, std::uint32_t imageWidth, std::uint32_t imageHeight,
                         std::span<const std::uint32_t> pixels, std::uint8_t opacity) noexcept
{
    if (!opacity || !imageWidth || !imageHeight ||
        std::uint64_t(imageWidth) * imageHeight > pixels.size()) return;
    const auto left = std::max<std::int64_t>(0, x);
    const auto top = std::max<std::int64_t>(0, y);
    const auto right = std::min<std::int64_t>(width(), std::int64_t(x) + imageWidth);
    const auto bottom = std::min<std::int64_t>(height(), std::int64_t(y) + imageHeight);
    if (left >= right || top >= bottom) return;
    for (auto column = left; column < right; ++column) {
        auto* destination = frame_.pixels +
            std::size_t(frame_.height - bounds_.x - column - 1) * frame_.pitch +
            std::size_t(bounds_.y + top) * 4;
        for (auto row = top; row < bottom; ++row, destination += 4) {
            const auto color = pixels[std::size_t(row - y) * imageWidth + std::size_t(column - x)];
            if (opacity == 255) {
                destination[0] = static_cast<std::uint8_t>(color);
                destination[1] = static_cast<std::uint8_t>(color >> 8);
                destination[2] = static_cast<std::uint8_t>(color >> 16);
            } else {
                for (unsigned channel = 0; channel < 3; ++channel) {
                    const auto value = (color >> (channel * 8)) & 255;
                    destination[channel] = static_cast<std::uint8_t>(
                        (value * opacity + destination[channel] * (255 - opacity) + 127) / 255);
                }
            }
            destination[3] = 255;
        }
    }
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

std::array<unsigned char,7> labelGlyph(char c) noexcept
{
    switch(c) {
    case 'a': return {0,0,14,1,15,17,15};
    case 'b': return {16,16,30,17,17,17,30};
    case 'c': return {0,0,14,17,16,17,14};
    case 'd': return {1,1,15,17,17,17,15};
    case 'e': return {0,0,14,17,31,16,14};
    case 'f': return {6,8,8,28,8,8,8};
    case 'g': return {0,15,17,17,15,1,14};
    case 'h': return {16,16,30,17,17,17,17};
    case 'i': return {4,0,12,4,4,4,14};
    case 'j': return {2,0,6,2,2,18,12};
    case 'k': return {16,16,18,20,24,20,18};
    case 'l': return {12,4,4,4,4,4,14};
    case 'm': return {0,0,26,21,21,17,17};
    case 'n': return {0,0,30,17,17,17,17};
    case 'o': return {0,0,14,17,17,17,14};
    case 'p': return {0,30,17,17,30,16,16};
    case 'q': return {0,15,17,17,15,1,1};
    case 'r': return {0,0,22,25,16,16,16};
    case 's': return {0,0,15,16,14,1,30};
    case 't': return {8,8,28,8,8,9,6};
    case 'u': return {0,0,17,17,17,19,13};
    case 'v': return {0,0,17,17,17,10,4};
    case 'w': return {0,0,17,17,21,21,10};
    case 'x': return {0,0,17,10,4,10,17};
    case 'y': return {0,17,17,17,15,1,14};
    case 'z': return {0,0,31,2,4,8,31};
    default: return glyph(c);
    }
}

struct LabelFont {
    int advance;
    int height;
    std::array<std::array<unsigned char,18*24>,95> coverage{};
};

const LabelFont& font(RegionCanvas::TextStyle style) noexcept
{
    static const auto fonts=[] {
        std::array<LabelFont,4> result{{{7,12,{}},{8,14,{}},{18,24,{}},{6,10,{}}}};
        constexpr std::array<double,4> scales{1.2,1.35,3.,1.};
        for (std::size_t i=0;i<result.size();++i) {
            auto& selected=result[i];
            const double scale=scales[i];
            for (int c=32;c<127;++c) {
                const auto rows=labelGlyph(static_cast<char>(c));
                auto& coverage=selected.coverage[c-32];
                for (int y=0;y<selected.height;++y)
                    for (int x=0;x<selected.advance;++x) {
                        unsigned hits=0;
                        for (int sy=0;sy<4;++sy)
                            for (int sx=0;sx<4;++sx) {
                                const int col=int(std::floor((x+(sx+.5)/4)/scale));
                                const int row=int(std::floor((y-2+(sy+.5)/4)/scale));
                                if (col>=0 && col<5 && row>=0 && row<7 &&
                                    (rows[row] & (1u<<(4-col)))) ++hits;
                            }
                        coverage[y*selected.advance+x]=static_cast<unsigned char>((hits*255+8)/16);
                    }
            }
        }
        return result;
    }();
    const auto index=static_cast<unsigned>(style);
    return fonts[index<fonts.size() ? index : 0];
}

}

void RegionCanvas::blendPixel(int x, int y, std::uint32_t color, std::uint8_t opacity) noexcept
{
    if (!opacity || x < 0 || y < 0 || static_cast<unsigned>(x) >= width() ||
        static_cast<unsigned>(y) >= height()) return;
    const auto nativeX = bounds_.y+static_cast<unsigned>(y);
    const auto nativeY = frame_.height-1-bounds_.x-static_cast<unsigned>(x);
    auto* pixel = frame_.pixels+std::size_t(nativeY)*frame_.pitch+nativeX*4;
    for (unsigned channel = 0; channel < 3; ++channel) {
        const unsigned source = (color >> (channel*8)) & 0xff;
        pixel[channel] = static_cast<std::uint8_t>(
            (source*opacity+pixel[channel]*(255-opacity)+127)/255);
    }
    pixel[3] = 0xff;
}

int RegionCanvas::textWidth(const char* value, TextStyle style) noexcept
{
    if (!value) return 0;
    const auto& selected = font(style);
    int width = 0;
    for (; *value; ++value) {
        width += selected.advance;
    }
    return width;
}

void RegionCanvas::label(int x, int y, const char* value, std::uint32_t color,
                         TextStyle style) noexcept
{
    if (!value) return;
    const auto& selected = font(style);
    for (; *value && x < int(width()); ++value) {
        const auto c = static_cast<unsigned char>(*value);
        const auto& coverage = selected.coverage[(c >= 32 && c < 127 ? c : '?')-32];
        for (int row = 0; row < selected.height; ++row)
            for (int col = 0; col < selected.advance; ++col)
                blendPixel(x+col, y+row, color,coverage[row*selected.advance+col]);
        x += selected.advance;
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
