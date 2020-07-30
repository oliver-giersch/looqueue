#!/bin/sh

#SBATCH --job-name=lcr
#SBATCH --time 01:00:00
#SBATCH --nodes=1
#SBATCH --partition=standard96:test
#SBATCH -L ansys:1

SIZE=50M
OUT_DIR=../csv/lcr/$SIZE/throughput

mkdir -p $OUT_DIR
cd ../cmake-build-remote-release/benches || exit
./bench_throughput lcr pairs $SIZE 15  > ../$OUT_DIR/pairs.csv
./bench_throughput lcr bursts $SIZE 15 > ../$OUT_DIR/bursts.csv
./bench_throughput lcr rand $SIZE 15   > ../$OUT_DIR/rand.csv
