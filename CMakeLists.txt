cmake_minimum_required(VERSION 3.0)
project(looqueue CXX)

set(CMAKE_CXX_STANDARD 14)
set(CMAKE_CXX_EXTENSIONS OFF)

option(USE_SEQ_CST "use only sequentially consistent atomics" OFF)

add_library(looqueue INTERFACE)
add_subdirectory(benches)

target_include_directories(looqueue INTERFACE include/)
if(USE_SEQ_CST)
    target_compile_definitions(looqueue INTERFACE USE_SEQ_CST)
endif()

add_executable(example examples/example.cpp)
target_link_libraries(example PRIVATE looqueue)
