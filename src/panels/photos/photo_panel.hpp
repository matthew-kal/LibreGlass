#pragma once

#include "../panel.hpp"
#include "photo_loader.hpp"

class PhotoPanel final : public Panel {
public:
    static constexpr std::size_t index = 4;
    explicit PhotoPanel(PhotoLoader& loader, std::chrono::seconds interval = std::chrono::seconds(30)) noexcept
        : loader_(loader), interval_(interval) {}
    bool accept(PhotoSnapshot snapshot, Clock::time_point now) noexcept;
    bool update(Clock::time_point now) noexcept override;
    Clock::time_point nextUpdate() const noexcept override { return next_; }
    void paint(RegionCanvas& canvas) const noexcept override;

private:
    void request();
    PhotoLoader& loader_;
    std::chrono::seconds interval_;
    std::shared_ptr<const PhotoList> files_;
    std::shared_ptr<const PhotoImage> current_, upcoming_;
    std::size_t position_{};
    Clock::time_point next_ = Clock::time_point::max();
    Clock::time_point slideAt_{}, fadeAt_{};
    unsigned char opacity_{};
    bool fading_{}, failed_{};
};
