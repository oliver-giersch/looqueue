#!/bin/sh

#SBATCH --job-name=msc_pairs
#SBATCH --time 04:00:00
#SBATCH --nodes=1
#SBATCH --partition=standard96

mkdir -p ../csv/msc/10M/throughput
cd ../cmake-build-remote-release/benches/ || exit
./bench_throughput msc pairs > ../../csv/msc/10M/throughput/pairs.csv
