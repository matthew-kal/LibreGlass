#pragma once
#include "../panel.hpp"
#include <cstddef>

class SshPanel final : public Panel {
public:
    static constexpr std::size_t index = 4;
    void setConnected(bool connected, Clock::time_point now) noexcept;
    bool update(Clock::time_point now) noexcept override;
    Clock::time_point nextUpdate() const noexcept override { return next_; }
    void paint(RegionCanvas& canvas) const noexcept override;
    //QA
    bool connected() const noexcept { return connected_; }
private:
    // connected_: whether at least one SSH session is connected.
    // started_: whether the animation's starting timestamp has been set.
    // changed_: forces update() to report a change initially or after a connection change.
    // epoch_: starting point for current animation 
    // next_: timestamp when the event loop should next update this panel.
    // openness_: eyelid opening, from 0 (closed) to 1 (fully open).
    // gaze_: horizontal gaze offset, scaled into pixels when painting the iris.
    // rotation_: current ornament rotation in radians.
    // gazeY_: vertical gaze offset, scaled into pixels when painting the iris.
    // transitionOpening_: eyelid opening saved when the connection state changes.
    // transitionRotation_: ornament rotation saved when the connection state changes.
    bool connected_{};
    bool started_{};
    bool changed_ = true;
    Clock::time_point epoch_{};
    Clock::time_point next_{};
    float openness_{};
    float gaze_{};
    float rotation_{};
    float gazeY_{};
    float transitionOpening_{};
    float transitionRotation_{};
};
