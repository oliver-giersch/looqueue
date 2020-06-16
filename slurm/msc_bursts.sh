#!/bin/sh

#SBATCH --job-name=msc_bursts
#SBATCH --time 04:00:00
#SBATCH --nodes=1
#SBATCH --partition=standard96

mkdir -p ../csv/msc/10M/throughput
cd ../cmake-build-remote-release/ || exit
./bench_throughput msc bursts > ../csv/msc/10M/throughput/bursts.csv
