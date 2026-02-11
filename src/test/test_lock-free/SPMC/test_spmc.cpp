#include "SpmcRingBuffer.h"
#include <atomic>
#include <benchmark/benchmark.h>
#include <cstddef>
#include <cstdio>
#include <iostream>
#include <thread>
#include <unistd.h>

constexpr size_t testSize = 1e7;

const int threadsNum = std::thread::hardware_concurrency();

SpmcRingBuffer<bool, (1 << 27)> spmc;
std::atomic<size_t> popSuccessSum{0}; 

static void BM_SpmcRingBuffer(benchmark::State& state) {
    for (auto _ : state) {
        size_t successPut = 0;
        size_t successPop = 0;
        if (state.thread_index() == 0) {
            for (size_t i = 0; i < testSize * 10; ++i) {
                successPut += spmc.try_emplace(1);
            }
            std::cout << "successPut: " << successPut << std::endl; 
        } else {
            usleep(1);
            while (spmc.size() > 0) {
                auto out = true;
                auto k = spmc.try_pop(out);
                if (k) {
                    successPop += out;
                }
            }
            popSuccessSum += successPop;
        }
    }
}
BENCHMARK(BM_SpmcRingBuffer)->Unit(benchmark::kMillisecond)->Iterations(1)->Threads(threadsNum);

// 主函数
int main(int argc, char **argv) {
    char arg0_default[] = "benchmark";
    char *args_default = arg0_default;
    if (!argv) {
    argc = 1;
    argv = &args_default;
    }
    ::benchmark ::Initialize(&argc, argv);
    if (::benchmark ::ReportUnrecognizedArguments(argc, argv))
    return 1;
    ::benchmark ::RunSpecifiedBenchmarks();
    std::cout << "successPopSum" << popSuccessSum << std::endl;
    ::benchmark ::Shutdown();
    return 0;
}
int main(int, char **);