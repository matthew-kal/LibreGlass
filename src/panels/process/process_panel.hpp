#pragma once
#include "../panel.hpp"
#include "../../drm_device.hpp"
#include "kernel_stats.hpp"
#include <array>

class ProcessPanel final : public Panel {
public:
    static constexpr std::size_t index = 1;
    //QA
    explicit ProcessPanel(const DrmDevice& device, const char* procRoot="/proc") noexcept
        : device_(device), reader_(procRoot) {}
    bool update(Clock::time_point now) noexcept override;
    Clock::time_point nextUpdate() const noexcept override { return next_; }
    void paint(RegionCanvas& canvas) const noexcept override;
    const KernelSnapshot& snapshot() const noexcept { return snapshot_; }
private:
    const DrmDevice& device_;
    KernelStats reader_;
    KernelSnapshot snapshot_{};
    std::array<DrmDevice::BufferInfo,2> buffers_{};
    std::array<float,32> history_{};
    Clock::time_point next_{};
    Clock::time_point nextSample_{}, previousAnimation_{};
    bool animationStarted_{};
    double displayedStackDepth_{};
    double stackScale_ = 16 * 1024;
};
