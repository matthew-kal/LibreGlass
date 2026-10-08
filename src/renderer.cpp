#include "renderer.hpp"

#include <algorithm>

bool Renderer::paint(FrameView frame) const noexcept
{
    if (!frame.valid()) {
        return false;
    }

    constexpr std::uint32_t columns = 3;
    constexpr std::uint32_t rows = 3;
    const auto logicalWidth = frame.height;
    const auto logicalHeight = frame.width;

    RegionCanvas background{frame, {0, 0, logicalWidth, logicalHeight}};
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

            RegionCanvas canvas{frame, {x, y, width, height}};
            panel->paint(canvas);
        }
    }

    return true;
}
