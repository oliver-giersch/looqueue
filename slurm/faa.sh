#!/bin/sh

#SBATCH --job-name=faa
#SBATCH --time 00:10:00
#SBATCH --nodes=1
#SBATCH --partition=standard96:test
#SBATCH -L ansys:1

mkdir -p ../csv/faa/10M/throughput
cd ../cmake-build-remote-release/benches || exit
./bench_throughput faa pairs > ../../csv/faa/10M/throughput/pairs.csv
./bench_throughput faa bursts > ../../csv/faa/10M/throughput/bursts.csv
./bench_throughput faa rand50 > ../../csv/faa/10M/throughput/rand50.csv
./bench_throughput faa rand75 > ../../csv/faa/10M/throughput/rand75.csv
