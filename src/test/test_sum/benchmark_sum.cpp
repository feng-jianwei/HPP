#include <benchmark/benchmark.h>
#include <cstdio>
#include <immintrin.h>
#include <vector>

// 数组大小（可通过 --benchmark_filter 调整或修改常量）
constexpr size_t ARRAY_SIZE = 1e8;  // 1e8 个 int ≈ 800MB

// 全局数据（只初始化一次）
static std::vector<int> data(ARRAY_SIZE, 1);

// 1. naive 版本：严格依赖链
static void BM_Sum_Naive(benchmark::State& state) {
    int sum = 0;
    for (auto _ : state) {
        benchmark::DoNotOptimize(sum = 0.0);
        for (size_t i = 0; i < ARRAY_SIZE; ++i) {
            sum += data[i];
        }
        benchmark::DoNotOptimize(sum);
    }
    state.SetItemsProcessed(state.iterations() * ARRAY_SIZE);
    state.SetBytesProcessed(state.iterations() * ARRAY_SIZE * sizeof(int));
}
BENCHMARK(BM_Sum_Naive)->Unit(benchmark::kMillisecond)->Iterations(1);

// 2. 多累加器（8 个 scalar 累加器）
static void BM_Sum_MultiAccum(benchmark::State& state) {
    for (auto _ : state) {
        int acc[6] = {};
        size_t i = 0;
        for (; i + 5 < ARRAY_SIZE; i += 6) {
            acc[0] += data[i + 0];
            acc[1] += data[i + 1];
            acc[2] += data[i + 2];
            acc[3] += data[i + 3];
            acc[4] += data[i + 4];
            acc[5] += data[i + 5];
        }
        int total = acc[0] + acc[1] + acc[2] + acc[3] +
                       acc[4] + acc[5];
        for (; i < ARRAY_SIZE; ++i) {
            total += data[i];
        }
        benchmark::DoNotOptimize(total);
    }
    state.SetItemsProcessed(state.iterations() * ARRAY_SIZE);
    state.SetBytesProcessed(state.iterations() * ARRAY_SIZE * sizeof(int));
}
BENCHMARK(BM_Sum_MultiAccum)->Unit(benchmark::kMillisecond)->Iterations(1);


static void BM_Sum_AVX2_MultiAccum(benchmark::State& state) {

    for (auto _ : state) {
        __m256i acc[6] = {};  // 256-bit 整数向量
        size_t i = 0;

        // 主循环：一次处理 64 个 int（8 acc × 8 int/acc）
        for (; i + 47 < ARRAY_SIZE; i += 48) {
            acc[0] = _mm256_add_epi32(acc[0], _mm256_loadu_si256((__m256i*)(data.data() + i + 0)));
            acc[1] = _mm256_add_epi32(acc[1], _mm256_loadu_si256((__m256i*)(data.data() + i + 8)));
            acc[2] = _mm256_add_epi32(acc[2], _mm256_loadu_si256((__m256i*)(data.data() + i + 16)));
            acc[3] = _mm256_add_epi32(acc[3], _mm256_loadu_si256((__m256i*)(data.data() + i + 24)));
            acc[4] = _mm256_add_epi32(acc[4], _mm256_loadu_si256((__m256i*)(data.data() + i + 32)));
            acc[5] = _mm256_add_epi32(acc[5], _mm256_loadu_si256((__m256i*)(data.data() + i + 40)));
        }

        // 归约：把 8 个 __m256i 加起来（水平加法）
        __m256i sum = _mm256_setzero_si256();
        for (int k = 0; k < 6; ++k) {
            sum = _mm256_add_epi32(sum, acc[k]);
        }

        // AVX2 水平加法（4 个 64-bit 通道 → 最终 32-bit）
        __m128i low  = _mm256_extracti128_si256(sum, 0);
        __m128i high = _mm256_extracti128_si256(sum, 1);
        __m128i total128 = _mm_add_epi32(low, high);

        // 继续 shuffle 降维
        total128 = _mm_add_epi32(total128, _mm_shuffle_epi32(total128, _MM_SHUFFLE(2, 3, 0, 1)));
        total128 = _mm_add_epi32(total128, _mm_shuffle_epi32(total128, _MM_SHUFFLE(1, 0, 3, 2)));

        // 取最低 32-bit 作为结果（因为 int 是 32-bit）
        int result = _mm_cvtsi128_si32(total128);

        // 剩余部分用 scalar 处理
        for (; i < ARRAY_SIZE; ++i) {
            result += data[i];
        }

        benchmark::DoNotOptimize(result);
    }

    state.SetItemsProcessed(state.iterations() * ARRAY_SIZE);
    state.SetBytesProcessed(state.iterations() * ARRAY_SIZE * sizeof(int));
}
BENCHMARK(BM_Sum_AVX2_MultiAccum)->Unit(benchmark::kMillisecond)->Iterations(1);

// 主函数
BENCHMARK_MAIN();