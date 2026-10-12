#include "globe_panel.hpp"
#include "globe_geometry.hpp"
#include "land.hpp"
#include "locations.hpp"
#include "../../region_canvas.hpp"
#include <algorithm>
#include <array>
#include <cmath>

namespace {

constexpr unsigned ivory = 0xf2f0e9;
constexpr unsigned scarlet = 0xe43b4f;
constexpr int radius = 150;
constexpr int extent = radius + 3;
constexpr int diameter = extent * 2 + 1;

struct Tile {
    double latitude{};
    double longitude{};
    double light{};
    double rim{};
    double coverage{};
    bool inside{};
};

const auto& surface() noexcept
{
    static const auto tiles = [] {
        std::array<Tile, diameter * diameter> result{};
        for (int row = -extent; row <= extent; ++row) {
            for (int col = -extent; col <= extent; ++col) {
                const double x = double(col) / radius;
                const double y = double(row) / radius;
                const double square = x*x + y*y;
                const double distance = std::sqrt(square) * radius;
                if (distance > radius+3) continue;
                const auto coordinate = Globe::unproject(x, y, 0);
                const double depth = std::sqrt(std::max(0., 1-square));
                const double light = .22 + .78*std::max(0., -x*.36-y*.42+depth*.83);
                result[(row+extent)*diameter+col+extent] = {
                    coordinate.latitude, coordinate.longitude,
                    light, std::pow(1-depth, 3),
                    std::clamp(radius+.5-distance, 0., 1.), true};
            }
        }
        return result;
    }();
    return tiles;
}

struct Color {
    double red;
    double green;
    double blue;
};

Color blend(Color a, Color b, double amount) noexcept
{
    return {a.red+(b.red-a.red)*amount, a.green+(b.green-a.green)*amount,
            a.blue+(b.blue-a.blue)*amount};
}

unsigned packed(Color color, double brightness = 1) noexcept
{
    const auto channel = [brightness](double value) {
        return unsigned(std::clamp(std::lround(value*brightness), 0l, 255l));
    };
    return channel(color.red)<<16 | channel(color.green)<<8 | channel(color.blue);
}

Color terrain(double latitude, double longitude) noexcept
{
    const double lat = latitude * Globe::radians;
    const double lon = longitude * Globe::radians;
    const double variation = .5+.25*std::sin(lon*11+std::sin(lat*9))+
                             .25*std::sin(lat*23-lon*7);
    Color color = blend({57, 108, 66}, {104, 137, 80}, variation);
    const double northDesert = std::exp(-std::pow((latitude-25)/13, 2));
    const double sahara = std::clamp((longitude+20)/12, 0., 1.) *
                          std::clamp((65-longitude)/15, 0., 1.);
    const double australia = std::exp(-std::pow((latitude+25)/12, 2)) *
                             std::clamp((longitude-110)/12, 0., 1.) *
                             std::clamp((155-longitude)/12, 0., 1.);
    color = blend(color, {186, 163, 112}, std::max(northDesert*sahara, australia)*.85);
    const double ice = std::clamp((std::abs(latitude)-66)/12, 0., 1.);
    return blend(color, {219, 228, 222}, ice);
}

void marker(RegionCanvas& canvas, int x, int y, Globe::Marker kind) noexcept
{
    if (kind == Globe::Marker::Pin) {
        for (int dy = -6; dy <= 0; ++dy)
            for (int dx = -3; dx <= 3; ++dx) {
                const double distance = std::hypot(double(dx), double(dy+4));
                if (distance <= 2.6)
                    canvas.pixel(x+dx, y+dy, distance < 1.6 ? scarlet : 0x271c27);
                else if (dy >= -2 && dx == 0) canvas.pixel(x, y+dy, ivory);
            }
        canvas.pixel(x-1, y-5, 0xffb5ab);
        return;
    }

    constexpr std::array<unsigned, 7> heart{0b0110110, 0b1111111, 0b1111111,
        0b1111111, 0b0111110, 0b0011100, 0b0001000};
    constexpr std::array<unsigned, 9> rutgers{0b11111100, 0b11111110, 0b01100110,
        0b01100110, 0b01111100, 0b01111000, 0b01101100, 0b01100110, 0b11100111};
    const auto draw = [&](const auto& rows, int left, int top, int width) {
        for (std::size_t row = 0; row < rows.size(); ++row)
            for (int col = 0; col < width; ++col)
                if (rows[row] & (1u << (width-1-col)))
                    for (int dy = -1; dy <= 1; ++dy)
                        for (int dx = -1; dx <= 1; ++dx)
                            canvas.pixel(left+col+dx, top+int(row)+dy, ivory);
        for (std::size_t row = 0; row < rows.size(); ++row)
            for (int col = 0; col < width; ++col)
                if (rows[row] & (1u << (width-1-col)))
                    canvas.pixel(left+col, top+int(row), scarlet);
    };
    if (kind == Globe::Marker::Heart) draw(heart, x-3, y-9, 7);
    else draw(rutgers, x-4, y+3, 8);
    canvas.pixel(x, y, ivory);
    canvas.pixel(x, y+1, ivory);
    canvas.pixel(x, y+2, ivory);
}

}

bool GlobePanel::update(Clock::time_point now) noexcept
{
    if (!started_) {
        epoch_ = now;
        started_ = true;
    }
    const double elapsed = std::max(0., std::chrono::duration<double>(now-epoch_).count());
    rotation_ = -40 * Globe::radians +
        std::fmod(elapsed, Globe::rotationPeriod) * 2 * std::numbers::pi / Globe::rotationPeriod;
    next_ = now + std::chrono::milliseconds(66);
    return true;
}

void GlobePanel::paint(RegionCanvas& canvas) const noexcept
{
    if (canvas.width() < 316 || canvas.height() < 316) return;
    const int cx = int(canvas.width()) / 2;
    const int cy = int(canvas.height()) / 2;

    const auto& tiles = surface();
    for (int row = -extent; row <= extent; ++row) {
        for (int col = -extent; col <= extent; ++col) {
            const auto& tile = tiles[(row+extent)*diameter+col+extent];
            if (!tile.inside) continue;
            const double rawLongitude = tile.longitude + rotation_ / Globe::radians;
            const double longitude = rawLongitude - 360*std::floor((rawLongitude+180)/360);
            if (!tile.coverage) {
                const double distance = std::hypot(double(col), double(row)) - radius;
                canvas.pixel(cx+col, cy+row, packed({46, 103, 161},
                    std::exp(-distance*1.2)*.3));
                continue;
            }
            const double ground = Globe::landCoverage(tile.latitude, longitude);
            const Color ocean = blend({18, 65, 109}, {27, 101, 146}, ground*.4);
            Color color = blend(ocean, terrain(tile.latitude, longitude), ground);
            color = {color.red*tile.light, color.green*tile.light, color.blue*tile.light};
            color = blend(color, {61, 122, 181}, tile.rim*.32);
            canvas.pixel(cx+col, cy+row, packed(color, tile.coverage));
        }
    }

    struct Visible {
        Globe::Point point;
        Globe::Marker kind;
    };
    std::array<Visible, Globe::locations.size()> visible{};
    std::size_t count = 0;
    for (const auto& location : Globe::locations) {
        const auto point = Globe::project(location.latitude, location.longitude, rotation_);
        if (point.depth > 0) visible[count++] = {point, location.marker};
    }
    std::sort(visible.begin(), visible.begin()+count, [](const Visible& a, const Visible& b) {
        if ((a.kind == Globe::Marker::Pin) != (b.kind == Globe::Marker::Pin))
            return a.kind == Globe::Marker::Pin;
        return a.point.depth < b.point.depth;
    });
    for (std::size_t i = 0; i < count; ++i) {
        const auto& pin = visible[i];
        marker(canvas, cx+int(std::lround(pin.point.x*radius)),
               cy+int(std::lround(pin.point.y*radius)), pin.kind);
    }
}
