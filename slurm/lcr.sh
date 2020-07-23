#!/bin/sh

#SBATCH --job-name=lcr
#SBATCH --time 00:15:00
#SBATCH --nodes=1
#SBATCH --partition=standard96:test
#SBATCH -L ansys:1

OUT_DIR=../csv/lcr/1M/throughput

mkdir -p $OUT_DIR
cd ../cmake-build-remote-release/benches/ || exit
./bench_throughput lcr pairs  > ../$OUT_DIR/pairs.csv
./bench_throughput lcr bursts > ../$OUT_DIR/bursts.csv
./bench_throughput lcr rand50 > ../$OUT_DIR/rand50.csv
./bench_throughput lcr rand75 > ../$OUT_DIR/rand75.csv
