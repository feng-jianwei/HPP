file(GLOB SOURCE_FILES "*.cpp")
add_executable(test_spsc ${SOURCE_FILES})
target_include_directories(test_spsc PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
target_link_directories(test_spsc PRIVATE 
    ${CMAKE_BINARY_DIR}/lib
    )
target_link_libraries(test_spsc PRIVATE 
    benchmark
    )
