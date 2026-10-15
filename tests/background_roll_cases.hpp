#pragma once
#include "starfox/simulation/snes_ppu.hpp"
#include "starfox/render/environment_effects.hpp"
#include <stdexcept>
#include <string>

// Offset words may contain either continuous 13-bit scrolls or the source
// atlas's wrapped phase. They address the same pixels; fitting a wide horizon
// must not turn the atlas seam into a discontinuity in the bank angle.
template<class Draw> void check_wrapped_background_roll(Draw draw) {
    using starfox::simulation::SnesPpuState;
    for(unsigned layout=0;layout<3;++layout) {
        SnesPpuState source;
        source.background_mode=2;source.main_screen=2;
        source.bg2_vertical_offsets_enabled=true;
        source.bg2_tile_size_16=layout!=0;source.bg2_screen_size=layout==2?2:0;
        source.bg2_screen_base=0x4000;source.bg2_character_base=0x1000;
        const unsigned height=(layout==2?64:32)*(layout?16:8);
        const auto word=[](auto& p,unsigned address,unsigned value) {
            p.vram[address*2]=std::uint8_t(value);p.vram[address*2+1]=std::uint8_t(value>>8);
        };
        for(unsigned tile=0;tile<64;++tile) for(unsigned y=0;y<8;++y) {
            const unsigned ink=1+tile%15,at=0x2000+tile*32+y*2;
            source.vram[at]=ink&1?255:0;source.vram[at+1]=ink&2?255:0;
            source.vram[at+16]=ink&4?255:0;source.vram[at+17]=ink&8?255:0;
        }
        for(unsigned y=0;y<(layout==2?64U:32U);++y) for(unsigned x=0;x<32;++x)
            word(source,0x4000+(y/32)*1024+(y%32)*32+x,(y*7+x)%16);
        for(unsigned ink=1;ink<16;++ink) source.cgram[ink]=std::uint16_t(ink*1024+ink*32+ink);
        for(int bank:{-4,-1,1,4}) for(int phase:{3,int(height)-3}) {
            auto continuous=source,wrapped=source;
            for(unsigned i=0;i<32;++i) {
                const int offset=phase+bank*(int(i+1)-16);
                word(continuous,0x2fa0+i,0x4000|(unsigned(offset)&8191));
                word(wrapped,0x2fa0+i,0x4000|(unsigned(offset)&(height-1)));
            }
            const auto a=starfox::render::environment_raster_horizon(continuous,112,3);
            const auto b=starfox::render::environment_raster_horizon(wrapped,112,3);
            if(!a || !b || std::abs((*a)[0]-(*b)[0])>.001f || std::abs((*a)[1]-(*b)[1])>.001f)
                throw std::runtime_error("Enhanced horizon disagrees across equivalent atlas phases");
            for(unsigned width:{256U,400U,853U}) {
                const auto expected=draw(continuous,width),actual=draw(wrapped,width);
                if(actual!=expected) {
                    std::size_t first=0;while(first<actual.size() && actual[first]==expected[first]) ++first;
                    throw std::runtime_error("Wrapped BG2 roll changed pixels: atlas="
                        +std::to_string(height)+" width="+std::to_string(width)+" bank="+std::to_string(bank)
                        +" phase="+std::to_string(phase)+" pixel="+std::to_string(first)
                        +" expected="+std::to_string(expected[first])+" actual="+std::to_string(actual[first]));
                }
            }
        }
    }
}
