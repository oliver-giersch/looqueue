#!/bin/sh

module load cmake
module load gcc/9.3.0
module load llvm/9.0.0
module load boost/1.72.0

mkdir out

rm -r cmake-build-remote-release
mkdir cmake-build-remote-release

cd cmake-build-remote-release || exit
cmake -DCMAKE_BUILD_TYPE=Release \
      -DCMAKE_C_COMPILER=clang \
      -DCMAKE_CXX_COMPILER=clang++ \
      -G "CodeBlocks - Unix Makefiles" "$(dirname "$(pwd)")"

cd ..
cmake --build cmake-build-remote-release/ --target bench_throughput -- -j 1
