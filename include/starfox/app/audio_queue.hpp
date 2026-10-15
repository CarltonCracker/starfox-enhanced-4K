#pragma once

#include <SDL3/SDL_audio.h>
#include <SDL3/SDL_error.h>
#include <SDL3/SDL_hints.h>

#include "starfox/app/audio_playback_policy.hpp"

#include <cstdint>
#include <limits>
#include <span>
#include <string>

namespace starfox::app {

inline constexpr std::uint32_t realtime_audio_startup_ms = native_audio_playback_policy.startup_ms;
inline constexpr std::uint32_t realtime_audio_limit_ms = native_audio_playback_policy.limit_ms;

// Apply before SDL audio initialization/opening a device, in both the flat
// runtime and Quest/PCVR PCM host. Explicit SDL environment/user overrides
// remain authoritative; these are platform defaults, not forced values.
inline void configure_realtime_audio_device(
    AudioPlaybackPolicy policy = native_audio_playback_policy) {
    const char* frames = policy.device_sample_frames == 256U ? "256"
        : policy.device_sample_frames == 512U ? "512" : nullptr;
    if (frames) static_cast<void>(SDL_SetHintWithPriority(
        SDL_HINT_AUDIO_DEVICE_SAMPLE_FRAMES, frames, SDL_HINT_DEFAULT));
    if (policy.game_stream_role) static_cast<void>(SDL_SetHintWithPriority(
        SDL_HINT_AUDIO_DEVICE_STREAM_ROLE, "Game", SDL_HINT_DEFAULT));
}

// The callback size is a request: some Android drivers choose another size.
// Record the negotiated format/buffer so a device-specific report can show
// whether the short-buffer request was actually honored.
inline std::string realtime_audio_device_description(SDL_AudioStream* stream) {
    SDL_AudioSpec device{};
    int frames{};
    if (!SDL_GetAudioDeviceFormat(SDL_GetAudioStreamDevice(stream), &device, &frames))
        return "Audio device format unavailable";
    const auto* driver = SDL_GetCurrentAudioDriver();
    return "Audio playback: driver=" + std::string{driver ? driver : "unknown"}
        + " output-hz=" + std::to_string(device.freq)
        + " channels=" + std::to_string(device.channels)
        + " callback-frames=" + std::to_string(frames)
        + " startup-ms=" + std::to_string(realtime_audio_startup_ms)
        + " queue-limit-ms=" + std::to_string(realtime_audio_limit_ms);
}

// SDL reports queued bytes in the stream's INPUT format. This stream always
// accepts interleaved S16 stereo, even when the device resamples to 48 kHz.
// Limit only the playback FIFO: callers must still run every SPC/MSU update
// and deliver its output ports, including when old playback is discarded.
inline bool queue_realtime_audio(SDL_AudioStream* stream,
    std::span<const std::int16_t> samples, std::uint32_t max_source_frames) {
    constexpr auto bytes_per_frame = 2U * sizeof(std::int16_t);
    const auto limit = static_cast<std::uint64_t>(max_source_frames)
        * bytes_per_frame;
    if (samples.size() % 2U != 0U || samples.size_bytes() > limit
        || samples.size_bytes() > std::numeric_limits<int>::max()) {
        return SDL_SetError("Invalid realtime stereo audio packet size");
    }
    const auto queued = SDL_GetAudioStreamQueued(stream);
    if (queued < 0) return false;
    if (static_cast<std::uint64_t>(queued) + samples.size_bytes() > limit) {
        // A render stall/catch-up burst must not leave old sound effects
        // playing behind current input indefinitely. Rejoin current audio.
        if (!SDL_ClearAudioStream(stream)) return false;
    }
    return SDL_PutAudioStreamData(stream, samples.data(),
        static_cast<int>(samples.size_bytes()));
}

} // namespace starfox::app
