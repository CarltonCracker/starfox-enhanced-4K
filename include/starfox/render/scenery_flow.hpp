#pragma once
#include "starfox/simulation/game_simulation.hpp"

namespace starfox::render {
// The results task starts before the fly-away has finished. Its BG2 is still
// world scenery; only the tally/portraits belong to the protected UI layer.
constexpr bool world_scenery_flow(simulation::GameFlowState flow) noexcept {
    using simulation::GameFlowState;
    return flow==GameFlowState::gameplay || flow==GameFlowState::training
        || flow==GameFlowState::intro || flow==GameFlowState::stage_results;
}
constexpr bool late_cartridge_objects(bool stage_hud,bool briefing_layers,unsigned main_screen) noexcept {
    // Stage results already restore every OBJ priority below the host tally.
    // Restoring priority 3 again after it puts the cartridge meter over digits.
    return (main_screen&0x10U) && !briefing_layers && !stage_hud;
}
}
