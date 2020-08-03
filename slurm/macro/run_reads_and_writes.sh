#!/bin/sh

queue=$1
size=$2

parent_dir=$HOME/projects/looqueue
out_dir=$parent_dir/csv/$queue/$size/throughput

mkdir -p $out_dir
cd $parent_dir/cmake-build-remote-release/benches || exit
./bench_throughput $queue reads  $size 15 > ../$out_dir/reads.csv
./bench_throughput $queue writes $size 15 > ../$out_dir/writes.csv
