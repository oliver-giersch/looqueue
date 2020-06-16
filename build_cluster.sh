#!/bin/sh

module load gcc/9.2.0
module load boost/1.72.0
module load intel/19.0.5
module load cmake

rm -r cmake-build-remote-release
mkdir cmake-build-remote-release

cd cmake-build-remote-release || exit
cmake -DCMAKE_BUILD_TYPE=Release \
#     -DCMAKE_VERBOSE_MAKEFILE=ON \
      -DCMAKE_C_COMPILER=icc \
      -DCMAKE_CXX_COMPILER=icpc \
      -G "CodeBlocks - Unix Makefiles" "$(dirname "$(pwd)")"

cd ..
cmake --build cmake-build-remote-release/ --target bench_throughput -- -j 1
