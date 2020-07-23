#!/bin/sh

#SBATCH --job-name=msc_bursts
#SBATCH --time 00:45:00
#SBATCH --nodes=1
#SBATCH --partition=standard96
#SBATCH -L ansys:1

PATH=../csv/msc/1M/throughput

mkdir -p $PATH
cd ../cmake-build-remote-release/benches/ || exit
./bench_throughput msc bursts > ../$PATH/bursts.csv
