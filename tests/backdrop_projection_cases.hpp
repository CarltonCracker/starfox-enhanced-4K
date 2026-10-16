#pragma once
#include "starfox/render/environment_effects.hpp"
#include <iostream>
#include <stdexcept>

// Every source pixel here is explicitly SKY, including the part that moves
// across the fitted horizon during a bank/scroll. A packed atlas must never
// mistake its panorama coordinates for a moon tag, nor retain native wedges.
template<class Draw> void check_packed_backdrop_extents(Draw draw) {
    using namespace starfox::render;
    unsigned cases=0;
    BackdropImage atlas{64,32,std::vector<std::uint32_t>(2048,0xff2050a0)};
    std::fill(atlas.pixels.begin()+1024,atlas.pixels.end(),0xffe0e0e0);
    for(unsigned mode:{6U,7U,8U}) for(unsigned width:{256U,400U,853U})
        for(float bank:{-4.f,0.f,4.f}) for(float horizon:{-700.f,100.f,700.f}) {
        Framebuffer frame(width,224);frame.enable_layer_tags(true);
        frame.pixels().assign(std::size_t(width)*224,1);
        frame.layer_tags().assign(frame.pixels().size(),unsigned(PixelLayer::background));
        std::vector<std::uint8_t> input(frame.pixels().size()*4,255);
        EnvironmentEffects effects;effects.modes[2]=1;effects.classes[1]=6;
        effects.motion[0]=horizon;effects.plane={bank,0,511.75f,1};effects.backdrop=&atlas;
        effects.backdrop_projection={1/512.f,1/224.f,.633f,float(mode)};
        effects.backdrop_keep={{{-24,60,8,8},{32,110,6,6}}};
        effects.backdrop_ramp[1]=effects.backdrop_ramp[2]=0xffffff;
        const auto output=draw(frame,input,effects);
        for(unsigned y=0;y<224;++y) for(unsigned x=0;x<width;++x) {
            const float sx=float(x)+.5f-float(width)*.5f,sy=float(y)+.5f;
            bool moon=false;
            if(mode==8) moon=city_moon_coordinates(sx,sy,effects.backdrop_keep[1]).has_value();
            else for(const auto& body:effects.backdrop_keep) {
                const float dx=(sx-body[0])/body[2],dy=(sy-body[1])/body[3];
                moon=moon || dx*dx+dy*dy<=1;
            }
            // Bilinear filtering may blend right at the packed texture edge,
            // but only an actual celestial footprint may reach that section.
            const auto i=(std::size_t(y)*width+x)*4;
            if(!moon && (output[i]!=160 || output[i+1]!=80 || output[i+2]!=32 || output[i+3]!=255))
                throw std::runtime_error("Packed panorama repeated a moon or left a native sky wedge: mode="
                    +std::to_string(mode)+" width="+std::to_string(width)+" bank="+std::to_string(bank)
                    +" horizon="+std::to_string(horizon)+" pixel="+std::to_string(x)+","+std::to_string(y));
        }
        ++cases;
    }
    std::cout<<"Packed background extents: "<<cases<<" atlas, bank, scroll, pitch and aspect cases passed\n";
}

template<class Draw> void check_orbital_surface_extents(Draw draw) {
    using namespace starfox::render;
    BackdropImage atlas{64,48,std::vector<std::uint32_t>(64*48,0xff1e140a)};
    for(unsigned y=16;y<32;++y) for(unsigned x=0;x<64;++x)
        atlas.pixels[y*64+x]=0xff000000U|(32+x*3)|((48+(y-16)*9)<<8)|((96+(x+y)%5)<<16);
    std::fill(atlas.pixels.begin()+64*32,atlas.pixels.end(),0xffdcdcdc);
    unsigned cases=0;
    for(unsigned width:{256U,400U,853U}) for(float bank:{-1.f,0.f,1.f}) for(float scroll:{0.f,511.75f}) {
        Framebuffer frame(width,224);frame.enable_layer_tags(true);
        frame.pixels().assign(std::size_t(width)*224,1);
        frame.layer_tags().assign(frame.pixels().size(),unsigned(PixelLayer::background));
        frame.pixels()[0]=7; // Authored star/planet protection is independent.
        std::vector<std::uint8_t> input(frame.pixels().size()*4,255);
        EnvironmentEffects effects;effects.modes[2]=1;effects.classes[1]=6;effects.classes[7]=7;
        effects.motion[0]=20;effects.plane={bank,0,scroll,1};effects.backdrop=&atlas;
        effects.backdrop_projection={1/512.f,1/160.f,.54f,10};
        effects.backdrop_keep[0]={-40,50,8,8};effects.backdrop_ramp[1]=effects.backdrop_ramp[2]=0xffffff;
        BackdropImage::fit_canvas(effects.backdrop_projection,effects.plane[2],width,224);
        const auto output=draw(frame,input,effects);
        if(output[0]!=255 || output[1]!=255 || output[2]!=255)
            throw std::runtime_error("Orbital surface overwrote a protected native subject");
        const auto a=(std::size_t(160)*width+width/2)*4,b=(std::size_t(220)*width+width/2)*4;
        unsigned difference=0;
        for(unsigned c=0;c<3;++c) difference+=unsigned(std::abs(int(output[a+c])-int(output[b+c])));
        if(difference<3) throw std::runtime_error("Downward orbital view still extrudes a single texture row");
        if(bank==0) {
            const auto left=(std::size_t(200)*width+width/2-100)*4,right=left+200*4;
            if(int(output[right])-int(output[left])<3)
                throw std::runtime_error("Orbital disk folded left/right detail into a radial texture fan");
        }
        for(float y:{-10000.f,0.f,224.f,10000.f}) for(float x:{-853.f,0.f,853.f}) {
            const auto uv=BackdropImage::coordinates(x,y,20,bank,effects.plane[2],effects.backdrop_projection,effects.backdrop_keep);
            if(!std::isfinite(uv[0]) || !std::isfinite(uv[1]) || uv[1]>=2)
                throw std::runtime_error("Orbital continuation selected an offscreen moon tag");
        }
        if(BackdropImage::coordinates(-40,50,20,bank,effects.plane[2],effects.backdrop_projection,effects.backdrop_keep)[1]!=2.5f)
            throw std::runtime_error("Orbital continuation lost the unique moon footprint");
        ++cases;
    }
    std::cout<<"Orbital surface continuation: "<<cases<<" downward/banked/wide sky, surface and moon cases passed\n";
}
