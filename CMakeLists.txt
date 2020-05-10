cmake_minimum_required(VERSION 3.12)
project(looqueue CXX)

set(CMAKE_CXX_STANDARD 17)

add_library(looqueue INTERFACE)
target_include_directories(looqueue INTERFACE include/)

add_executable(example examples/example.cpp)
target_link_libraries(example PRIVATE looqueue)
