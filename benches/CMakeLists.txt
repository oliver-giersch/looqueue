# looqueue build options (ineffective)
# set(LOO_USE_SEQ_CST ON)

# mimalloc build options
set(MI_BUILD_SHARED OFF)
set(MI_BUILD_OBJECT OFF)
set(MI_BUILD_TESTS  OFF)
add_subdirectory(lib/mimalloc)

find_package(Threads REQUIRED)
find_package(Boost 1.68.0 REQUIRED thread)

add_executable(bench_throughput
        src/bench_throughput.cpp
        src/common.cpp)
target_include_directories(bench_throughput PRIVATE include/)
target_link_libraries(bench_throughput PRIVATE
        looqueue
        mimalloc-static
        ${Boost_LIBRARIES}
        Threads::Threads)
target_compile_definitions(bench_throughput PRIVATE LOO_USE_SEQ_CST)
