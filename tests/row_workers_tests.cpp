#include "starfox/render/row_workers.hpp"

#include <algorithm>
#include <atomic>
#include <chrono>
#include <condition_variable>
#include <exception>
#include <iostream>
#include <mutex>
#include <stdexcept>
#include <string_view>
#include <vector>

namespace {
void check(bool condition, const char* message) {
    if (!condition) throw std::runtime_error(message);
}
struct RenderFailure : std::exception {};

void check_reuse(starfox::render::RowWorkers& workers) {
    for (const unsigned rows : {0U, 1U, 17U, 127U, 128U, 257U}) {
        std::vector<unsigned> visits(rows);
        workers.parallel_rows(rows, [&](unsigned first, unsigned last) {
            for (unsigned y = first; y < last; ++y) ++visits[y];
        });
        check(std::all_of(visits.begin(), visits.end(), [](auto n) { return n == 1; }),
            "Row partition was lost, duplicated or corrupted after a failed pass");
    }
}

void check_throw(starfox::render::RowWorkers& workers, unsigned count, bool caller, bool every) {
    std::atomic<unsigned> finished{};
    bool caught{};
    try {
        workers.parallel_rows(count * 32, [&](unsigned first, unsigned) {
            struct Completion {
                std::atomic<unsigned>& finished;
                ~Completion() { ++finished; }
            } completion{finished};
            if (every || (caller ? first == 0 : first != 0)) throw RenderFailure{};
        });
    } catch (const RenderFailure&) { caught = true; }
    check(caught, "A render callback exception did not reach its dispatching caller");
    check(finished == count, "Failure escaped before all callbacks were joined");
    check_reuse(workers);
}

// The caller fails while another slice deliberately remains in progress.
// Keep every capture alive even against the old implementation, so detecting
// the premature return does not itself invoke a dangling callback.
void check_join_before_rethrow(starfox::render::RowWorkers& workers) {
    std::mutex mutex;
    std::condition_variable changed;
    bool started{}, throwing{}, release{}, returned{}, returned_early{}, finished{};
    std::thread coordinator([&] {
        std::unique_lock lock{mutex};
        changed.wait(lock, [&] { return throwing; });
        returned_early = changed.wait_for(lock, std::chrono::milliseconds(100), [&] { return returned; });
        release = true;
        changed.notify_all();
    });
    bool caught{};
    const std::function<void(unsigned, unsigned)> body = [&](unsigned first, unsigned) {
        std::unique_lock lock{mutex};
        if (!first) {
            changed.wait(lock, [&] { return started; });
            throwing = true;
            changed.notify_all();
            throw RenderFailure{};
        }
        started = true;
        changed.notify_all();
        changed.wait(lock, [&] { return release; });
        finished = true;
        changed.notify_all();
    };
    try { workers.parallel_rows(128, body); }
    catch (const RenderFailure&) { caught = true; }
    {
        std::unique_lock lock{mutex};
        returned = true;
        changed.notify_all();
        changed.wait(lock, [&] { return finished; });
    }
    coordinator.join();
    check(caught && !returned_early, "Caller failure returned with render slices still using its job");
    check_reuse(workers);
}
} // namespace

int main(int argc, char** argv) try {
    if (argc == 2 && std::string_view{argv[1]} == "--worker-throw") {
        starfox::render::RowWorkers workers;
        workers.set_worker_count(2);
        check_throw(workers, 2, false, false);
    } else if (argc == 2 && std::string_view{argv[1]} == "--caller-join") {
        starfox::render::RowWorkers workers;
        workers.set_worker_count(2);
        check_join_before_rethrow(workers);
    } else {
        for (const unsigned count : {1U, 2U, 4U, 8U}) {
            starfox::render::RowWorkers workers;
            workers.set_worker_count(count);
            check_reuse(workers);
            for (unsigned repeat = 0; repeat < 16; ++repeat) {
                check_throw(workers, count, true, false);
                if (count > 1) check_throw(workers, count, false, false);
                check_throw(workers, count, false, true);
            }
            workers.set_worker_count(2);
            check_join_before_rethrow(workers);
            workers.set_worker_count(1);
            check_reuse(workers);
        }
    }
    std::cout << "Render workers: caller/worker failures joined and propagated; partitions and pool reuse exact\n";
    return 0;
} catch (const std::exception& error) {
    std::cerr << "FAILED: " << error.what() << '\n';
    return 1;
}
