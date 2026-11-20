#pragma once

#include <algorithm>
#include <condition_variable>
#include <cstdint>
#include <mutex>
#include <thread>
#include <vector>

// Zero-allocation persistent worker pool for column/row-stripe parallelism.
// Uses raw function pointer + void* context (never std::function) so hot paths allocate 0 bytes.
class ParallelPool {
public:
    ParallelPool() {
        const unsigned hw = std::thread::hardware_concurrency();
        const unsigned workers = hw > 1 ? std::min(hw - 1, 7u) : 0u;
        threads_.reserve(workers);
        for (unsigned i = 0; i < workers; ++i) {
            threads_.emplace_back([this, i] { workerLoop(i); });
        }
    }

    ~ParallelPool() {
        {
            std::lock_guard<std::mutex> lk(mu_);
            stop_ = true;
        }
        cvStart_.notify_all();
        for (auto& t : threads_) t.join();
    }

    ParallelPool(const ParallelPool&) = delete;
    ParallelPool& operator=(const ParallelPool&) = delete;

    // Splits [0, count) into stripes aligned to `align` (e.g. 32 cells = 64B cache line).
    template <typename Fn>
    void parallelFor(int count, int minParallel, int align, Fn&& fn) {
        if (threads_.empty() || count < minParallel) {
            fn(0, count);
            return;
        }
        const int parts = static_cast<int>(threads_.size()) + 1;
        const int step = std::max(1, align);
        const int slice = ((count / parts + step - 1) / step) * step;
        if (slice >= count) {
            fn(0, count);
            return;
        }
        auto trampoline = [](void* ctx, int a, int b) {
            (*static_cast<Fn*>(ctx))(a, b);
        };
        {
            std::lock_guard<std::mutex> lk(mu_);
            fn_ = trampoline;
            ctx_ = &fn;
            count_ = count;
            slice_ = slice;
            remaining_ = static_cast<int>(threads_.size());
            ++generation_;
        }
        cvStart_.notify_all();
        // Main thread runs stripe 0 while worker threads run stripes 1..N.
        fn(0, std::min(slice, count));
        std::unique_lock<std::mutex> lk(mu_);
        cvDone_.wait(lk, [&] { return remaining_ == 0; });
        fn_ = nullptr;
        ctx_ = nullptr;
    }

private:
    using TaskFn = void(*)(void*, int, int);

    void workerLoop(unsigned index) {
        std::uint64_t seen = 0;
        for (;;) {
            TaskFn fn = nullptr;
            void* ctx = nullptr;
            int a = 0, b = 0;
            {
                std::unique_lock<std::mutex> lk(mu_);
                cvStart_.wait(lk, [&] { return stop_ || generation_ != seen; });
                if (stop_) return;
                seen = generation_;
                fn = fn_;
                ctx = ctx_;
                a = static_cast<int>(index + 1) * slice_;
                b = (index + 1 == threads_.size()) ? count_ : std::min(count_, a + slice_);
            }
            if (fn && a < b) fn(ctx, a, b);
            {
                std::lock_guard<std::mutex> lk(mu_);
                if (--remaining_ == 0) cvDone_.notify_one();
            }
        }
    }

    std::vector<std::thread> threads_;
    std::mutex mu_;
    std::condition_variable cvStart_, cvDone_;
    TaskFn fn_ = nullptr;
    void* ctx_ = nullptr;
    int count_ = 0, slice_ = 0, remaining_ = 0;
    std::uint64_t generation_ = 0;
    bool stop_ = false;
};

inline ParallelPool& sharedPool() {
    static ParallelPool pool;
    return pool;
}
