#pragma once
#include "starfox/render/background_renderer.hpp"
#include "starfox/render/tunnel_margin.hpp"
#include <stdexcept>

inline starfox::simulation::SnesPpuState tunnel_margin_fixture(int shift=0) {
    starfox::simulation::SnesPpuState p;
    p.main_screen=2;p.background_mode=1;p.tunnel_scene=true;
    p.bg2_screen_size=3;p.bg2_screen_base=0x4000;p.bg2_character_base=0x1000;
    p.bg2_horizontal_offsets_enabled=p.bg2_scanline_scroll_enabled=true;
    for(unsigned ink=1;ink<16;++ink) for(unsigned y=0;y<8;++y) {
        const unsigned b=0x2000+ink*32+y*2;
        p.vram[b]=ink&1?255:0;p.vram[b+1]=ink&2?255:0;
        p.vram[b+16]=ink&4?255:0;p.vram[b+17]=ink&8?255:0;
        p.cgram[ink]=std::uint16_t(ink*1024+ink*32+ink);
    }
    for(int ty=0;ty<64;++ty) for(int tx=0;tx<64;++tx) {
        const int page=ty>=32,local=ty*8-(page?280:24)+4;
        const double dy=std::abs(local-112.0),dx=std::abs(tx*8+4-256.0);
        const unsigned ink=dx>=2*dy?1:dx>=1.65*dy?13:unsigned(page?3:2);
        const unsigned at=(0x4000+((ty/32)*2+tx/32)*1024+(ty%32)*32+tx%32)*2;
        p.vram[at]=std::uint8_t(ink);p.vram[at+1]=0;
    }
    for(int y=0;y<224;++y) {
        p.bg2_horizontal_offsets[y]=std::int16_t(128+shift*std::abs(y-112)/112);
        p.bg2_scanline_scroll_y[y]=std::int16_t((y/8)%2?280:24);
    }
    return p;
}
template<class Draw> void check_tunnel_margins(Draw draw) {
    using namespace starfox::render;
    for(int shift:{-64,0,64}) {
        const auto p=tunnel_margin_fixture(shift);
        if(tunnel_margin(p).data[0]!=15) throw std::runtime_error("Tunnel source-art fit missed a ceiling/floor page");
        for(unsigned width:{256U,400U,853U}) for(unsigned scale:{1U,2U}) {
            const auto baseline=draw(p,width,scale,false),extended=draw(p,width,scale,true);
            const unsigned origin=width/2-128;
            unsigned changed=0;
            for(unsigned y=0;y<224*scale;++y) for(unsigned x=0;x<width*scale;++x) {
                if(baseline[y*width*scale+x]==extended[y*width*scale+x]) continue;
                if(x>=origin*scale && x<(origin+256)*scale)
                    throw std::runtime_error("Tunnel diagonal continuation changed a native viewport pixel");
                ++changed;
            }
            if(width>256 && changed<1000*scale*scale) throw std::runtime_error("Tunnel margins still stretch flat edge bars");
            if(width==256 && changed) throw std::runtime_error("Native tunnel view changed");
        }
    }
    auto asymmetric=tunnel_margin_fixture();
    for(unsigned i=0;i<1024;++i) asymmetric.vram[0x8000+i*2]=14;
    if(tunnel_margin(asymmetric).data[0]) throw std::runtime_error("Asymmetric tunnel was forced into a symmetric corridor");
    auto offsets=tunnel_margin_fixture();
    offsets.bg2_vertical_offsets_enabled=true;
    if(tunnel_margin(offsets).data[0]!=15)
        throw std::runtime_error("Inactive Mode 2 vertical-offset latch suppressed a Mode 1 corridor");
    offsets.background_mode=2;
    if(tunnel_margin(offsets).data[0])
        throw std::runtime_error("Active Mode 2 vertical offsets were replaced with a radial corridor");
    // Static Mode 2 rooms (Space Armada's nucleus) have authored walls beyond
    // the native viewport, not alternating checker pages. Verify against the
    // untouched atlas decoder wherever that artwork exists; do not just compare
    // two extensions of the same edge. Include banked columns and whole-atlas
    // scroll translations, with native columns remaining byte-exact.
    for(bool banked:{false,true}) for(int phase:{-512,0,512}) {
        auto room=tunnel_margin_fixture();
        room.background_mode=2;room.bg2_scanline_scroll_enabled=false;
        room.bg2_scroll_y=24;room.bg2_horizontal_offsets.fill(std::int16_t(128+phase));
        room.bg2_vertical_offsets_enabled=banked;
        for(unsigned column=0;column<32;++column) {
            const auto value=unsigned(24+int(column)/4-4)|0x4000U;
            room.vram[(0x2fa0+column)*2]=std::uint8_t(value);
            room.vram[(0x2fa0+column)*2+1]=std::uint8_t(value>>8);
        }
        auto atlas=room;atlas.tunnel_scene=false;
        for(unsigned width:{256U,400U,853U}) for(unsigned scale:{1U,2U}) {
            const auto baseline=draw(room,width,scale,false),extended=draw(room,width,scale,true);
            const auto original=draw(atlas,width,scale,false);
            const int origin=int(width)/2-128;
            unsigned changed=0,compared=0;
            for(unsigned y=0;y<224*scale;++y) for(unsigned x=0;x<width*scale;++x) {
                const int logical=int(x/scale)-origin;
                if(logical>=0 && logical<256) {
                    if(baseline[y*width*scale+x]!=extended[y*width*scale+x])
                        throw std::runtime_error("Static room extension changed a native pixel");
                } else {
                    changed+=baseline[y*width*scale+x]!=extended[y*width*scale+x];
                    if(logical+128>=0 && logical+128<512 && y>=40*scale && y<184*scale) {
                        if((unsigned(extended[y*width*scale+x])&255U)!=(unsigned(original[y*width*scale+x])&255U))
                            throw std::runtime_error("Static room margin did not use the authored atlas");
                        ++compared;
                    }
                }
            }
            if(width>256 && (changed<1000 || compared<1000))
                throw std::runtime_error("Static room margins still hold viewport-edge bars");
        }
    }
}
