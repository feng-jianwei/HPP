# 获取当前目录下所有的 .cpp 文件
file(GLOB cpp_sources *.cpp *.c)

link_directories(${CMAKE_BINARY_DIR}/lib)
link_libraries(benchmark)
# 为每个源文件生成一个可执行文件
foreach(source ${cpp_sources})
    # 获取文件名（不带扩展名）
    get_filename_component(exec_name ${source} NAME_WE)
    # 添加一个可执行文件目标
    add_executable(${exec_name} ${source})
endforeach()