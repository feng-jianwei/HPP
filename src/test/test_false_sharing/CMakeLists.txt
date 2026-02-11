# 获取当前目录下所有的 .cpp 文件
file(GLOB cpp_sources *.cpp *.c)

add_executable(test_false_sharing ${cpp_sources})

target_include_directories(test_false_sharing PRIVATE 
    ${CMAKE_CURRENT_SOURCE_DIR}
    )

target_link_directories(test_false_sharing PRIVATE 
    ${CMAKE_BINARY_DIR}/lib
    )
target_link_libraries(test_false_sharing PRIVATE 
    benchmark
    )