#include <benchmark/benchmark.h>
#include <vector>
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

static constexpr int THREADS = 16;
static constexpr int ITER = 50'000'000;

// 情况1：所有线程的计数器紧密排列 → 严重伪共享
struct PaddedFalse {
    int64_t counters[THREADS];
};
static PaddedFalse data{};
static void BM_FalseSharing(benchmark::State& state) {
    for (auto _ : state) {
        auto& cnt = data.counters[state.thread_index()];
        for (int i = 0; i < ITER; ++i) {
            cnt++;
        }
    }
}
BENCHMARK(BM_FalseSharing)->Threads(THREADS)
    ->Unit(benchmark::kMillisecond)
    ->Iterations(1);   // 只跑一次，因为我们关心的是多线程行为


// 情况2：每个计数器独占一个（或多个）缓存行 → 消除伪共享

struct alignas(64) Padder {
    int64_t value{};
    char padding[64 - sizeof(int64_t)];
};
alignas(64) Padder padData[THREADS];

static void BM_NoFalseSharing(benchmark::State& state) {
    for (auto _ : state) {
        auto& cnt = padData[state.thread_index()].value;
        for (int i = 0; i < ITER; ++i) {
            cnt++;
        }
    }
}
BENCHMARK(BM_NoFalseSharing)->Threads(THREADS)
    ->Unit(benchmark::kMillisecond)
    ->Iterations(1);


// 更激进的填充方式（常见于生产代码）
struct alignas(128) AlignedCounter {
    int64_t value{};
};

alignas(128) AlignedCounter counters[THREADS];
static void BM_128ByteAligned(benchmark::State& state) {
    for (auto _ : state) {
        auto& cnt = counters[state.thread_index()].value;
        for (int i = 0; i < ITER; ++i) {
            cnt++;
        }
    }
}
BENCHMARK(BM_128ByteAligned)->Threads(THREADS)
    ->Unit(benchmark::kMillisecond)
    ->Iterations(1);


// LocalValue
struct LocalValue {
    int64_t value{};
};

thread_local LocalValue LocalValue;
static void BM_LocalValue(benchmark::State& state) {
    for (auto _ : state) {
        auto& cnt =LocalValue.value;
        for (int i = 0; i < ITER; ++i) {
            cnt++;
        }
    }
}
BENCHMARK(BM_LocalValue)->Threads(THREADS)
    ->Unit(benchmark::kMillisecond)
    ->Iterations(1);

BENCHMARK_MAIN();