#include "renderer.hpp"
#include <algorithm>
#include <new>

namespace {
// Physical 1920x1080, logical 1080x1920. Fixed 3x3 layout.
constexpr Rectangle slot(std::size_t index) {
    return {33 + static_cast<std::uint32_t>(index % 3) * 349,
            33 + static_cast<std::uint32_t>(index / 3) * 629, 316, 596};
}
}

//QA
bool Renderer::paint(FrameView frame) const noexcept
{
    if (!frame.valid() || frame.width != 1920 || frame.height != 1080) return false;
    RegionCanvas background{frame, {0, 0, 1080, 1920}};
    background.clear(0);
    for (std::size_t i = 0; i < PanelCount; ++i) {
        if (panels_[i]) {
            RegionCanvas canvas{frame, slot(i)};
            panels_[i]->paint(canvas);
        }
    }
    return true;
}

bool Renderer::render(FrameView frame) noexcept
{
    if (!frame.valid() || frame.width != 1920 || frame.height != 1080 ||
        frame.bufferIndex >= painted_.size()) return false;
    const auto b = frame.bufferIndex;
    if (!initialized_[b]) {
        RegionCanvas background{frame, {0, 0, 1080, 1920}};
        background.clear(0);
        initialized_[b] = true;
    }
    for (std::size_t i = 0; i < PanelCount; ++i) {
        if (painted_[b][i] == revisions_[i]) continue;
        RegionCanvas canvas{frame, slot(i)};
        canvas.clear(0);
        if (panels_[i]) panels_[i]->paint(canvas);
        painted_[b][i] = revisions_[i];
    }
    return true;
}

// Dispatches update call to panels to update internal state if needed
void Renderer::updateDue(Panel::Clock::time_point now) noexcept
{
    for (std::size_t i = 0; i < PanelCount; ++i)
        if (panels_[i] && panels_[i]->nextUpdate() <= now && panels_[i]->update(now))
            ++revisions_[i];
}

Panel::Clock::time_point Renderer::nextUpdate() const noexcept
{
    auto next = Panel::Clock::time_point::max();
    for (const auto& panel : panels_) if (panel) next = std::min(next, panel->nextUpdate());
    return next;
}
