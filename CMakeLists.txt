cmake_minimum_required(VERSION 3.14)
project(EventSystem VERSION 1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Header-only library (all implementation is in headers)
add_library(event_system INTERFACE)
target_include_directories(event_system INTERFACE include/)

# Main demo executable
add_executable(main src/main.cpp)
target_link_libraries(main PRIVATE event_system)
# Link pthreads for std::thread on Linux
find_package(Threads REQUIRED)
target_link_libraries(main PRIVATE Threads::Threads)

# Google Test
include(FetchContent)
FetchContent_Declare(
    googletest
    URL https://github.com/google/googletest/archive/refs/tags/v1.14.0.zip
)
set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(googletest)

enable_testing()

add_executable(run_tests tests/test_event_bus.cpp)
target_link_libraries(run_tests PRIVATE event_system GTest::gtest_main Threads::Threads)

include(GoogleTest)
gtest_discover_tests(run_tests)
