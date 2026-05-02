cmake_minimum_required(VERSION 3.20 FATAL_ERROR)

project(verihogg-format VERSION 1.0 LANGUAGES CXX)

# Default build type
if(NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE Release CACHE STRING "Choose build type" FORCE)
endif()

# Use C++17
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

file(GLOB_RECURSE SRC formatter/src/**.cpp)

find_package(slang REQUIRED)
find_package(Microsoft.GSL CONFIG REQUIRED)

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)

add_executable(${PROJECT_NAME} ${SRC})

target_include_directories(${PROJECT_NAME}
  PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/formatter/include>
)

target_link_libraries(${PROJECT_NAME} slang::slang Microsoft.GSL::GSL)

if(MSVC)
    target_compile_options(${PROJECT_NAME} PRIVATE /W4 /bigobj)
else()
    target_compile_options(${PROJECT_NAME} PRIVATE -Wall -Wextra -Wpedantic -Werror)
endif()

# Install rule
include(GNUInstallDirs)
install(TARGETS ${PROJECT_NAME}
        RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})

find_package(GTest QUIET)
if(GTest_FOUND)
  enable_testing()
  message(STATUS "GTest_FOUND: ${GTest_FOUND}")
  message(STATUS "TEST_SRC: ${TEST_SRC}")
  message(STATUS "LIB_SRC: ${LIB_SRC}")

  file(GLOB_RECURSE LIB_SRC formatter/src/**.cpp)
  list(FILTER LIB_SRC EXCLUDE REGEX ".*/main\\.cpp$")

  add_library(verihogg-format-lib STATIC ${LIB_SRC})
  target_include_directories(verihogg-format-lib PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/formatter/include>)
  target_link_libraries(verihogg-format-lib PUBLIC slang::slang)

  file(GLOB_RECURSE TEST_SRC formatter/tests/**.cpp)
  if(TEST_SRC)
    add_executable(verihogg-tests ${TEST_SRC})
    target_link_libraries(verihogg-tests PRIVATE verihogg-format-lib GTest::gtest GTest::gtest_main)
    include(GoogleTest)
    gtest_discover_tests(verihogg-tests)
  endif()
endif()

