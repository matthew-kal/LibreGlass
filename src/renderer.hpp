#pragma once

#include "panel.hpp"
#include "region_canvas.hpp"

#include <array>
#include <concepts>
#include <cstddef>
#include <memory>
#include <utility>

class Renderer {
public:

    // 3x3 grid of panels on the mirror
    static constexpr std::size_t PanelCount = 9;

    // Supply a pack of panels to be evaluated at compile time
    // Hand ownership to the Renderer if succesful, otherwise throw
    template <typename... PanelTypes>
        requires (std::derived_from<PanelTypes, Panel> && ...)
    explicit Renderer(std::unique_ptr<PanelTypes>... panels) noexcept
    {
        static_assert(validPanelSlots<PanelTypes...>(),
                      "Panel indices must be in range and unique");

        ((panels_[PanelTypes::index] = std::move(panels)), ...);
    }

    template <std::size_t Index>
    bool update(Panel::Clock::time_point now) noexcept
    {
        static_assert(Index < PanelCount,
                      "Panel index must be in the range 0 through 8");

        auto& panel = panels_[Index];
        if (!panel) {
            return false;
        }

        panel->update(now);
        return true;
    }

    // Paint the fixed 90-degree portrait layout.
    bool paint(FrameView frame) const noexcept;

private:

    // Helper function for renderer, will also
    // evaluate at compile time.
    template <typename... PanelTypes>
    static consteval bool validPanelSlots()
    {
        std::array<bool, PanelCount> occupied{};
        constexpr std::array<std::size_t, sizeof...(PanelTypes)> indices{
            PanelTypes::index...
        };

        for (const auto index : indices) {
            if (index >= PanelCount || occupied[index]) {
                return false;
            }

            occupied[index] = true;
        }

        return true;
    }

    std::array<std::unique_ptr<Panel>, PanelCount> panels_{};
};
