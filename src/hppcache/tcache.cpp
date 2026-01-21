#include <benchmark/benchmark.h>
#include <vector>
#include <thread>
#include <atomic>
#include <cstdint>

// ────────────────────────────────────────────────
//  Part 1: 缓存局部性（行优先 vs 列优先）
// ────────────────────────────────────────────────

static constexpr int N = 4096;
static constexpr int M = 4096;

static void BM_RowMajor(benchmark::State& state) {
    std::vector<std::vector<double>> mat(N, std::vector<double>(M, 1.0));

    for (auto _ : state) {
        double sum = 0;
        for (int i = 0; i < N; ++i) {
            for (int j = 0; j < M; ++j) {
                sum += mat[i][j];
            }
        }
        benchmark::DoNotOptimize(sum);
    }
}
BENCHMARK(BM_RowMajor)->Unit(benchmark::kMillisecond);


static void BM_ColumnMajor(benchmark::State& state) {
    std::vector<std::vector<double>> mat(N, std::vector<double>(M, 1.0));

    for (auto _ : state) {
        double sum = 0;
        for (int j = 0; j < M; ++j) {
            for (int i = 0; i < N; ++i) {
                sum += mat[i][j];   // 列优先 → 很差的局部性
            }
        }
        benchmark::DoNotOptimize(sum);
    }
}
BENCHMARK(BM_ColumnMajor)->Unit(benchmark::kMillisecond);


// ────────────────────────────────────────────────
//  Part 2: 伪共享（False Sharing） vs 避免伪共享
// ────────────────────────────────────────────────

static constexpr int THREADS = 8;
static constexpr int ITER = 50'000'000;

// 情况1：所有线程的计数器紧密排列 → 严重伪共享
struct PaddedFalse {
    alignas(64) std::atomic<int64_t> counters[THREADS];
};

static void BM_FalseSharing(benchmark::State& state) {
    PaddedFalse data{};

    std::vector<std::thread> threads;
    for (int t = 0; t < THREADS; ++t) {
        threads.emplace_back([&, t] {
            auto& cnt = data.counters[t];
            for (int i = 0; i < ITER; ++i) {
                cnt.fetch_add(1, std::memory_order_relaxed);
            }
        });
    }

    for (auto& th : threads) th.join();

    for (auto _ : state) {
        // 只是防止优化掉上面代码
        benchmark::DoNotOptimize(data.counters[0].load(std::memory_order_relaxed));
    }
}
BENCHMARK(BM_FalseSharing)
    ->Unit(benchmark::kMillisecond)
    ->Iterations(1);   // 只跑一次，因为我们关心的是多线程行为


// 情况2：每个计数器独占一个（或多个）缓存行 → 消除伪共享
struct PaddedGood {
    struct alignas(64) Counter {
        std::atomic<int64_t> value{};
        char padding[64 - sizeof(std::atomic<int64_t>)];
    };
    Counter counters[THREADS];
};

static void BM_NoFalseSharing(benchmark::State& state) {
    PaddedGood data{};

    std::vector<std::thread> threads;
    for (int t = 0; t < THREADS; ++t) {
        threads.emplace_back([&, t] {
            auto& cnt = data.counters[t].value;
            for (int i = 0; i < ITER; ++i) {
                cnt.fetch_add(1, std::memory_order_relaxed);
            }
        });
    }

    for (auto& th : threads) th.join();

    for (auto _ : state) {
        benchmark::DoNotOptimize(data.counters[0].value.load(std::memory_order_relaxed));
    }
}
BENCHMARK(BM_NoFalseSharing)
    ->Unit(benchmark::kMillisecond)
    ->Iterations(1);


// 更激进的填充方式（常见于生产代码）
struct alignas(128) AlignedCounter {
    std::atomic<int64_t> value{};
};

static void BM_128ByteAligned(benchmark::State& state) {
    alignas(128) AlignedCounter counters[THREADS];

    std::vector<std::thread> threads;
    for (int t = 0; t < THREADS; ++t) {
        threads.emplace_back([&, t] {
            auto& cnt = counters[t].value;
            for (int i = 0; i < ITER; ++i) {
                cnt.fetch_add(1, std::memory_order_relaxed);
            }
        });
    }

    for (auto& th : threads) th.join();

    for (auto _ : state) {
        benchmark::DoNotOptimize(counters[0].value.load());
    }
}
BENCHMARK(BM_128ByteAligned)
    ->Unit(benchmark::kMillisecond)
    ->Iterations(1);


BENCHMARK_MAIN();