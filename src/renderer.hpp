#pragma once

#include "panel.hpp"
#include "region_canvas.hpp"

#include <array>
#include <memory>

class Renderer {
public:
    static constexpr std::size_t PanelCount = 9;

    bool setPanel(std::size_t index, std::unique_ptr<Panel> panel) noexcept;
    void update(Panel::Clock::time_point now) noexcept;
    bool paint(FrameView frame,
               DisplayRotation rotation = DisplayRotation::None) const noexcept;

private:
    std::array<std::unique_ptr<Panel>, PanelCount> panels_{};
};
