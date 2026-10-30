#pragma once
#include "starfox/simulation/snes_ppu.hpp"
#include <algorithm>
#include <cmath>
#include <stdexcept>

// Independent atlas: stars above row 376, a striped planet through row 495,
// then stars again. The old periodic sampler exposed those lower stars as
// rectangular gaps while banking. Never turn the sky itself into terrain.
template<class Draw> void check_background_surface_cases(Draw draw) {
    starfox::simulation::SnesPpuState p;
    p.background_mode=2;p.main_screen=2;p.bg2_screen_base=0x7000;
    p.bg2_character_base=0x5000;p.bg2_screen_size=3;
    p.bg2_vertical_offsets_enabled=true;
    const auto word=[&](unsigned at,unsigned value) {
        p.vram[at*2]=std::uint8_t(value);p.vram[at*2+1]=std::uint8_t(value>>8);
    };
    for(unsigned ink:{1U,2U,3U}) for(unsigned y=0;y<8;++y) {
        const auto at=0xa000+ink*32+y*2;
        p.vram[at]=(ink&1)?255:0;p.vram[at+1]=(ink&2)?255:0;
    }
    p.cgram[1]=0x7fff;p.cgram[2]=0x03e0;p.cgram[3]=0x0210;
    for(const auto bounds:{std::array<unsigned,2>{376,496},std::array<unsigned,2>{416,480}}) {
    for(unsigned y=0;y<64;++y) for(unsigned x=0;x<64;++x) {
        const auto entry=((y/32)*2+x/32)*1024+(y%32)*32+x%32;
        word(0x7000+entry,y>=bounds[0]/8 && y<bounds[1]/8?2+(x%2):1);
    }
    for(unsigned width:{256U,400U,800U}) for(int bank:{-4,-2,0,2,4})
        for(int offset:{180,360,480}) {
            for(int i=0;i<32;++i) word(0x2fa0+i,
                0x4000|((offset+bank*(i+1-16))&8191));
            const auto actual=draw(p,width,true,bounds),control=draw(p,width,false,bounds);
            const int origin=int(width)/2-128;
            for(unsigned y=0;y<224;++y) for(unsigned x=0;x<width;++x) {
                if(width==256) {
                    if(actual[y*width+x]!=control[y*width+x])
                        throw std::runtime_error("Orbital extension changed the native 4:3 raster");
                    continue;
                }
                // All fixtures are exactly linear (whole-pixel centre phase).
                const int logical=int(x)-origin;
                const double scroll=offset+bank*(double(logical)/8-16);
                const int rounded=int(std::floor(scroll+.5));
                const int source_y=std::clamp(int(y)+rounded,0,int(bounds[1])-1);
                const unsigned source_x=unsigned(logical)&511;
                const unsigned expected=source_y>=int(bounds[0])?2+((source_x/8)%2):1;
                if(actual[y*width+x]!=expected)
                    throw std::runtime_error("Banked planet wrapped to stars or clipped its surface");
            }
        }
    }
}
