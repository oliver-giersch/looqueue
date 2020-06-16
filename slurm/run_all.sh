#!/bin/sh

rm slurm*

sbatch faa.sh
sbatch lcr.sh
sbatch loo.sh
sbatch msc_pairs.sh
sbatch msc_bursts.sh
