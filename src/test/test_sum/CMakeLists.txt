# 获取当前目录下所有的 .cpp 文件
file(GLOB cpp_sources *.cpp *.c)

add_executable(test_sum ${cpp_sources})

target_include_directories(test_sum PRIVATE 
    ${CMAKE_CURRENT_SOURCE_DIR}
    )

target_link_directories(test_sum PRIVATE 
    ${CMAKE_BINARY_DIR}/lib
    )
target_link_libraries(test_sum PRIVATE 
    benchmark
    )

target_compile_options(test_sum
        PRIVATE
            -O1
    )

include(CheckCXXCompilerFlag)
check_cxx_compiler_flag("-mavx512f" HAS_AVX512F)
check_cxx_compiler_flag("-mavx512dq" HAS_AVX512DQ)
check_cxx_compiler_flag("-mavx2" HAS_AVX2)
check_cxx_compiler_flag("-mfma" HAS_FMA)  # AVX2 常和 FMA 一起用
# 机器不支持AVX-512 

if(HAS_AVX2 AND HAS_FMA)
    message(STATUS "AVX2 support detected, enabling -mavx2 -mfma")
    target_compile_options(test_sum PRIVATE -march=native -mavx2 -mfma)
else()
    message("AVX2 not supported, using scalar fallback")
endif()