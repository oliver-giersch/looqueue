#!/bin/sh

#SBATCH --job-name=faa_reads
#SBATCH --time 00:30:00
#SBATCH --nodes=1
#SBATCH --partition=standard96:test
#SBATCH -L ansys:1

./run_reads.sh faa 10M
