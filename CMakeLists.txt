cmake_minimum_required(VERSION 3.12)
project(looqueue CXX)

set(CMAKE_CXX_STANDARD 14)

add_library(looqueue INTERFACE)
target_include_directories(looqueue INTERFACE include/)