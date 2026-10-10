#pragma once

#include <chrono>

class RegionCanvas;

class Panel {
public:

    // 
    using Clock = std::chrono::steady_clock;

    virtual ~Panel() = default;
    // update changes state; paint only reproduces it into a temporary canvas.
    virtual bool update(Clock::time_point now) noexcept = 0;
    virtual Clock::time_point nextUpdate() const noexcept { return Clock::time_point::max(); }
    virtual void paint(RegionCanvas& canvas) const noexcept = 0;
};
