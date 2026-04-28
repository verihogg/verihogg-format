add_executable(test_tree_unwrapper test_tree_unwrapper.cpp)

target_link_libraries(test_tree_unwrapper PUBLIC formatter_core)
target_link_libraries(test_tree_unwrapper PUBLIC ${GTEST_LIBRARY} ${GTEST_MAIN_LIBRARY})

target_include_directories(test_tree_unwrapper
  PUBLIC
    ${GTEST_INCLUDE_DIR}
)

include(GoogleTest)
gtest_discover_tests(test_tree_unwrapper)
