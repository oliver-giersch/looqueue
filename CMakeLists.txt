cmake_minimum_required(VERSION 3.16)

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
target_compile_options(test_stress PRIVATE "-fsanitize=address,leak,undefined")
target_link_options(test_stress PRIVATE "-fsanitize=address,leak,undefined")

# Either ASAN or TSAN
#target_compile_options(test_stress PRIVATE "-fsanitize=thread,leak,undefined")
#target_link_options(test_stress PRIVATE "-fsanitize=thread,leak,undefined")

add_executable(test test/test.cpp)
target_link_libraries(test PRIVATE Threads::Threads looqueue)
#target_compile_options(test PRIVATE "-fsanitize=address,undefined")
#target_link_options(test PRIVATE "-fsanitize=address,undefined")
