cmake_minimum_required(VERSION 3.20)

project(HPP)

set(BUILD_SHARED_LIBS ON CACHE BOOL "Build using shared libraries" FORCE)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# 设置 C++ 标准
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED True)
set(CMAKE_BUILD_TYPE Debug)

set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)

add_compile_options(-Wall -Wextra)
add_link_options(-Wl,--allow-shlib-undefined)

# 在项目最顶层 CMakeLists.txt 最早的地方设置（在 project() 之前最好）
# set(FETCHCONTENT_BASE_DIR "${CMAKE_SOURCE_DIR}/_deps" CACHE PATH "Base dir for all fetched content")

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
include_directories(third_party/benchmark/include)
add_subdirectory(third_party/benchmark)

enable_testing()                       # 别忘了打开测试开关
# 声明 GoogleTest
FetchContent_Declare(
    googletest
    GIT_REPOSITORY https://github.com/google/googletest.git
    GIT_TAG        v1.15.2             # 2025年常用版本，建议固定 tag
    SOURCE_DIR ${PROJECT_SOURCE_DIR}/third_party/googletest
)
FetchContent_Populate(googletest)
add_subdirectory(third_party/googletest)

add_subdirectory(src)