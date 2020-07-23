#!/bin/sh

#SBATCH --job-name=loo
#SBATCH --time 00:15:00
#SBATCH --nodes=1
#SBATCH --partition=standard96:test
#SBATCH -L ansys:1

PATH=../csv/loo/1M/throughput

mkdir -p $PATH
cd ../cmake-build-remote-release/benches/ || exit
./bench_throughput loo pairs  > ../$PATH/pairs.csv
./bench_throughput loo bursts > ../$PATH/bursts.csv
./bench_throughput loo rand50 > ../$PATH/rand50.csv
./bench_throughput loo rand75 > ../$PATH/rand75.csv
