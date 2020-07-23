#!/bin/sh

#SBATCH --job-name=faa
#SBATCH --time 00:10:00
#SBATCH --nodes=1
#SBATCH --partition=standard96:test
#SBATCH -L ansys:1

OUT_DIR=../csv/faa/1M/throughput

mkdir -p $OUT_DIR
cd ../cmake-build-remote-release/benches || exit
./bench_throughput faa pairs  > ../$OUT_DIR/pairs.csv
./bench_throughput faa bursts > ../$OUT_DIR/bursts.csv
./bench_throughput faa rand50 > ../$OUT_DIR/rand50.csv
./bench_throughput faa rand75 > ../$OUT_DIR/rand75.csv
