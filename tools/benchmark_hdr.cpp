#include "starfox/render/hdr_effect.hpp"
#include <algorithm>
#include <chrono>
#include <iostream>
#include <stdexcept>

namespace {
// Pre-optimization reference: build the curve on each serial invocation.
void legacy(const starfox::render::Framebuffer& frame,std::vector<std::uint8_t>& rgba,std::uint8_t level) {
    std::array<std::uint8_t,256> curve{};
    const auto exposure=std::array<double,4>{1,1.15,1.35,1.65}[level];
    const auto contrast=std::array<double,4>{0,.2,.4,.6}[level];
    for (unsigned i=0;i<256;++i) {
        const auto input=i/255.0;
        const auto lifted=input*exposure/(1+input*(exposure-1));
        const auto shaped=lifted*lifted*(3-2*lifted);
        curve[i]=static_cast<std::uint8_t>(std::lround(255*(lifted*(1-contrast)+shaped*contrast)));
    }
    const auto& tags=frame.layer_tags();
    for (std::size_t i=0;i<frame.pixels().size();++i) {
        if (tags[i]==static_cast<std::uint8_t>(starfox::render::PixelLayer::two_d)) continue;
        for (unsigned c=0;c<3;++c) rgba[i*4+c]=curve[rgba[i*4+c]];
    }
}
}

int main() try {
    using namespace starfox::render;
    RowWorkers workers;workers.set_worker_count(4);
    for (const auto layout:std::array<std::array<unsigned,3>,5>{{
             {256,224,1},{400,224,1},{400,224,2},{400,224,4},{796,224,4}}}) {
        Framebuffer frame{layout[0],layout[1],layout[2]};
        frame.enable_layer_tags(true);
        // Include HUD holes and every world tag, not only a uniform mask.
        for (unsigned y=0;y<frame.stored_height();++y)
            for (unsigned x=0;x<frame.stored_width();++x)
                frame.set_stored(x,y,1,static_cast<PixelLayer>((x/13+y/7)%6));
        std::vector<std::uint8_t> original(frame.pixels().size()*4U);
        for (std::size_t i=0;i<original.size();++i) original[i]=std::uint8_t(i*73U+i/257U);
        auto reference=original;legacy(frame,reference,3);
        std::vector<std::uint8_t> rgba;
        std::array<std::vector<double>,2> samples;
        for (unsigned run=0;run<71;++run) for (unsigned slot=0;slot<2;++slot) {
            const auto path=(slot+run)%2;
            rgba=original;
            const auto start=std::chrono::steady_clock::now();
            if (path==0) apply_hdr_effect(frame,rgba,3);
            else apply_hdr_effect(frame,rgba,3,&workers);
            const auto end=std::chrono::steady_clock::now();
            if (rgba!=reference) throw std::runtime_error("Brightness/contrast benchmark changed pixels");
            if (run>=10) samples[path].push_back(std::chrono::duration<double,std::micro>(end-start).count());
        }
        for (auto& sample:samples) std::sort(sample.begin(),sample.end());
        std::uint64_t hash=14695981039346656037ULL;
        for (auto byte:rgba) {hash^=byte;hash*=1099511628211ULL;}
        std::cout<<layout[0]<<"x"<<layout[1]<<"@"<<layout[2]
            <<" serial_us="<<samples[0][30]<<" pooled4_us="<<samples[1][30]
            <<" speedup="<<samples[0][30]/samples[1][30]
            <<" pooled4_p95_us="<<samples[1][57]<<" hash="<<hash<<'\n';
    }
    return 0;
} catch (const std::exception& error) {
    std::cerr<<error.what()<<'\n';return 1;
}
