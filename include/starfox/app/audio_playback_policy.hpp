#pragma once

#include <cstdint>

namespace starfox::app {

enum class AudioPlaybackPlatform { desktop, android, switch_console };

struct AudioPlaybackPolicy {
    std::uint32_t startup_ms;
    std::uint32_t limit_ms;
    std::uint32_t device_sample_frames;
    bool game_stream_role;
};

// Source PCM still arrives in complete 50 ms packets. Android's AAudio
// backend adds its own triple-buffered mixer, so a 64 ms application prime
// on top of the default 20 ms callbacks makes effects noticeably late.
// Keep a 24 ms application cushion (enough for the 16 ms jitter regression)
// and request shorter hardware callbacks, without changing source cadence.
[[nodiscard]] constexpr AudioPlaybackPolicy audio_playback_policy(
    AudioPlaybackPlatform platform) noexcept {
    switch (platform) {
    case AudioPlaybackPlatform::android: return {24U, 150U, 256U, true};
    case AudioPlaybackPlatform::switch_console: return {64U, 150U, 512U, false};
    default: return {64U, 150U, 0U, false};
    }
}

inline constexpr auto native_audio_playback_policy = audio_playback_policy(
#if defined(__ANDROID__)
    AudioPlaybackPlatform::android
#elif defined(__SWITCH__)
    AudioPlaybackPlatform::switch_console
#else
    AudioPlaybackPlatform::desktop
#endif
);

} // namespace starfox::app
