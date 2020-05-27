add_subdirectory(lib)

add_executable(bench_throughput src/bench_throughput.cpp)
target_include_directories(bench_throughput PRIVATE include/)
target_link_libraries(bench_throughput PRIVATE looqueue)
target_link_libraries(bench_throughput PRIVATE rpmalloc)
