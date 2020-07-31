#!/bin/sh

#SBATCH --job-name=lcr_reads
#SBATCH --time 00:30:00
#SBATCH --nodes=1
#SBATCH --partition=standard96:test
#SBATCH -L ansys:1

SIZE=1M
OUT_DIR=../csv/lcr/$SIZE/throughput

mkdir -p $OUT_DIR
cd ../cmake-build-remote-release/benches || exit
./bench_reads lcr $SIZE 15 > ../$OUT_DIR/reads.csv
