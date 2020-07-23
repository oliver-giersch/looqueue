#!/bin/sh

#SBATCH --job-name=lcr
#SBATCH --time 00:15:00
#SBATCH --nodes=1
#SBATCH --partition=standard96:test
#SBATCH -L ansys:1

PATH=../csv/lcr/1M/throughput

mkdir -p $PATH
cd ../cmake-build-remote-release/benches/ || exit
./bench_throughput lcr pairs  > ../$PATH/pairs.csv
./bench_throughput lcr bursts > ../$PATH/bursts.csv
./bench_throughput lcr rand50 > ../$PATH/rand50.csv
./bench_throughput lcr rand75 > ../$PATH/rand75.csv
