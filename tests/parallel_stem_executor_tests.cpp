#include "starfox/audio/parallel_stem_executor.hpp"
#include <iostream>
#include <stdexcept>
#include <thread>

namespace {
void require(bool value, const char* message) {
    if (!value) throw std::runtime_error(message);
}
struct Work {
    std::thread::id thread;
    unsigned calls{};
    bool fail{};
    static void run(void* context) {
        auto& self = *static_cast<Work*>(context);
        self.thread = std::this_thread::get_id();
        ++self.calls;
        if (self.fail) throw std::runtime_error("Expected task failure");
    }
};
}
int main() try {
    using starfox::audio::ParallelStemExecutor;
    {
        ParallelStemExecutor configured;
        if(!ParallelStemExecutor::default_enabled())
            require(!configured.parallel_available(),"Serial audio override was ignored");
    }
    for (bool parallel : {false, true}) {
        ParallelStemExecutor executor(parallel);
        require(executor.parallel_available() == parallel, "Executor test did not get its requested mode");
        Work foreground, background;
        std::thread::id worker;
        for (unsigned i = 0; i < 128; ++i) {
            executor.execute({&foreground, Work::run}, {&background, Work::run});
            require(foreground.thread == std::this_thread::get_id(), "Foreground task left its owner thread");
            require((background.thread != foreground.thread) == parallel, "Wrong SPC execution mode");
            if (i) require(background.thread == worker, "SPC worker was recreated per tick");
            worker = background.thread;
        }
        require(foreground.calls == 128 && background.calls == 128, "Executor returned before work completed");
        for (unsigned failure_mode : {1U, 2U, 3U}) {
            foreground.fail = (failure_mode & 1) != 0;
            background.fail = (failure_mode & 2) != 0;
            bool caught{};
            const auto before = background.calls;
            try { executor.execute({&foreground, Work::run}, {&background, Work::run}); }
            catch (const std::runtime_error&) { caught = true; }
            require(caught, "Task failure was silently discarded");
            if (parallel) require(background.calls == before + 1, "Foreground failure did not join background work");
            foreground.fail = background.fail = false;
            executor.execute({&foreground, Work::run}, {&background, Work::run});
        }
        bool invalid{};
        try { executor.execute({}, {&background, Work::run}); }
        catch (const std::invalid_argument&) { invalid = true; }
        require(invalid, "Missing stem callback was accepted");
        struct Reentrant {
            ParallelStemExecutor& executor;
            bool rejected{};
            static void run(void* context) {
                auto& self = *static_cast<Reentrant*>(context);
                Work first, second;
                try { self.executor.execute({&first, Work::run}, {&second, Work::run}); }
                catch (const std::logic_error&) { self.rejected = true; }
            }
        } nested{executor};
        executor.execute({&nested, Reentrant::run}, {&background, Work::run});
        require(nested.rejected, "Overlapping borrowed SPC jobs were accepted");
    }
    std::cout << "Persistent SPC worker: serial fallback, joined exceptions, overlap rejection and reuse passed\n";
    return 0;
} catch (const std::exception& error) {
    std::cerr << error.what() << '\n';
    return 1;
}
