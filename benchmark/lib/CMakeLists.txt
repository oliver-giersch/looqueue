cmake_minimum_required(VERSION 3.0)
project(rpmalloc C)

add_library(rpmalloc STATIC rpmalloc/rpmalloc/rpmalloc.c)
target_include_directories(rpmalloc PUBLIC rpmalloc/rpmalloc)
target_compile_definitions(rpmalloc PUBLIC ENABLE_PRELOAD=1 ENABLE_OVERRIDE=1)
target_compile_definitions(rpmalloc PRIVATE _GNU_SOURCE)
