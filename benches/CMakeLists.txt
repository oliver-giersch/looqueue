cmake_policy(SET CMP0077 NEW)

set(ALLOCATOR "system" CACHE STRING "Choose global allocator override [system|mimalloc|rpmalloc]")
option(STATIC "using static linking" OFF)

if(STATIC)
    MESSAGE("using static linking")
    set(Boost_USE_STATIC_LIBS ON)
endif()

# looqueue build options (ineffective)
set(LOO_USE_SEQ_CST ON)

# mimalloc build options
set(MI_BUILD_SHARED OFF)
set(MI_BUILD_OBJECT OFF)
set(MI_BUILD_TESTS  OFF)
set(MI_USE_CXX      OFF)

# allocator libraries
MESSAGE("using override allocator: ${ALLOCATOR}")
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
target_include_directories(bench_throughput PRIVATE include)
target_compile_definitions(bench_throughput PRIVATE LOO_USE_SEQ_CST)
target_link_libraries(bench_throughput PRIVATE
        looqueue
        ${Boost_LIBRARIES}
        Threads::Threads)

if(ALLOCATOR MATCHES "mimalloc")
    target_link_libraries(bench_throughput PRIVATE mimalloc-static)
elseif(ALLOCATOR MATCHES "rpmalloc")
    target_link_libraries(bench_throughput PRIVATE rpmalloc)
endif()

if(STATIC)
    target_link_libraries(bench_throughput PRIVATE -static -static-libgcc -static-libstdc++)
endif()

add_executable(test_lcrq src/test_lcrq.cpp)
target_include_directories(test_lcrq PRIVATE include)
target_link_libraries(test_lcrq PRIVATE
        Threads::Threads
        ${Boost_LIBRARIES}
        looqueue)

target_compile_options(test_lcrq PRIVATE -fsanitize=address)
target_link_options(test_lcrq PRIVATE -fsanitize=address)
