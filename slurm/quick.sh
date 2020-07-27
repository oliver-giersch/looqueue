#!/bin/sh

#SBATCH --job-name=quick
#SBATCH --time 00:20:00
#SBATCH --nodes=1
#SBATCH --partition=standard96:test
#SBATCH -L ansys:1

SIZE=1M
FAA_DIR=../csv/faa/$SIZE/throughput
LOO_DIR=../csv/loo/$SIZE/throughput
LCR_DIR=../csv/lcr/$SIZE/throughput

mkdir -p $FAA_DIR
mkdir -p $LOO_DIR
mkdir -p $LCR_DIR

cd ../cmake-build-remote-release/benches || exit

./bench_throughput faa pairs 1M 25  > ../$FAA_DIR/pairs.csv
./bench_throughput faa bursts 1M 25 > ../$FAA_DIR/bursts.csv
./bench_throughput faa rand 1M 25   > ../$FAA_DIR/rand.csv

./bench_throughput loo pairs 1M 25  > ../$LOO_DIR/pairs.csv
./bench_throughput loo bursts 1M 25 > ../$LOO_DIR/bursts.csv
./bench_throughput loo rand 1M 25   > ../$LOO_DIR/rand.csv

./bench_throughput lcr pairs 1M 25  > ../$LCR_DIR/pairs.csv
./bench_throughput lcr bursts 1M 25 > ../$LCR_DIR/bursts.csv
./bench_throughput lcr rand 1M 25   > ../$LCR_DIR/rand.csv
