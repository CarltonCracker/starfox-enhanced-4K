#include "starfox/simulation/game_simulation.hpp"
#include "starfox/timing/fixed_step.hpp"

#include <algorithm>
#include <iostream>
#include <memory>
#include <stdexcept>
#include <vector>

namespace {
void require(bool value, const char* message) {
    if (!value) throw std::runtime_error(message);
}

void check_death_msu(const starfox::assets::RomImage& rom,
    const starfox::assets::SymbolMap& symbols, bool cockpit, bool hp0, unsigned orchestra) {
    using namespace starfox::simulation;
    const auto addr = [&](const char* name) { return symbols.find(name).at(0); };
    auto game = std::make_unique<GameSimulation>(rom, symbols, "LEVEL1_1");
    game->set_timing_mode(TimingMode::unlocked_20_fps);
    for (unsigned tick = 0; tick < 200; ++tick) (void)game->tick({});
    game->map().write_native_byte(addr("ORCHORBAND"), orchestra);
    game->map().write_native_byte(addr("BGMCNT"), 2U);
    game->map().write_native_byte(addr("BGM_MUSIC"), 5U);
    game->map().write_native_byte(addr("PSHIPFLAGS2"), hp0 ? 0x80U : 0U);
    if (cockpit) game->map().write_native_byte(addr("SPLAYERFLYMODE"), 3U);
    game->objects().at(game->player()).strategy_address = addr("PLAYERDEAD_ISTRAT");
    (void)game->map().take_msu_register_writes();
    unsigned track{}, control{}, starts{};
    for (unsigned tick = 0; tick < 25; ++tick) {
        (void)game->tick({});
        bool selected_death = false;
        for (const auto& write : game->map().take_msu_register_writes()) {
            if (write.address == 0x2004U) track = (track & 0xff00U) | write.value;
            if (write.address == 0x2005U) track = (track & 0xffU) | unsigned(write.value) << 8U;
            if (write.address == 0x2007U) {
                control = write.value;
                if ((control & 1U) && (track == 38U || track == 39U)) {
                    selected_death = true; ++starts;
                }
            }
        }
        if (selected_death) {
            require(track == 38U + orchestra, "Wrong band/orchestra death cue");
            require(control == 1U, "Death cue was stopped in the same audio batch that started it");
        }
    }
    require(starts != 0U, "Death did not start an MSU one-shot cue");
}

std::vector<unsigned> check_tally(const starfox::assets::RomImage& rom,
    const starfox::assets::SymbolMap& symbols, unsigned percentage,
    unsigned previous, bool bonus, unsigned fps) {
    using namespace starfox::simulation;
    const auto addr = [&](const char* name) { return symbols.find(name).at(0); };
    auto game = std::make_unique<GameSimulation>(rom, symbols, "LEVEL1_1");
    game->set_god_mode(true);
    game->set_timing_mode(TimingMode::unlocked_20_fps);
    for (unsigned tick = 0; tick < 30; ++tick) (void)game->tick({});
    for (const auto* friend_hp : {"BUNNY", "COCK", "FROG"})
        game->map().write_native_byte(addr(friend_hp), 0U);
    game->map().write_native_byte(addr("SPECIALOBJTOTAL"), 100U);
    game->map().write_native_byte(addr("SPECIALS_DEAD"), percentage);
    game->map().write_native_word(addr("SPECPTR"), 2U);
    game->map().write_native_byte(addr("SPECBUF"), previous);
    game->map().write_native_byte(addr("SPECBUF") + 1U, 101U); // route marker is not a score.
    game->map().write_native_byte(addr("CREDITS"), 3U);
    game->map().write_native_byte(addr("CLB2"), 0U);
    game->map().write_native_byte(addr("LEVELFINISHED"), 1U);
    (void)game->tick({});
    require(game->stage_results_state().active, "Fixture did not enter retail tally");
    require(game->stage_results_state().percentage == percentage, "Wrong target percentage");

    unsigned clicks{}, completions{}, bonuses{};
    std::vector<unsigned> timeline;
    starfox::timing::RasterPhaseClock clock;
    unsigned raster{};
    for (unsigned frame = 0; frame < fps * 6U; ++frame) {
        const auto batch = clock.advance(fps);
        for (unsigned phase = 0; phase < batch.video_phases; ++phase) {
            game->present_frame(); ++raster;
            if (raster % 3U != 0U) continue;
            const auto tick = game->tick({});
            for (auto command : tick.sound_effect_commands) {
                if (command == 0x12U) { ++clicks; timeline.push_back(raster * 256U + command); }
                if (command == 0x11U) { ++completions; timeline.push_back(raster * 256U + command); }
                if (command == 0x1aU) { ++bonuses; timeline.push_back(raster * 256U + command); }
            }
            if (raster == 90U || raster == 285U)
                game = game->restored_state(game->save_state());
        }
    }
    require(clicks == std::max(1U, (percentage + 2U) / 3U), "Missing/repeating tally clicks");
    require(completions == 1U, "Missing/repeating tally completion sound");
    require(bonuses == unsigned(bonus), "Wrong credit-bonus sound count");
    require(game->stage_results_state().credit_bonus == bonus, "Bonus text does not match award");
    require(game->map().read_native_byte(addr("CREDITS")) == 3U + unsigned(bonus),
        "Credit award missing or awarded more than once");
    require(game->map().read_native_byte(addr("ONECREDSPR")) == unsigned(bonus),
        "Bonus-credit sprite does not match award");
    require(game->map().read_native_word(addr("SPECPTR")) == 3U,
        "Tally did not record its percentage exactly once");
    require(game->map().read_native_byte(addr("SPECBUF") + 2U) == percentage,
        "Tally stored wrong percentage");
    const auto count_ticks = std::max(1U, (percentage + 2U) / 3U);
    require(timeline.at(count_ticks) == (count_ticks + 18U) * 3U * 256U + 0x11U,
        "Completion cue lost the source eighteen-update delay");
    if (bonus) require(timeline.back() == (count_ticks + 36U) * 3U * 256U + 0x1aU,
        "Credit cue lost the source bonus delay");
    // Finish via the authored exit latch; finish_stage_results must not append again.
    game->map().write_native_byte(addr("CLB2"), 1U);
    game->map().write_native_byte(addr("DONEACIRCLE"), 1U);
    (void)game->tick({});
    require(!game->stage_results_state().active, "Tally ignored source exit signal");
    require(game->map().read_native_word(addr("SPECPTR")) == 3U, "Exit stored the score twice");
    require(game->map().read_native_byte(addr("ONECREDSPR")) == 0U, "Credit sprite survived stage exit");
    return timeline;
}
}

int main(int argc, char** argv) try {
    if (argc != 3) return 2;
    const auto rom = starfox::assets::RomImage::load(argv[1]);
    const auto symbols = starfox::assets::SymbolMap::load(argv[2]);
    for (const auto percentage : {0U, 9U, 10U, 100U}) {
        const bool bonus = percentage >= 10U; // 90 + score crosses the source 100 threshold.
        const auto timeline = check_tally(rom, symbols, percentage, 90U, bonus, 60U);
        require(timeline == check_tally(rom, symbols, percentage, 90U, bonus, 120U),
            "Presentation FPS changed results audio cadence");
    }
    (void)check_tally(rom, symbols, 100U, 100U, false, 60U); // no threshold at 200.
    for (bool cockpit : {false, true})
        for (bool hp0 : {false, true})
            for (unsigned orchestra : {0U, 1U})
                check_death_msu(rom, symbols, cockpit, hp0, orchestra);
    std::cout << "Retail tally sounds, threshold bonus and exit pass at 60/120 Hz\n";
    return 0;
} catch (const std::exception& error) { std::cerr << error.what() << '\n'; return 1; }
