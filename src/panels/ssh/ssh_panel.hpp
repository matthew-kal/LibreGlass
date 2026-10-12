#pragma once
#include "../panel.hpp"
#include <cstddef>

class SshPanel final : public Panel {
public:
    static constexpr std::size_t index = 1;
    void setConnected(bool connected, Clock::time_point now) noexcept;
    bool update(Clock::time_point now) noexcept override;
    Clock::time_point nextUpdate() const noexcept override { return next_; }
    void paint(RegionCanvas& canvas) const noexcept override;
    //QA
    bool connected() const noexcept { return connected_; }
private:
    /// connected_: whether this user has at least one SSH session.
    bool connected_{};

    /// started_: whether the animation's starting timestamp has been set.
    bool started_{};

    /// changed_: forces update() to report a change initially or after a connection change.
    bool changed_ = true;

    /// epoch_: starting point for current animation
    Clock::time_point epoch_{};

    /// next_: timestamp when the event loop should next update this panel.
    Clock::time_point next_{};

    /// openness_: eyelid opening, from 0 (closed) to 1 (fully open).
    float openness_{};

    /// gaze_: horizontal gaze offset, scaled into pixels when painting the iris.
    float gaze_{};

    /// rotation_: current ornament rotation in radians.
    float rotation_{};

    /// gazeY_: vertical gaze offset, scaled into pixels when painting the iris.
    float gazeY_{};

    /// transitionOpening_: eyelid opening saved when the connection state changes.
    float transitionOpening_{};

    /// transitionRotation_: ornament rotation saved when the connection state changes.
    float transitionRotation_{};
};
