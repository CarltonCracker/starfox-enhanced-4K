#include "starfox/audio/parallel_stem_executor.hpp"
#include <condition_variable>
#include <cstdlib>
#include <exception>
#include <mutex>
#include <stdexcept>
#include <string_view>
#include <system_error>
#include <thread>

namespace starfox::audio {
struct ParallelStemExecutor::Impl {
    std::mutex mutex;
    std::condition_variable ready, completed;
    std::thread worker;
    StemTask task{};
    std::exception_ptr failure;
    bool stopping{}, pending{}, done{}, busy{};

    explicit Impl(bool enabled) {
        if (!enabled) return;
        try { worker = std::thread([this] { run(); }); }
        catch (const std::system_error&) {
            // Thread restrictions/resource exhaustion must not prevent audio
            // playback on a single-core or sandboxed platform.
        }
    }
    ~Impl() {
        if (!worker.joinable()) return;
        {
            const std::lock_guard lock(mutex);
            stopping = true;
        }
        ready.notify_one();
        worker.join();
    }
    void run() noexcept {
        std::unique_lock lock(mutex);
        for (;;) {
            ready.wait(lock, [this] { return pending || stopping; });
            if (stopping && !pending) return;
            const auto work = task;
            lock.unlock();
            std::exception_ptr error;
            try { work.run(work.context); }
            catch (...) { error = std::current_exception(); }
            lock.lock();
            failure = error;
            task = {};
            pending = false;
            done = true;
            completed.notify_one();
        }
    }
    void execute(StemTask foreground, StemTask background) {
        if (!foreground.run || !background.run)
            throw std::invalid_argument("Missing SPC stem task");
        {
            const std::lock_guard lock(mutex);
            if (busy) throw std::logic_error("Overlapping SPC stem executor call");
            busy = true;
            if (worker.joinable()) {
                task = background;
                failure = {};
                done = false;
                pending = true;
            }
        }
        std::exception_ptr foreground_failure, background_failure;
        if (worker.joinable()) {
            ready.notify_one();
            try { foreground.run(foreground.context); }
            catch (...) { foreground_failure = std::current_exception(); }
            // The worker borrows stack-owned render state. Even foreground
            // failure cannot return until the background task has finished.
            std::unique_lock lock(mutex);
            completed.wait(lock, [this] { return done; });
            background_failure = failure;
            failure = {};
            busy = false;
        } else {
            try { foreground.run(foreground.context); background.run(background.context); }
            catch (...) { foreground_failure = std::current_exception(); }
            const std::lock_guard lock(mutex);
            busy = false;
        }
        if (foreground_failure) std::rethrow_exception(foreground_failure);
        if (background_failure) std::rethrow_exception(background_failure);
    }
};
bool ParallelStemExecutor::default_enabled() noexcept {
    const auto* sync = std::getenv("STARFOX_SYNC_AUDIO");
    return !(sync && std::string_view(sync) == "1")
        && std::thread::hardware_concurrency() > 1;
}
ParallelStemExecutor::ParallelStemExecutor(bool enabled) : impl_(std::make_unique<Impl>(enabled)) {}
ParallelStemExecutor::~ParallelStemExecutor() = default;
bool ParallelStemExecutor::parallel_available() const noexcept { return impl_->worker.joinable(); }
void ParallelStemExecutor::execute(StemTask foreground, StemTask background) {
    impl_->execute(foreground, background);
}
} // namespace starfox::audio
