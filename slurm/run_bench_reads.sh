#!/bin/sh

rm slurm*

sbatch faa_reads.sh
sbatch lcr_reads.sh
sbatch loo_reads.sh

#if [ "$0" == "--all" ]; then
  #sbatch msc_pairs.sh
  #sbatch msc_bursts.sh
#fi
