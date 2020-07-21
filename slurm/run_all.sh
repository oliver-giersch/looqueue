#!/bin/sh

rm slurm*

sbatch faa.sh
sbatch lcr.sh
sbatch loo.sh
if [ "$0" == "--all" ]; then
  sbatch msc_pairs.sh
  sbatch msc_bursts.sh
fi
