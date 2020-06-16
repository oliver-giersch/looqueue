cmake_minimum_required(VERSION 3.0)
project(rpmalloc C)

# required for C11 atomics
set(CMAKE_C_STANDARD 11)

add_library(rpmalloc STATIC rpmalloc/rpmalloc/rpmalloc.c)
target_include_directories(rpmalloc PUBLIC rpmalloc/rpmalloc)
target_compile_definitions(rpmalloc PUBLIC ENABLE_PRELOAD=1 ENABLE_OVERRIDE=1)
# required for dlsym support
target_compile_definitions(rpmalloc PRIVATE _GNU_SOURCE)
target_link_libraries(rpmalloc PRIVATE ${CMAKE_DL_LIBS})
