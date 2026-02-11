#include "SpscRingBuffer.h"
#include <benchmark/benchmark.h>
#include <iostream>
#include <unistd.h>

constexpr size_t testSize = 1e8;

SpscRingBuffer<int, (1 << 10)> spmc;

static void BM_SpmcRingBuffer(benchmark::State& state) {
    for (auto _ : state) {
        size_t successPut = 0;
        size_t successPop = 0;
        if (state.thread_index() == 0) {
            for (size_t i = 0; i < testSize; ++i) {
                successPut += spmc.try_emplace(1);
            }
            std::cout << "successPut: " << successPut << " curr queue size: " << spmc.size() << std::endl; 
        } else {
            for (size_t i = 0; i < testSize; ++i) {
                auto out = 0;
                auto k = spmc.try_pop(out);
                if (k) {
                    successPop += out;
                }
            }
            std::cout << "successPop: " << successPop << " curr queue size: " << spmc.size() << std::endl; 
        }
    }
}
BENCHMARK(BM_SpmcRingBuffer)->Unit(benchmark::kMillisecond)->Iterations(1)->Threads(2);

// 主函数
BENCHMARK_MAIN();