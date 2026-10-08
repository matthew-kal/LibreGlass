#pragma once

#include "../panel.hpp"

#include <cstddef>

class SshPanel final : public Panel {
public:
    static constexpr std::size_t index = 0;

    void update(Clock::time_point now) noexcept override;
    void paint(RegionCanvas& canvas) const noexcept override;
};
