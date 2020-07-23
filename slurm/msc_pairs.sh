#!/bin/sh

#SBATCH --job-name=msc_pairs
#SBATCH --time 00:45:00
#SBATCH --nodes=1
#SBATCH --partition=standard96
#SBATCH -L ansys:1

OUT_DIR=../csv/msc/1M/throughput

mkdir -p $OUT_DIR
cd ../cmake-build-remote-release/benches/ || exit
./bench_throughput msc pairs > ../$OUT_DIR/pairs.csv
