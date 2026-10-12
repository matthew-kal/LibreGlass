#include "photo_panel.hpp"
#include "../../region_canvas.hpp"

#include <algorithm>
#include <cstdio>

void PhotoPanel::request()
{
    if (!files_ || files_->empty()) return;
    loader_.request((*files_)[position_], (*files_)[(position_ + 1) % files_->size()]);
}

bool PhotoPanel::accept(PhotoSnapshot snapshot, Clock::time_point now) noexcept
{
    bool changed = failed_ != snapshot.failed;
    failed_ = snapshot.failed;
    try {
        if (files_ != snapshot.files) {
            files_ = std::move(snapshot.files);
            position_ = 0;
            if (files_ && current_) {
                const auto found = std::find(files_->begin(), files_->end(), current_->file);
                if (found != files_->end()) position_ = std::size_t(found - files_->begin());
            }
            fading_ = false;
            opacity_ = 0;
            upcoming_.reset();
            changed = true;
            request();
        }
        if (!files_ || files_->empty()) {
            next_ = Clock::time_point::max();
            return changed;
        }
        for (const auto& image : snapshot.images) {
            if (!image) continue;
            if (image->file == (*files_)[position_] && current_ != image) {
                current_ = image;
                slideAt_ = now + interval_;
                changed = true;
            }
            if (image->file == (*files_)[(position_ + 1) % files_->size()]) upcoming_ = image;
        }
        if (!fading_) next_ = current_ && upcoming_ && files_->size() > 1 ? slideAt_ : Clock::time_point::max();
    } catch (...) {
        std::fputs("Could not apply photo update.\n", stderr);
        failed_ = true;
        changed = true;
    }
    return changed;
}

bool PhotoPanel::update(Clock::time_point now) noexcept
{
    if (!current_ || !upcoming_ || !files_ || files_->size() < 2) {
        next_ = Clock::time_point::max();
        return false;
    }
    if (!fading_) {
        if (now < slideAt_) {
            next_ = slideAt_;
            return false;
        }
        fading_ = true;
        fadeAt_ = now;
    }
    constexpr auto duration = std::chrono::milliseconds(500);
    const auto elapsed = std::max(Clock::duration::zero(), now - fadeAt_);
    const auto previous = opacity_;
    opacity_ = static_cast<unsigned char>(std::clamp(
        std::chrono::duration<double>(elapsed).count() / std::chrono::duration<double>(duration).count(),
        0., 1.) * 255);
    if (elapsed >= duration) {
        current_ = std::move(upcoming_);
        position_ = (position_ + 1) % files_->size();
        fading_ = false;
        opacity_ = 0;
        slideAt_ = now + interval_;
        next_ = Clock::time_point::max();
        try { request(); } catch (...) { failed_ = true; }
        return true;
    }
    next_ = std::min(now + std::chrono::nanoseconds(33333333), fadeAt_ + duration);
    return previous != opacity_;
}

void PhotoPanel::paint(RegionCanvas& canvas) const noexcept
{
    if (current_) {
        canvas.image(0, 0, PhotoImage::width, PhotoImage::height, current_->pixels);
        if (fading_ && upcoming_)
            canvas.image(0, 0, PhotoImage::width, PhotoImage::height, upcoming_->pixels, opacity_);
        return;
    }
    const char* label = failed_ ? "Photos unavailable" : files_ && files_->empty() ? "No photos" : "Loading photos";
    canvas.label((int(canvas.width()) - RegionCanvas::textWidth(label)) / 2,
                 int(canvas.height()) / 2, label, 0x929298);
}
