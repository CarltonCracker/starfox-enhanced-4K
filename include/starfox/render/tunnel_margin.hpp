#pragma once
#include "starfox/simulation/snes_ppu.hpp"
#include <array>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <limits>
#include <optional>
#include <vector>

namespace starfox::render {
// The same source-art zone fitting used by the VR corridor: continue the
// authored diagonals, rather than stretching the x=0/255 texel into a bar.
// This supplies only added flat-view columns; native pixels are never moved.
struct TunnelMargin {
    // Shared CPU/GPU layout: valid-half mask, four zone counts, four eight-
    // word zone tables (float slope + indexed colour), then two centre rows.
    static constexpr unsigned words = 40 + 2 * 224;
    std::array<std::uint32_t, words> data{};

    std::optional<std::uint8_t> sample(
        int x, int y, int row_x, int row_y) const noexcept {
        if (y < 0 || y >= 224 || ((row_y & 511) != 24 && (row_y & 511) != 280)) return {};
        const unsigned page = (row_y & 511) == 280;
        const unsigned part = page * 2 + (y >= 112);
        if (!(data[0] & (1U << part))) return {};
        auto colour = data[40 + page * 224 + unsigned(y)];
        const float dx = std::abs(float(x + row_x) - 255.5f);
        const float dy = std::abs(float(y) + .5f - (y < 112 ? 112.5f : 111.5f));
        for (unsigned z = 0; z < data[1 + part]; ++z) {
            float slope;
            std::memcpy(&slope, &data[8 + part * 8 + z * 2], sizeof(slope));
            if (dx >= slope * dy) colour = data[9 + part * 8 + z * 2];
        }
        return std::uint8_t(colour);
    }
};
inline TunnelMargin fit_tunnel_margin(const simulation::SnesPpuState& p) {
    TunnelMargin result;
    if(!p.tunnel_scene || !(p.main_screen&2) || !p.bg2_scanline_scroll_enabled
        || (p.background_mode==2 && p.bg2_vertical_offsets_enabled)
        || (p.mosaic&2)) return result;
    unsigned darkest=std::numeric_limits<unsigned>::max(),black=0;
    for(unsigned i=0;i<256;++i) {
        const auto c=p.cgram[i];const unsigned l=77*(c&31)+150*((c>>5)&31)+29*((c>>10)&31);
        if(l<darkest) {darkest=l;black=i;}
    }
    const unsigned edge=p.bg2_tile_size_16?16:8;
    const unsigned width=((p.bg2_screen_size&1)?64:32)*edge;
    const unsigned height=((p.bg2_screen_size&2)?64:32)*edge;
    const auto byte=[&](unsigned a){return unsigned(p.vram[a&65535]);};
    const auto art=[&](unsigned page,int x,int row) {
        const unsigned px=unsigned(x+128)&(width-1),py=unsigned(row+(page?280:24))&(height-1);
        const unsigned tx=px/edge,ty=py/edge;
        const unsigned at=(p.bg2_screen_base+((ty/32)*(width/edge/32)+tx/32)*1024+(ty%32)*32+tx%32)*2;
        const unsigned tile=byte(at)|(byte(at+1)<<8);
        const unsigned ix=(tile&16384)?edge-1-px%edge:px%edge,iy=(tile&32768)?edge-1-py%edge:py%edge;
        const unsigned chr=((tile&1023)+(ix/8)+(iy/8)*16)&1023;
        const unsigned base=p.bg2_character_base*2+chr*32+(iy%8)*2,mask=128>>(ix%8);
        const unsigned ink=((byte(base)&mask)?1:0)|((byte(base+1)&mask)?2:0)
            |((byte(base+16)&mask)?4:0)|((byte(base+17)&mask)?8:0);
        return ink?((tile>>10)&7)*16+ink:black;
    };
    unsigned samples=0,different=0;
    for(unsigned page=0;page<2;++page) for(int y=0;y<224;y+=4) for(int k=4;k<124;k+=8) {
        ++samples;different+=art(page,127-k,y)!=art(page,128+k,y);
    }
    // Decorated/asymmetric tunnels retain their native sampling, not a guessed
    // symmetric cross-section. This is the VR art-probe's existing threshold.
    if(different*8>samples) return result;
    struct Transition {unsigned colour;int column;};
    for(unsigned page=0;page<2;++page) for(unsigned half=0;half<2;++half) {
        std::array<std::vector<Transition>,224> rows;
        const unsigned begin=half?112:0,end=half?224:112,part=page*2+half;
        unsigned canonical=begin;
        for(unsigned y=begin;y<end;++y) {
            auto previous=art(page,128,int(y));result.data[40+page*224+y]=previous;
            for(int x=127;x>=0;--x) {
                const auto c=art(page,x,int(y));
                if(c!=previous) rows[y].push_back({c,x+1});
                previous=c;
            }
            if(rows[y].size()>rows[canonical].size()) canonical=y;
        }
        const auto& zones=rows[canonical];
        for(unsigned z=0;z<zones.size() && z<3;++z) {
            double xy=0,xx=0;unsigned count=0;
            for(unsigned y=begin;y<end;++y) for(const auto& t:rows[y]) if(t.colour==zones[z].colour) {
                const double dy=std::abs(double(y)+.5-(half?111.5:112.5));
                if(dy>4) {xy+=dy*(128-t.column);xx+=dy*dy;++count;}
                break;
            }
            if(count<3 || !xx) break;
            const float slope=float(xy/xx);
            if(!std::isfinite(slope) || slope<=0) break;
            std::memcpy(&result.data[8+part*8+z*2],&slope,4);
            result.data[9+part*8+z*2]=zones[z].colour;
            result.data[1+part]=z+1;result.data[0]|=1U<<part;
        }
    }
    return result;
}
inline const TunnelMargin& tunnel_margin(const simulation::SnesPpuState& p) {
    // Source textures are normally static. Keep one exact, thread-local cache,
    // including palette fallback/format, never a borrowed PPU or frame pointer.
    struct Cache {
        bool valid{};std::array<std::uint8_t,65536> vram{};
        std::array<std::uint16_t,256> cgram{};
        std::array<unsigned,3> format{};TunnelMargin profile;
    };
    thread_local Cache cache;
    const std::array<unsigned,3> format{p.bg2_screen_base,p.bg2_character_base,
        unsigned(p.bg2_screen_size)|(unsigned(p.bg2_tile_size_16)<<8)
            |(unsigned(p.tunnel_scene)<<9)|(unsigned(p.main_screen&2)<<10)
            |(unsigned(p.bg2_scanline_scroll_enabled)<<12)|(unsigned(p.bg2_vertical_offsets_enabled)<<13)
            |(unsigned(p.mosaic&2)<<14)|(unsigned(p.background_mode)<<16)};
    if(!cache.valid || cache.format!=format || cache.vram!=p.vram || cache.cgram!=p.cgram) {
        cache.profile=fit_tunnel_margin(p);cache.vram=p.vram;cache.cgram=p.cgram;
        cache.format=format;cache.valid=true;
    }
    return cache.profile;
}
}
