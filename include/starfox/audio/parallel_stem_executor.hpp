#pragma once
#include "starfox/audio/stem_executor.hpp"
#include <memory>

namespace starfox::audio {
// One persistent worker for the independent music/effects SPC machines. Each
// execute remains synchronous: PCM, APU ports, save states and source cadence
// are complete before returning. No per-tick thread creation or borrowed jobs
// survive the call. The owning audio runtime serializes calls and destruction.
class ParallelStemExecutor final : public StemExecutor {
public:
    static bool default_enabled() noexcept;
    explicit ParallelStemExecutor(bool enabled = default_enabled());
    ~ParallelStemExecutor() override;
    ParallelStemExecutor(const ParallelStemExecutor&) = delete;
    ParallelStemExecutor& operator=(const ParallelStemExecutor&) = delete;
    bool parallel_available() const noexcept;
    void execute(StemTask foreground, StemTask background) override;
private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};
} // namespace starfox::audio
