cmake_minimum_required(VERSION 3.0)
cmake_policy(SET CMP0077 NEW)

project(looqueue CXX)

set(CMAKE_CXX_STANDARD 14)

option(LOO_USE_SEQ_CST "use only sequentially consistent atomics" OFF)

add_subdirectory(benches)

add_library(looqueue INTERFACE)
target_compile_features(looqueue INTERFACE cxx_std_14)

target_include_directories(looqueue INTERFACE include)
if(LOO_USE_SEQ_CST)
    target_compile_definitions(looqueue PUBLIC LOO_USE_SEQ_CST=1)
endif()

add_executable(example examples/example.cpp)
target_link_libraries(example PRIVATE looqueue)
