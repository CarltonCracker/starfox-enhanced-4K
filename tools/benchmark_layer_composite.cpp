#include "starfox/render/framebuffer.hpp"
#include <algorithm>
#include <chrono>
#include <cstdint>
#include <iostream>
#include <vector>

// Bounded, asset-free microbenchmark. Compare the printed digest as well as
// timing when changing the compositor; this does not measure gameplay FPS.
int main() {
    using namespace starfox::render;
    for (const auto scale : {1U, 2U, 4U}) for (const bool metadata : {false, true}) {
        Framebuffer source(400, 224, scale), destination(400, 224, scale);
        source.enable_layer_tags(metadata);
        destination.enable_layer_tags(metadata);
        if (metadata) destination.begin_write_coverage();
        for (std::size_t i = 0; i < source.pixels().size(); ++i) {
            source.pixels()[i] = i % 7 < 3 ? 0 : std::uint8_t(i * 73 + 1);
            destination.pixels()[i] = std::uint8_t(i * 31 + 13);
            if (metadata) source.layer_tags()[i] = std::uint8_t(i % 6);
        }
        std::vector<double> samples;
        for (unsigned run = 0; run < 35; ++run) {
            const auto start = std::chrono::steady_clock::now();
            for (unsigned repeat = 0; repeat < 20; ++repeat)
                composite_transparent_layer(source, destination, {});
            const auto end = std::chrono::steady_clock::now();
            if (run >= 5) samples.push_back(
                std::chrono::duration<double, std::micro>(end - start).count() / 20);
        }
        std::sort(samples.begin(), samples.end());
        std::uint64_t hash = 14695981039346656037ULL;
        const auto digest = [&](auto bytes) {
            for (const auto value : bytes) { hash ^= value; hash *= 1099511628211ULL; }
        };
        digest(destination.pixels());
        digest(destination.layer_tags());
        digest(destination.write_coverage());
        std::cout << scale << "x metadata=" << metadata << " median_us="
            << samples[15] << " p95_us=" << samples[28] << " digest=" << hash << '\n';
    }
}
