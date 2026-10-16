#pragma once
#include "starfox/simulation/game_simulation.hpp"
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <limits>

namespace starfox::render {
// The white/red screen-fill programs request radius 197 with speed 18 and
// acceleration -1. Growth stops at 18+17+...+1 = 171, which covers 256x224,
// not the additional widescreen/fit-to-screen corners.
// Scale only these programs, in logical pixels, before either presenter sees
// the circle. Ordinary bombs, menu disks and the saved simulation stay exact.
inline simulation::CircleEffectState fit_screen_fill_circle(
    simulation::CircleEffectState circle,unsigned width,unsigned height,
    unsigned animation,unsigned first_screen_fill,unsigned end_screen_fill) {
    if(!circle.active || !circle.radius || !width || !height
        || (width<=256 && height<=224) || first_screen_fill>=end_screen_fill
        || animation<first_screen_fill || animation>=end_screen_fill) return circle;
    const double dx=std::max(std::abs(double(circle.centre_x)),
        std::abs(double(width)-circle.centre_x));
    const double dy=std::max(std::abs(double(circle.centre_y)),
        std::abs(double(height)-circle.centre_y));
    const double factor=std::max(1.0,std::hypot(dx,dy)/171.0);
    circle.radius=std::uint16_t(std::min(std::ceil(circle.radius*factor),
        double(std::numeric_limits<std::uint16_t>::max())));
    return circle;
}
}
