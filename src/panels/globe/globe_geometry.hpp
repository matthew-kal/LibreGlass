#pragma once

#include <algorithm>
#include <cmath>
#include <numbers>

namespace Globe {

inline constexpr double radians = std::numbers::pi / 180;
inline constexpr double tilt = 23.4 * radians;
inline constexpr double elevation = 15 * radians;
inline constexpr double rotationPeriod = 90;

struct Point {
    double x;
    double y;
    double depth;
};

struct Coordinate {
    double latitude;
    double longitude;
};

inline Point project(double latitude, double longitude, double rotation) noexcept
{
    const double lat = latitude * radians;
    const double lon = longitude * radians - rotation;
    const double x = std::cos(lat) * std::sin(lon);
    const double y = std::sin(lat);
    const double z = std::cos(lat) * std::cos(lon);
    const double vertical = y * std::cos(elevation) - z * std::sin(elevation);
    return {x * std::cos(tilt) - vertical * std::sin(tilt),
            -x * std::sin(tilt) - vertical * std::cos(tilt),
            y * std::sin(elevation) + z * std::cos(elevation)};
}

inline Coordinate unproject(double x, double y, double rotation) noexcept
{
    const double depth = std::sqrt(std::max(0., 1 - x*x - y*y));
    const double horizontal = x * std::cos(tilt) - y * std::sin(tilt);
    const double vertical = -x * std::sin(tilt) - y * std::cos(tilt);
    const double north = vertical * std::cos(elevation) + depth * std::sin(elevation);
    const double forward = depth * std::cos(elevation) - vertical * std::sin(elevation);
    return {std::asin(std::clamp(north, -1., 1.)) / radians,
            (std::atan2(horizontal, forward) + rotation) / radians};
}

}
