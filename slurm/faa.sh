#!/bin/sh

#SBATCH --job-name=faa
#SBATCH --time 00:15:00
#SBATCH --nodes=1
#SBATCH --partition=standard96:test
#SBATCH -L ansys:1

PATH=../csv/faa/1M/throughput

mkdir -p $PATH
cd ../cmake-build-remote-release/benches || exit
./bench_throughput faa pairs  > ../$PATH/pairs.csv
./bench_throughput faa bursts > ../$PATH/bursts.csv
./bench_throughput faa rand50 > ../$PATH/rand50.csv
./bench_throughput faa rand75 > ../$PATH/rand75.csv
