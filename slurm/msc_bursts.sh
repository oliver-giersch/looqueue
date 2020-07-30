#!/bin/sh

#SBATCH --job-name=msc_bursts
#SBATCH --time 01:00:00
#SBATCH --nodes=1
#SBATCH --partition=standard96
#SBATCH -L ansys:1

SIZE=50M
OUT_DIR=../csv/msc/$SIZE/throughput

mkdir -p $OUT_DIR
cd ../cmake-build-remote-release/benches/ || exit
./bench_throughput msc bursts $SIZE 15 > ../$OUT_DIR/bursts.csv
