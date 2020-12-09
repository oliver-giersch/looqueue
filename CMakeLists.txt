cmake_minimum_required(VERSION 3.16)

project(looqueue CXX)

set(CMAKE_CXX_STANDARD 17)

# set to "ON" to use sequentially consistent atomics exclusively
option(LOO_USE_SEQ_CST "use only sequentially consistent atomics" ON)

add_library(looqueue INTERFACE)
target_compile_features(looqueue INTERFACE cxx_std_17)

target_include_directories(looqueue INTERFACE include)
if(LOO_USE_SEQ_CST)
    message("using sequentially consistent ordering")
    target_compile_definitions(looqueue INTERFACE LOO_USE_SEQ_CST=1)
endif()

add_executable(example examples/example.cpp)
target_link_libraries(example PRIVATE looqueue)

find_package(Threads REQUIRED)

add_executable(test_loo test/test_loo.cpp)
target_link_libraries(test_loo PRIVATE Threads::Threads looqueue)

target_compile_options(test_loo PRIVATE "-fsanitize=leak,undefined")
target_link_options(test_loo PRIVATE "-fsanitize=leak,undefined")
