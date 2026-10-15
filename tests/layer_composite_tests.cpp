#include "starfox/render/framebuffer.hpp"
#include <algorithm>
#include <cstdint>
#include <iostream>
#include <stdexcept>

namespace {
using namespace starfox::render;

void check_equal(const Framebuffer& a, const Framebuffer& b) {
    if (a.pixels() != b.pixels() || a.layer_tags() != b.layer_tags()
        || !std::ranges::equal(a.write_coverage(), b.write_coverage())
        || !std::ranges::equal(a.dither_pairs(), b.dither_pairs()))
        throw std::runtime_error("Bulk composite differs from scalar 1-pixel mosaic reference");
}

void seed(Framebuffer& frame, unsigned flags, bool source) {
    frame.enable_layer_tags((flags & (source ? 1U : 2U)) != 0);
    if (!source && (flags & 4U)) frame.begin_write_coverage();
    if (flags & (source ? 8U : 16U)) frame.enable_dither_pairs(true);
    for (std::size_t i = 0; i < frame.pixels().size(); ++i) {
        if (!source && i % 7 == 0)
            frame.set_stored(unsigned(i % frame.stored_width()),
                unsigned(i / frame.stored_width()), 1);
        frame.pixels()[i] = i % 3 == 0 ? 0 : std::uint8_t(i * 73 + (source ? 11 : 37));
        if (frame.layer_tags_enabled()) frame.layer_tags()[i] = std::uint8_t((i + (source ? 0 : 3)) % 6);
        if (i % 5 == 0 && !frame.dither_pairs().empty())
            frame.set_dither_alternate(i, std::uint8_t(i * 19));
    }
}
}

int main() try {
    unsigned cases = 0;
    for (unsigned width : {1U, 15U, 17U, 65U}) for (unsigned scale : {1U, 2U, 4U})
    for (unsigned flags = 0; flags < 32; ++flags) for (int offset : {-3, 0, 5})
    for (bool clipped : {false, true}) {
        Framebuffer source(width, 9, scale), actual(width + 6, 13, scale);
        seed(source, flags, true); seed(actual, flags, false);
        auto expected = actual;
        LayerCompositeSettings settings;
        settings.offset_x = offset; settings.offset_y = -offset;
        if (clipped) {
            settings.clip_left = 2; settings.clip_top = 1;
            settings.clip_right = int(width) + 1; settings.clip_bottom = 8;
        }
        composite_transparent_layer(source, actual, settings);
        // A one-pixel mosaic selects the independent per-cell path without
        // changing sampling, clipping or source material provenance.
        settings.mosaic = 1; settings.mosaic_layer_mask = 1;
        composite_transparent_layer(source, expected, settings);
        check_equal(actual, expected); ++cases;
    }
    // In-place overlapping copies retain the existing sequential semantics;
    // the independent-buffer vector loop must never handle aliasing.
    for (int offset : {-2, 0, 2}) {
        Framebuffer actual(65, 13); seed(actual, 7, false);
        auto expected = actual;
        LayerCompositeSettings settings; settings.offset_x = offset;
        composite_transparent_layer(actual, actual, settings);
        settings.mosaic = 1; settings.mosaic_layer_mask = 1;
        composite_transparent_layer(expected, expected, settings);
        check_equal(actual, expected); ++cases;
    }
    std::cout << cases << " layer composite cases passed (pixels, tags, coverage, dither)\n";
    return 0;
} catch (const std::exception& error) {
    std::cerr << error.what() << '\n'; return 1;
}
