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

// Source PCM arrives in complete 50 ms packets. A 24 ms cushion and forced
// 256-frame callbacks leave little handheld scheduling headroom (#56/#60).
// Restore the safer policy and let SDL/AAudio choose its device
// buffer and retain more than one source tick of application headroom. The
// 150 ms cap still bounds catch-up latency; source cadence/pitch never change.
[[nodiscard]] constexpr AudioPlaybackPolicy audio_playback_policy(
    AudioPlaybackPlatform platform) noexcept {
    switch (platform) {
    case AudioPlaybackPlatform::android: return {64U, 150U, 0U, true};
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
