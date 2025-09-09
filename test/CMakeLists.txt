
add_executable(basic_test basic_test.cc)
target_link_libraries(basic_test skiplist)
target_include_directories(basic_test PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/../include
)
add_test(NAME basic_test COMMAND basic_test)

add_executable(thread_test thread_test.cc)
target_link_libraries(thread_test skiplist)
target_include_directories(thread_test PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/../include
)
add_test(NAME thread_test COMMAND thread_test)
target_compile_options(thread_test PRIVATE -fsanitize=address)
target_link_options(thread_test PRIVATE -fsanitize=address)