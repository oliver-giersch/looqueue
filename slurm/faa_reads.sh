#!/bin/sh

#SBATCH --job-name=faa_reads
#SBATCH --time 00:30:00
#SBATCH --nodes=1
#SBATCH --partition=standard96:test
#SBATCH -L ansys:1

SIZE=1M
OUT_DIR=../csv/faa/$SIZE/throughput

mkdir -p $OUT_DIR
cd ../cmake-build-remote-release/benches || exit
./bench_reads faa $SIZE 15 > ../$OUT_DIR/reads.csv
