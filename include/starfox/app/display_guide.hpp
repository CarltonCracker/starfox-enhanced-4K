#pragma once

#include "starfox/render/framebuffer.hpp"
#include "starfox/simulation/game_simulation.hpp"

namespace starfox::app {

[[nodiscard]] constexpr bool display_guide_visible(
    simulation::PregamePage page, std::uint8_t selection) noexcept {
    return page == simulation::PregamePage::main
        && (selection == 3U || selection == 43U);
}

// Outline the actual presented canvas, not the central 256-pixel menu panel.
// Normal presentation then puts this frame inside any letterbox/pillarbox;
// FIT TO SCREEN reaches the device edges, unless integer scaling is enabled.
// This is static host UI: it needs neither a gameplay preview nor GPU effects.
inline void draw_display_guide(render::Framebuffer& frame,
    std::uint8_t colour = 122U) {
    const auto width = static_cast<std::int32_t>(frame.width());
    const auto height = static_cast<std::int32_t>(frame.height());
    if (width == 0 || height == 0) return;
    for (std::int32_t x = 0; x < width; ++x) {
        frame.set(x, 0, colour);
        frame.set(x, height - 1, colour);
    }
    for (std::int32_t y = 1; y < height - 1; ++y) {
        frame.set(0, y, colour);
        frame.set(width - 1, y, colour);
    }
}

} // namespace starfox::app
