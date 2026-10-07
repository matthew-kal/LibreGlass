#pragma once

#include <chrono>

class RegionCanvas;

class Panel {
public:
    using Clock = std::chrono::steady_clock;

    virtual ~Panel() = default;
    virtual void update(Clock::time_point now) noexcept = 0;
    virtual void paint(RegionCanvas& canvas) const noexcept = 0;
};
