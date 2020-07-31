#!/bin/sh

#SBATCH --job-name=lcr
#SBATCH --time 01:00:00
#SBATCH --nodes=1
#SBATCH --partition=standard96:test
#SBATCH -L ansys:1

./run_pairs_and_bursts.sh lcr 50M
