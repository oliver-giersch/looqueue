#!/bin/sh

queue=$1
size=$2

parent_dir=$HOME/projects/looqueue
out_dir=$parent_dir/csv/$queue/$size/throughput

mkdir -p $out_dir
cd $HOME/cmake-build-remote-release/benches || exit
./bench_throughput $queue pairs  $size 15 > ../$out_dir/pairs.csv
./bench_throughput $queue bursts $size 15 > ../$out_dir/bursts.csv
