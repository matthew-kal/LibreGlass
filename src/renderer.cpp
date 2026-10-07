#include "renderer.hpp"

#include <algorithm>
#include <utility>

bool Renderer::setPanel(std::size_t index, std::unique_ptr<Panel> panel) noexcept
{
    if (index >= panels_.size()) {
        return false;
    }

    panels_[index] = std::move(panel);
    return true;
}

void Renderer::update(Panel::Clock::time_point now) noexcept
{
    for (auto& panel : panels_) {
        if (panel) {
            panel->update(now);
        }
    }
}

bool Renderer::paint(FrameView frame, DisplayRotation rotation) const noexcept
{
    if (!frame.valid() || (rotation != DisplayRotation::None &&
                           rotation != DisplayRotation::Clockwise90)) {
        return false;
    }

    constexpr std::uint32_t columns = 3;
    constexpr std::uint32_t rows = 3;
    const auto logicalWidth = rotation == DisplayRotation::Clockwise90
        ? frame.height : frame.width;
    const auto logicalHeight = rotation == DisplayRotation::Clockwise90
        ? frame.width : frame.height;

    RegionCanvas background{frame, {0, 0, logicalWidth, logicalHeight}, rotation};
    background.clear(0);

    const auto gap = std::min(logicalWidth, logicalHeight) / 32;
    const auto panelWidth = (logicalWidth - gap * (columns + 1)) / columns;
    const auto panelHeight = (logicalHeight - gap * (rows + 1)) / rows;

    for (std::uint32_t row = 0; row < rows; ++row) {
        for (std::uint32_t column = 0; column < columns; ++column) {
            const auto& panel = panels_[row * columns + column];
            if (!panel) {
                continue;
            }

            const auto x = gap + column * (panelWidth + gap);
            const auto y = gap + row * (panelHeight + gap);
            const auto width = column == columns - 1 ? logicalWidth - gap - x : panelWidth;
            const auto height = row == rows - 1 ? logicalHeight - gap - y : panelHeight;
            if (width == 0 || height == 0) {
                continue;
            }

            RegionCanvas canvas{frame, {x, y, width, height}, rotation};
            panel->paint(canvas);
        }
    }

    return true;
}
