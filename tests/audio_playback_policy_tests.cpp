#include "starfox/app/audio_playback_policy.hpp"

using namespace starfox::app;
constexpr auto policy = native_audio_playback_policy;
#if defined(__ANDROID__)
static_assert(policy.startup_ms == 24 && policy.device_sample_frames == 256
    && policy.game_stream_role);
#elif defined(__SWITCH__)
static_assert(policy.startup_ms == 64 && policy.device_sample_frames == 512
    && !policy.game_stream_role);
#else
static_assert(policy.startup_ms == 64 && policy.device_sample_frames == 0
    && !policy.game_stream_role);
#endif
static_assert(policy.limit_ms == 150);
int main() { return 0; }
