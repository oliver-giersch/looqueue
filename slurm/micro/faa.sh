#!/bin/sh

#SBATCH --job-name=faa
#SBATCH --time 01:00:00
#SBATCH --nodes=1
#SBATCH --partition=standard96:test
#SBATCH -L ansys:1

./run_pairs_and_bursts.sh faa 50M
