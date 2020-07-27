#!/bin/sh

#SBATCH --job-name=loo
#SBATCH --time 01:00:00
#SBATCH --nodes=1
#SBATCH --partition=standard96:test
#SBATCH -L ansys:1

OUT_DIR=../csv/loo/100M/throughput

mkdir -p $OUT_DIR
cd ../cmake-build-remote-release/benches/ || exit
./bench_throughput loo pairs  > ../$OUT_DIR/pairs.csv
./bench_throughput loo bursts > ../$OUT_DIR/bursts.csv
./bench_throughput loo rand50 > ../$OUT_DIR/rand50.csv
./bench_throughput loo rand75 > ../$OUT_DIR/rand75.csv
