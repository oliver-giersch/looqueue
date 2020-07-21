#!/bin/sh

#SBATCH --job-name=lcr
#SBATCH --time 00:10:00
#SBATCH --nodes=1
#SBATCH --partition=standard96:test
#SBATCH -L ansys:1

mkdir -p ../csv/lcr/10M/throughput
cd ../cmake-build-remote-release/benches/ || exit
./bench_throughput lcr pairs > ../../csv/lcr/10M/throughput/pairs.csv
./bench_throughput lcr bursts > ../../csv/lcr/10M/throughput/bursts.csv
