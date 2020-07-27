#!/bin/sh

#SBATCH --job-name=loo
#SBATCH --time 01:00:00
#SBATCH --nodes=1
#SBATCH --partition=standard96:test
#SBATCH -L ansys:1

OUT_DIR=../csv/loo/50M/throughput

mkdir -p $OUT_DIR
cd ../cmake-build-remote-release/benches || exit
./bench_throughput loo pairs 50M 15  > ../$OUT_DIR/pairs.csv
./bench_throughput loo bursts 50M 15 > ../$OUT_DIR/bursts.csv
./bench_throughput loo rand 50M 15   > ../$OUT_DIR/rand.csv
