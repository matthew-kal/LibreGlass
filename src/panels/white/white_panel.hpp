#pragma once

#include "../panel.hpp"
#include "../../region_canvas.hpp"
#include <algorithm>
#include <cmath>

class WhitePanel final : public Panel {
public:
    static constexpr std::size_t index = 3;
    bool update(Clock::time_point) noexcept override { return false; }
    void paint(RegionCanvas& canvas) const noexcept override
    {
        const auto width = canvas.width(), height = canvas.height();
        const auto radius = std::min({14u, width / 2, height / 2});
        canvas.clear(0);
        canvas.fillRectangle({0, radius, width, height - 2 * radius}, 0xffffff);
        canvas.fillRectangle({radius, 0, width - 2 * radius, radius}, 0xffffff);
        canvas.fillRectangle({radius, height - radius, width - 2 * radius, radius}, 0xffffff);
        for (unsigned y = 0; y < radius; ++y) {
            for (unsigned x = 0; x < radius; ++x) {
                const double dx = radius - x - .5, dy = radius - y - .5;
                const auto opacity = static_cast<std::uint8_t>(
                    std::clamp(radius + .5 - std::hypot(dx, dy), 0., 1.) * 255);
                canvas.blendPixel(x, y, 0xffffff, opacity);
                canvas.blendPixel(width - 1 - x, y, 0xffffff, opacity);
                canvas.blendPixel(x, height - 1 - y, 0xffffff, opacity);
                canvas.blendPixel(width - 1 - x, height - 1 - y, 0xffffff, opacity);
            }
        }
        canvas.label(16, 16, "to do", 0x121214, RegionCanvas::TextStyle::Body);
    }
};
