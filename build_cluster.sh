#!/bin/sh

module load llvm/9.0.0
module load gcc/9.2.0
module load boost/1.72.0
module load intel/19.0.5
module load cmake

rm -r cmake-build-remote-release
mkdir cmake-build-remote-release

cd cmake-build-remote-release || exit

# -DCMAKE_VERBOSE_MAKEFILES=ON
# -DCMAKE_CXX_FLAGS=--gcc-toolchain=/sw/compiler/gcc/9.3.0/skl
cmake -DCMAKE_BUILD_TYPE=Release \
      -DCMAKE_C_COMPILER=clang \
      -DCMAKE_CXX_COMPILER=clang++ \
      -DCMAKE_CXX_FLAGS=--gcc-toolchain=/sw/compiler/gcc/9.3.0/skl \
      -G "CodeBlocks - Unix Makefiles" "$(dirname "$(pwd)")"

cd ..
cmake --build cmake-build-remote-release/ --target bench_throughput -- -j 1
