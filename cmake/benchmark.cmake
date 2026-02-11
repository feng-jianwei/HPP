include(FetchContent)

# 外部项目申明
FetchContent_Declare(
    benchmark
    GIT_REPOSITORY https://github.com/google/benchmark.git
    GIT_TAG        v1.8.5          # 2025-2026 推荐用最新稳定版，可换成 main 分支
    SOURCE_DIR ${PROJECT_SOURCE_DIR}/third_party/benchmark
)
# 手动下载项目而不添加到构建系统中
FetchContent_Populate(benchmark)
set(BENCHMARK_ENABLE_TESTING        OFF CACHE BOOL "" FORCE)
include_directories(${PROJECT_SOURCE_DIR}/third_party/benchmark/include)
add_subdirectory(${PROJECT_SOURCE_DIR}/third_party/benchmark)
