#pragma once

#include "../../panel.hpp"

#include <cstddef>

class WeatherPanel final : public Panel {
public:
    static constexpr std::size_t index = 1;

    bool update(Clock::time_point now) noexcept override;
    void paint(RegionCanvas& canvas) const noexcept override;
};
