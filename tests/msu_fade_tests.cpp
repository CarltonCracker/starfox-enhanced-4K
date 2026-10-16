#include "starfox/assets/rom.hpp"
#include "starfox/audio/msu1_audio.hpp"
#include "starfox/simulation/game_simulation.hpp"
#include <algorithm>
#include <fstream>
#include <iostream>
#include <iterator>
#include <memory>
#include <stdexcept>

namespace {
void require(bool ok,const char* message) {if(!ok) throw std::runtime_error(message);}
}
int main(int argc,char** argv) {try {
    if(argc!=4) return 2;
    const auto rom=starfox::assets::RomImage::load(argv[1]);
    const auto symbols=starfox::assets::SymbolMap::load(argv[2]);
    std::ifstream file(argv[3],std::ios::binary);
    const std::vector<std::uint8_t> tone{std::istreambuf_iterator<char>(file),{}};
    require(!tone.empty(),"Missing small FLAC fixture");
    for(const unsigned command:{0xf0U,0xf1U}) {
        auto game=std::make_unique<starfox::simulation::GameSimulation>(rom,symbols,"LEVEL1_2");
        game->set_god_mode(true);
        for(unsigned i=0;i<100;++i) (void)game->tick({});
        starfox::audio::Msu1Audio audio{[&](std::uint16_t){return tone;}};
        audio.set_enabled(true);
        const std::array<starfox::simulation::MsuRegisterWrite,4> play{{
            {0x2004,5,0},{0x2005,0,0},{0x2006,255,0},{0x2007,3,0}}};
        audio.process_register_writes(play);
        require(std::ranges::any_of(audio.render(800,48000),[](auto s){return s!=0;}),"Fixture music is silent");
        auto& map=game->map();
        const auto addr=[&](const char* name){return symbols.find(name).at(0);};
        map.write_native_byte(addr("MSUFADEFLAG"),0);
        map.write_native_byte(addr("CURMSUVOLUME"),255);
        map.write_native_byte(addr("BGM_MUSIC"),static_cast<std::uint8_t>(command));
        map.write_native_byte(addr("BGMCNT"),0);
        (void)map.take_msu_register_writes();
        // Run the real shared simulation/IRQ path, not the desktop-only host
        // fade shim. The source DSTRATS boss explosion uses $f1 without msufade.
        bool stopped=false,saw_fade=false;
        unsigned previous=255;
        for(unsigned frame=0;frame<130;++frame) {
            game->present_frame();
            if(game->logic_tick_ready()) (void)game->tick({});
            const auto writes=map.take_msu_register_writes();
            for(const auto& w:writes) {
                if(w.address==0x2006) {
                    require(w.value<=previous,"Stop/fade unexpectedly raised music volume");
                    previous=w.value;saw_fade|=w.value>0 && w.value<255;
                }
                if(w.address==0x2007 && w.value==0) stopped=true;
            }
            audio.process_register_writes(writes);
            const auto samples=audio.render(800,48000);
            if(frame>=110) require(std::ranges::none_of(samples,[](auto s){return s!=0;}),"Boss music still audible after native stop/fade");
        }
        require(stopped,"Native music command left the replacement track looping");
        require(command!=0xf1U || saw_fade,"Native fade was replaced with an abrupt cut");
        // Source-started replacement tracks must still be able to play later.
        audio.process_register_writes(play);
        require(std::ranges::any_of(audio.render(800,48000),[](auto s){return s!=0;}),"Music remained muted after the next track started");
        std::cout<<"PASS: native command "<<command<<" stops MSU playback, fade="<<saw_fade<<", next track audible\n";
    }
    // Explicit source MSU fades are also handled by this shared 60 Hz path.
    auto game=std::make_unique<starfox::simulation::GameSimulation>(rom,symbols,"LEVEL1_2");
    const auto flag=symbols.find("MSUFADEFLAG").at(0),volume=symbols.find("CURMSUVOLUME").at(0);
    game->map().write_native_byte(flag,1);game->map().write_native_byte(volume,12);
    require(game->toggle_runtime_options(),"Could not freeze fade fixture");
    for(unsigned i=0;i<12;++i) game->present_frame();
    require(game->map().read_native_byte(volume)==12,"Options advanced a paused MSU fade");
    require(game->toggle_runtime_options(),"Could not resume fade fixture");
    for(unsigned expected:{9U,6U,3U,0U}) {
        game->present_frame();
        require(game->map().read_native_byte(volume)==expected,"Shared fade did not follow source's 3/60Hz decrement");
        const auto saved=game->save_state();game=game->restored_state(saved);
    }
    std::cout<<"PASS: source fade flag, paused options, source cadence and saved fade continuation\n";
    return 0;
}catch(const std::exception& error){std::cerr<<error.what()<<'\n';return 1;}}
