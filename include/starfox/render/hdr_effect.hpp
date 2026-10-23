#pragma once
#include "starfox/render/framebuffer.hpp"
#include "starfox/render/row_workers.hpp"
#include <array>
#include <cmath>

namespace starfox::render {
// SDR brightness/contrast styling only: does not request an HDR swapchain,
// change display metadata, or claim additional physical display headroom.
inline void apply_hdr_effect(const Framebuffer& frame,
    std::vector<std::uint8_t>& rgba, std::uint8_t intensity) {
    if (intensity==0 || intensity>3 || !frame.layer_tags_enabled()
        || rgba.size()!=frame.pixels().size()*4U) return;
    std::array<std::uint8_t,256> curve{};
    const auto exposure=std::array<double,4>{1,1.15,1.35,1.65}[intensity];
    const auto contrast=std::array<double,4>{0,.2,.4,.6}[intensity];
    for (unsigned i=0;i<curve.size();++i) {
        const auto input=i/255.0;
        const auto lifted=input*exposure/(1+input*(exposure-1));
        const auto shaped=lifted*lifted*(3-2*lifted);
        curve[i]=static_cast<std::uint8_t>(std::lround(
            255*(lifted*(1-contrast)+shaped*contrast)));
    }
    const auto& tags=frame.layer_tags();
    for (std::size_t pixel=0;pixel<frame.pixels().size();++pixel) {
        if (tags[pixel]==static_cast<std::uint8_t>(PixelLayer::two_d)) continue;
        for (unsigned channel=0;channel<3;++channel)
            rgba[pixel*4+channel]=curve[rgba[pixel*4+channel]];
    }
}

inline void apply_hdr_effect(const Framebuffer& frame,
    std::vector<std::uint8_t>& rgba, std::uint8_t intensity, RowWorkers* workers) {
    // Keep the established scalar loop for small frames and serial callers.
    // Wrapping that loop in a captured job costs more than it saves at 1x.
    if (!workers || workers->worker_count()==1 || frame.pixels().size()<262144 || frame.stored_height()<32) {
        apply_hdr_effect(frame,rgba,intensity);return;
    }
    if (intensity==0 || intensity>3 || !frame.layer_tags_enabled()
        || rgba.size()!=frame.pixels().size()*4U) return;
    // The three SDR curves depend only on the level, not the image/palette.
    // Preserve the old binary64 arithmetic and lround exactly, but construct
    // each curve once. Thread-safe static initialization also covers callers
    // outside the ordinary presentation thread.
    static const auto curves=[] {
        std::array<std::array<std::uint8_t,256>,3> result{};
        for (unsigned level=1;level<=3;++level) {
            const auto exposure=std::array<double,4>{1,1.15,1.35,1.65}[level];
            const auto contrast=std::array<double,4>{0,.2,.4,.6}[level];
            for (unsigned i=0;i<256;++i) {
                const auto input=i/255.0;
                const auto lifted=input*exposure/(1+input*(exposure-1));
                const auto shaped=lifted*lifted*(3-2*lifted);
                result[level-1][i]=static_cast<std::uint8_t>(std::lround(
                    255*(lifted*(1-contrast)+shaped*contrast)));
            }
        }
        return result;
    }();
    // A small local copy makes the lookup table independent of every output
    // pointer; no output byte can alias it while the compiler schedules RGB.
    const auto curve=curves[intensity-1];
    const auto* tags=frame.layer_tags().data();
    auto* pixels=rgba.data();
    const auto stride=std::size_t(frame.stored_width());
    const auto apply_rows=[&](std::uint32_t first,std::uint32_t last) {
        const auto end=std::size_t(last)*stride;
        for (std::size_t pixel=std::size_t(first)*stride;pixel<end;++pixel) {
            if (tags[pixel]==static_cast<std::uint8_t>(PixelLayer::two_d)) continue;
            const auto at=pixel*4;
            const auto red=curve[pixels[at]],green=curve[pixels[at+1]],blue=curve[pixels[at+2]];
            pixels[at]=red;pixels[at+1]=green;pixels[at+2]=blue;
        }
    };
    // Disjoint rows, immutable tags/curve, no alpha writes, scratch images,
    // extra threads, callback allocation or simulation work.
    workers->parallel_rows(frame.stored_height(),std::cref(apply_rows));
}
} // namespace starfox::render
