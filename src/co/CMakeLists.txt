# 获取当前目录下所有的 .cpp 文件
file(GLOB cpp_sources *.cpp)

add_executable(co ${cpp_sources})

target_include_directories(co PRIVATE 
    ${CMAKE_CURRENT_SOURCE_DIR}
    )

target_link_directories(co PRIVATE 
    ${CMAKE_BINARY_DIR}/lib
    )

target_compile_options(co
        PRIVATE
            -O2
    )