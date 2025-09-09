cmake_minimum_required(VERSION 3.10)
project(skiplist CXX)
add_library(skiplist INTERFACE)
target_include_directories(skiplist INTERFACE
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)

enable_testing()
add_subdirectory(test)
