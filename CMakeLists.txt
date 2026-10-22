cmake_minimum_required(VERSION 3.16)
project(ChunksTransmission CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

find_package(Threads REQUIRED)

include(FetchContent)
FetchContent_Declare(
    googletest
    URL https://github.com/google/googletest/archive/refs/tags/v1.14.0.zip
)

set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(googletest)

add_library(filecollector STATIC
    src/FileCollector.cpp
)
target_include_directories(filecollector PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include)
target_link_libraries(filecollector PUBLIC Threads::Threads)

enable_testing()

add_executable(filecollector_tests tests/tests.cpp)
target_link_libraries(filecollector_tests PRIVATE
    filecollector
    GTest::gtest_main
    Threads::Threads
)

include(GoogleTest)
gtest_discover_tests(filecollector_tests)