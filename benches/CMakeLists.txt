set(ALLOCATOR "system" CACHE STRING "Choose global allocator override [system|mimalloc|rpmalloc]")

# looqueue build options (ineffective)
set(LOO_USE_SEQ_CST ON)

# mimalloc build options
set(MI_BUILD_SHARED OFF)
set(MI_BUILD_OBJECT OFF)
set(MI_BUILD_TESTS  OFF)
set(MI_USE_CXX      ON)

# allocator libraries
MESSAGE("selected allocator: ${ALLOCATOR}")
if(ALLOCATOR MATCHES "mimalloc")
    add_subdirectory(lib/mimalloc)
elseif(ALLOCATOR MATCHES "rpmalloc")
    add_subdirectory(lib)
endif()

find_package(Threads REQUIRED)
find_package(Boost 1.68.0 REQUIRED thread)

add_executable(bench_throughput
        src/bench_throughput.cpp
        src/common.cpp)
target_include_directories(bench_throughput PRIVATE include/)
target_link_libraries(bench_throughput PRIVATE
        looqueue
        ${Boost_LIBRARIES}
        Threads::Threads)

if(ALLOCATOR MATCHES "mimalloc")
    target_link_libraries(bench_throughput PRIVATE mimalloc-static)
elseif(ALLOCATOR MATCHES "rpmalloc")
    target_link_libraries(bench_throughput PRIVATE rpmalloc)
endif()

target_compile_definitions(bench_throughput PRIVATE LOO_USE_SEQ_CST)
