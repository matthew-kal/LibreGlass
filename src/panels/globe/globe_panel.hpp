#pragma once

#include "../panel.hpp"
#include <cstddef>

class GlobePanel final : public Panel {
public:
    static constexpr std::size_t index = 0;

    bool update(Clock::time_point now) noexcept override;
    Clock::time_point nextUpdate() const noexcept override { return next_; }
    void paint(RegionCanvas& canvas) const noexcept override;

private:
    Clock::time_point epoch_{};
    Clock::time_point next_{};
    bool started_{};
    double rotation_ = -0.6981317007977318;
};
