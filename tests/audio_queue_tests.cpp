#include "starfox/app/audio_queue.hpp"

#include <SDL3/SDL.h>

#include <algorithm>
#include <array>
#include <cstdlib>
#include <iostream>
#include <string_view>
#include <vector>

namespace {
void require(bool condition, const char* message) {
    if (!condition) {
        std::cerr << message << ": " << SDL_GetError() << '\n';
        std::exit(1);
    }
}
} // namespace

int main() {
    using namespace starfox::app;
    constexpr auto android_policy = audio_playback_policy(AudioPlaybackPlatform::android);
    constexpr auto switch_policy = audio_playback_policy(AudioPlaybackPlatform::switch_console);
    constexpr auto desktop_policy = audio_playback_policy(AudioPlaybackPlatform::desktop);
    static_assert(android_policy.startup_ms == 24 && android_policy.device_sample_frames == 256
        && android_policy.limit_ms == 150 && android_policy.game_stream_role);
    static_assert(switch_policy.startup_ms == 64 && switch_policy.device_sample_frames == 512
        && switch_policy.limit_ms == 150 && !switch_policy.game_stream_role);
    static_assert(desktop_policy.startup_ms == 64 && desktop_policy.device_sample_frames == 0
        && desktop_policy.limit_ms == 150 && !desktop_policy.game_stream_role);
    // Android defaults must reach the SDL device, not just an unused policy.
    SDL_ResetHint(SDL_HINT_AUDIO_DEVICE_SAMPLE_FRAMES);
    SDL_ResetHint(SDL_HINT_AUDIO_DEVICE_STREAM_ROLE);
    configure_realtime_audio_device(android_policy);
    require(std::string_view(SDL_GetHint(SDL_HINT_AUDIO_DEVICE_SAMPLE_FRAMES)) == "256",
        "Android callback size was not configured");
    require(std::string_view(SDL_GetHint(SDL_HINT_AUDIO_DEVICE_STREAM_ROLE)) == "Game",
        "Android playback did not use the game stream role");
    require(SDL_Init(SDL_INIT_AUDIO), "audio initialization");
    constexpr SDL_AudioSpec source{SDL_AUDIO_S16, 2, 32'000};
    auto* device_stream = SDL_OpenAudioDeviceStream(SDL_AUDIO_DEVICE_DEFAULT_PLAYBACK,
        &source, nullptr, nullptr);
    require(device_stream != nullptr, "open device using Android defaults");
    SDL_AudioSpec actual_device{};
    int actual_frames{};
    require(SDL_GetAudioDeviceFormat(SDL_GetAudioStreamDevice(device_stream),
        &actual_device, &actual_frames) && actual_frames == 256,
        "SDL did not open the requested short-buffer device");
    SDL_AudioSpec actual_input{};
    require(SDL_GetAudioStreamFormat(device_stream, &actual_input, nullptr)
        && actual_input.freq == source.freq && actual_input.format == source.format
        && actual_input.channels == source.channels, "Device policy changed source PCM format");
    const auto description = realtime_audio_device_description(device_stream);
    require(description.find("callback-frames=256") != std::string::npos
        && description.find("driver=dummy") != std::string::npos,
        "Audio diagnostics did not report the negotiated device buffer");
    SDL_DestroyAudioStream(device_stream);
    // A user-selected buffer or stream role must never be silently overridden.
    require(SDL_SetHintWithPriority(SDL_HINT_AUDIO_DEVICE_SAMPLE_FRAMES, "1024", SDL_HINT_OVERRIDE)
        && SDL_SetHintWithPriority(SDL_HINT_AUDIO_DEVICE_STREAM_ROLE, "Media", SDL_HINT_OVERRIDE),
        "set explicit audio overrides");
    configure_realtime_audio_device(android_policy);
    require(std::string_view(SDL_GetHint(SDL_HINT_AUDIO_DEVICE_SAMPLE_FRAMES)) == "1024"
        && std::string_view(SDL_GetHint(SDL_HINT_AUDIO_DEVICE_STREAM_ROLE)) == "Media",
        "Android defaults replaced explicit audio overrides");
    constexpr SDL_AudioSpec switch_device{SDL_AUDIO_S16, 2, 48'000};
    constexpr std::uint32_t switch_limit = 3'200U; // exercise stricter legacy cap too
    constexpr int limit_bytes = switch_limit * 4U;
    auto* stream = SDL_CreateAudioStream(&source, &source);
    require(stream != nullptr, "create test stream");
    std::array<std::int16_t, 3'200> packet{}; // one 50 ms source tick
    std::array<std::int16_t, 3'200> output{};
    packet.fill(100);

    // Exactly two normal packets fit. A third catch-up packet evicts the
    // backlog and retains the most recent sound, rather than dropping it.
    for (int tick = 0; tick < 3; ++tick) {
        packet.fill(static_cast<std::int16_t>(100 + tick));
        require(starfox::app::queue_realtime_audio(stream, packet, switch_limit),
            "enqueue catch-up tick");
        require(SDL_GetAudioStreamQueued(stream)
                == static_cast<int>((tick == 1 ? 2U : 1U) * sizeof(packet)),
            "queue limit or normal headroom");
    }
    require(SDL_GetAudioStreamData(stream, output.data(), sizeof(output))
            == sizeof(output), "drain most recent tick");
    require(std::all_of(output.begin(), output.end(),
        [](auto value) { return value == 102; }), "stale audio survived reset");

    // Continuous real-time playback must preserve every packet unmodified.
    for (int tick = 0; tick < 100; ++tick) {
        packet.fill(static_cast<std::int16_t>(tick + 1));
        require(starfox::app::queue_realtime_audio(stream, packet, switch_limit),
            "enqueue regular tick");
        require(SDL_GetAudioStreamData(stream, output.data(), sizeof(output))
            == sizeof(output) && output == packet, "normal audio was lost");
    }
    SDL_DestroyAudioStream(stream);

    // Queue accounting must remain in 32 kHz INPUT units with a 48 kHz
    // Switch device. A blocked/slow consumer cannot grow the FIFO unbounded.
    stream = SDL_CreateAudioStream(&source, &switch_device);
    require(stream != nullptr, "create resampling stream");
    for (int tick = 0; tick < 1'000; ++tick) {
        require(starfox::app::queue_realtime_audio(stream, packet, switch_limit),
            "enqueue resampled backlog");
        const auto queued = SDL_GetAudioStreamQueued(stream);
        require(queued >= 0 && queued <= limit_bytes, "unbounded audio latency");
    }
    require(!starfox::app::queue_realtime_audio(stream,
        std::span<const std::int16_t>{packet}.first(3U), switch_limit),
        "accepted incomplete stereo frame");
    SDL_DestroyAudioStream(stream);
    // A render tick can be 16 ms late, then catch up on the following tick.
    // Reproduce the previous Switch underrun with 8 ms of headroom, and verify
    // the shared bounded policy survives the same jitter without losing PCM.
    const auto jitter_underruns = [&](unsigned startup_ms, unsigned limit_ms,
                                    unsigned output_hz = 32'000U) {
        const SDL_AudioSpec output_spec{SDL_AUDIO_S16, 2, static_cast<int>(output_hz)};
        auto* jitter = SDL_CreateAudioStream(&source, &output_spec);
        require(jitter != nullptr, "create jitter stream");
        std::vector<std::int16_t> prime(startup_ms * 32U * 2U, 100);
        require(SDL_PutAudioStreamData(jitter, prime.data(), static_cast<int>(prime.size()*2U)), "prime jitter stream");
        unsigned underruns = 0;
        for (unsigned tick = 0; tick < 100; ++tick) {
            packet.fill(100);
            require(starfox::app::queue_realtime_audio(jitter, packet, limit_ms * 32U), "queue jitter packet");
            std::vector<std::int16_t> consumed((tick % 2U ? 34U : 66U)
                * (output_hz / 1'000U) * 2U);
            const auto bytes = static_cast<int>(consumed.size() * 2U);
            const auto read = SDL_GetAudioStreamData(jitter, consumed.data(), bytes);
            require(read >= 0, "consume jitter packet");
            if (read != bytes) ++underruns;
            // Native-rate playback is bit-exact. Resampling can round a code
            // value and has an initial filter edge; check its steady tone too.
            const auto first = output_hz != 32'000U && tick == 0 ? std::min(read / 2, 128) : 0;
            require(std::all_of(consumed.begin()+first, consumed.begin()+read/2,
                [output_hz](auto sample) { return output_hz == 32'000U
                    ? sample == 100 : sample >= 98 && sample <= 102; }), "jitter altered PCM");
            require(SDL_GetAudioStreamQueued(jitter) <= static_cast<int>(limit_ms*32U*4U), "jitter exceeded latency cap");
        }
        SDL_DestroyAudioStream(jitter);
        return underruns;
    };
    require(jitter_underruns(8, 100) > 0, "legacy Switch underrun was not reproduced");
    require(jitter_underruns(starfox::app::realtime_audio_startup_ms,
        starfox::app::realtime_audio_limit_ms) == 0, "bounded headroom did not prevent jitter underruns");
    require(jitter_underruns(android_policy.startup_ms, android_policy.limit_ms) == 0,
        "Android reduced-latency policy underruns with 16 ms scheduling jitter");
    require(jitter_underruns(android_policy.startup_ms, android_policy.limit_ms, 48'000U) == 0,
        "Android reduced-latency policy underruns on a resampling 48 kHz device");
    require(jitter_underruns(switch_policy.startup_ms, switch_policy.limit_ms) == 0,
        "Switch startup headroom regressed");
    SDL_Quit();
    std::cout << "Realtime audio queue tests passed.\n";
}
