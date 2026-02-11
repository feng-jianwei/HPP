cmake_minimum_required(VERSION 3.20)

project(HPP)

# 设置模块路径
set(CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake;${CMAKE_MODULE_PATH}")
# 设置 C++ 标准
set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
message("CMAKE_CXX_STANDARD : ${CMAKE_CXX_STANDARD}")
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

find_program(CCACHE_EXECUTABLE ccache)
if(CCACHE_EXECUTABLE)
    message(STATUS "Found ccache: ${CCACHE_EXECUTABLE}")
    # 设置为默认 launcher（影响所有后续 target）
    set(CMAKE_C_COMPILER_LAUNCHER   "${CCACHE_EXECUTABLE}")
    set(CMAKE_CXX_COMPILER_LAUNCHER "${CCACHE_EXECUTABLE}")
    # 如果有其他语言，按需加
    # set(CMAKE_CUDA_COMPILER_LAUNCHER "${CCACHE_EXECUTABLE}")
else()
    message(WARNING "ccache not found, proceeding without it")
endif()

set(BUILD_SHARED_LIBS ON CACHE BOOL "Build using shared libraries" FORCE)

set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)

add_compile_options(-Wall -Wextra)
add_link_options(-Wl,--allow-shlib-undefined)

if(CMAKE_CXX_COMPILER_ID MATCHES "Clang")
    add_compile_options(-Wno-unused-command-line-argument)
    # 核心：告诉 Clang 用 libc++ 作为标准库
    add_compile_options(-stdlib=libc++)

    # 强烈推荐：禁止自动搜索 libstdc++ 的 include 路径（防止混淆）
    add_compile_options(-nostdinc++)

    # 显式添加 libc++ 的标准头文件路径（Ubuntu/Debian/WSL 常见位置）
    # 注意：路径必须存在，可用 ls /usr/include/c++/v1 检查
    include_directories(SYSTEM /usr/include/c++/v1)

    # 链接选项：libc++ + abi（异常支持）
    add_link_options(-lc++ -lc++abi)

    # 可选：更严格的警告，帮助调试
    add_compile_options(-Wall -Wextra -Wpedantic)
endif()

# 在项目最顶层 CMakeLists.txt 最早的地方设置（在 project() 之前最好）
# set(FETCHCONTENT_BASE_DIR "${CMAKE_SOURCE_DIR}/_deps" CACHE PATH "Base dir for all fetched content")

include(benchmark)
include(gtest)

add_subdirectory(src)