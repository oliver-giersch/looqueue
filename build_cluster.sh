#!/bin/sh

module load \
  llvm/9.0.0 \
  gcc/9.2.0 \
  boost/1.72.0 \
  intel/19.0.5 \
  cmake || exit

rm -r cmake-build-remote-release
mkdir cmake-build-remote-release

cd cmake-build-remote-release || exit

# -DCMAKE_VERBOSE_MAKEFILES=ON
# -DCMAKE_CXX_FLAGS=--gcc-toolchain=/sw/compiler/gcc/9.3.0/skl (for clang)
cmake -DCMAKE_BUILD_TYPE=Release \
      -DCMAKE_C_COMPILER=icc \
      -DCMAKE_CXX_COMPILER=icpc \
      -DALLOCATOR=rpmalloc \
      -G "CodeBlocks - Unix Makefiles" "$(dirname "$(pwd)")" || exit

cd ..
cmake --build cmake-build-remote-release/ --target bench_throughput -- -j 1
module unload cmake gcc boost intel cmake
