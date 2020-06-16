#!/bin/sh

module load gcc/9.2.0
module load boost/1.72.0
module load llvm/9.0.0
module load cmake

rm -r cmake-build-remote-release
mkdir cmake-build-remote-release

cd cmake-build-remote-release || exit
cmake -DCMAKE_BUILD_TYPE=Release \
      -DCMAKE_C_COMPILER=/sw/tools/llvm/9.0.0/skl/assertions_disbaled/bin/clang \
      -DCMAKE_CXX_COMPILER=/sw/tools/llvm/9.0.0/skl/assertions_disbaled/bin/clang++ \
      -G "CodeBlocks - Unix Makefiles" "$(dirname "$(pwd)")"

cd ..
cmake --build cmake-build-remote-release/ --target bench_throughput -- -j 1
