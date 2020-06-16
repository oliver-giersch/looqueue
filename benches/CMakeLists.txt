set(USE_SEQ_CST ON)
add_subdirectory(lib)

find_package(Threads REQUIRED)
find_package(Boost 1.68.0 REQUIRED thread)

add_executable(bench_throughput
        src/bench_throughput.cpp
        src/common.cpp)
target_include_directories(bench_throughput PRIVATE include/)
target_link_libraries(bench_throughput PRIVATE
        looqueue
        rpmalloc
        ${Boost_LIBRARIES}
        Threads::Threads)
target_compile_definitions(bench_throughput PRIVATE USE_SEQ_CST)
