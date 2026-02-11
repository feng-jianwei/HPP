include(FetchContent)
FetchContent_Declare(
    liburing
    GIT_REPOSITORY https://github.com/axboe/liburing.git
    GIT_TAG         master          # 改成你想要的 tag 或 master
    SOURCE_DIR ${PROJECT_SOURCE_DIR}/third_party/liburing
)

message(STATUS "Populating liburing...")
FetchContent_Populate(liburing)

# ---------------- 编译，并 install ----------------
set(LIBURING_SOURCE_DIR ${liburing_SOURCE_DIR})
set(LIBURING_INSTALL_DIR ${PROJECT_SOURCE_DIR}/third_party/liburing)

# 运行 configure（你可以加 --prefix=/tmp/xxx 但没必要）
execute_process(
    COMMAND ./configure --prefix=${LIBURING_INSTALL_DIR}
    WORKING_DIRECTORY ${LIBURING_SOURCE_DIR}
    RESULT_VARIABLE CONFIG_RESULT
)
if(NOT CONFIG_RESULT EQUAL 0)
    message(FATAL_ERROR "liburing configure failed")
endif()

# 只 build（可以加 -j）
execute_process(
    COMMAND make -j ${CMAKE_BUILD_PARALLEL_LEVEL}
    WORKING_DIRECTORY ${LIBURING_SOURCE_DIR}
    RESULT_VARIABLE BUILD_RESULT
)
if(NOT BUILD_RESULT EQUAL 0)
    message(FATAL_ERROR "liburing build failed")
endif()

execute_process(
    COMMAND make install
    WORKING_DIRECTORY ${liburing_SOURCE_DIR}
    RESULT_VARIABLE INSTALL_RESULT
)
if(NOT INSTALL_RESULT EQUAL 0)
    message(FATAL_ERROR "liburing install failed")
endif()