cmake_minimum_required(VERSION 3.16)

set(SAN "" CACHE STRING "Set of sanitizer flags to use (address or thread)")
set_property(CACHE SAN PROPERTY STRINGS address thread)

if (SAN STREQUAL "address" OR SAN STREQUAL "")
    set(SAN_FLAGS "-fsanitize=address,leak,undefined")
elseif (SAN STREQUAL "thread")
    set(SAN_FLAGS "-fsanitize=thread,leak,undefined")
else()
    message(FATAL_ERROR "unknown sanitizer: ${SAN}")
endif()

project(looqueue CXX)

set(CMAKE_CXX_STANDARD 20)

# header only library
add_library(looqueue INTERFACE)
target_compile_features(looqueue INTERFACE cxx_std_20)
target_include_directories(looqueue INTERFACE include)

# example and test executables
find_package(Threads REQUIRED)

add_executable(test_stress test/test_stress.cpp)
target_link_libraries(test_stress PRIVATE Threads::Threads looqueue)
target_compile_options(test_stress PRIVATE ${SAN_FLAGS})
target_link_options(test_stress PRIVATE ${SAN_FLAGS})

add_executable(test test/test.cpp)
target_link_libraries(test PRIVATE Threads::Threads looqueue)
target_compile_options(test PRIVATE ${SAN_FLAGS})
target_link_options(test PRIVATE ${SAN_FLAGS})
