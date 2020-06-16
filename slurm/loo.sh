#!/bin/sh

#SBATCH --job-name=loo
#SBATCH --time 01:00:00
#SBATCH --nodes=1
#SBATCH --partition=standard96:test

mkdir -p ../csv/loo/10M/throughput
cd ../cmake-build-remote-release/ || exit
./bench_throughput loo pairs > ../csv/loo/10M/throughput/pairs.csv
./bench_throughput loo bursts > ../csv/loo/10M/throughput/bursts.csv
