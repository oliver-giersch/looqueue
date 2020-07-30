#!/bin/sh

#SBATCH --job-name=loo
#SBATCH --time 01:00:00
#SBATCH --nodes=1
#SBATCH --partition=standard96:test
#SBATCH -L ansys:1

SIZE=50M
OUT_DIR=../csv/loo/$SIZE/throughput

mkdir -p $OUT_DIR
cd ../cmake-build-remote-release/benches || exit
./bench_throughput loo pairs $SIZE 15  > ../$OUT_DIR/pairs.csv
./bench_throughput loo bursts $SIZE 15 > ../$OUT_DIR/bursts.csv
./bench_throughput loo rand $SIZE 15   > ../$OUT_DIR/rand.csv
