cmake_minimum_required(VERSION 3.16)

project(looqueue CXX)

set(CMAKE_CXX_STANDARD 20)

# header only library
add_library(looqueue INTERFACE)
target_compile_features(looqueue INTERFACE cxx_std_20)
target_include_directories(looqueue INTERFACE include)

# example and test executables
find_package(Threads REQUIRED)

add_executable(test_loo test/test_loo.cpp)
target_link_libraries(test_loo PRIVATE Threads::Threads looqueue)
#target_compile_options(test_loo PRIVATE "-fsanitize=leak,undefined")
#target_link_options(test_loo PRIVATE "-fsanitize=leak,undefined")

add_executable(example examples/example.cpp)
target_link_libraries(example PRIVATE looqueue)
