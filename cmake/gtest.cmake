include(FetchContent)
enable_testing()                       # 别忘了打开测试开关
# 声明 GoogleTest
FetchContent_Declare(
    googletest
    GIT_REPOSITORY https://github.com/google/googletest.git
    GIT_TAG        v1.15.2             # 2025年常用版本，建议固定 tag
    SOURCE_DIR ${PROJECT_SOURCE_DIR}/third_party/googletest
)
FetchContent_MakeAvailable(googletest)