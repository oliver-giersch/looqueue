#!/bin/sh

#SBATCH --job-name=faa
#SBATCH --time 01:00:00
#SBATCH --nodes=1
#SBATCH --partition=standard96:test
#SBATCH -L ansys:1

SIZE=50M
OUT_DIR=../csv/faa/$SIZE/throughput

mkdir -p $OUT_DIR
cd ../cmake-build-remote-release/benches || exit
./bench_throughput faa pairs $SIZE 15  > ../$OUT_DIR/pairs.csv
./bench_throughput faa bursts $SIZE 15 > ../$OUT_DIR/bursts.csv
./bench_throughput faa rand $SIZE 15   > ../$OUT_DIR/rand.csv
