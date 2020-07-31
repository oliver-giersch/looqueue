#!/bin/sh

rm slurm*

sbatch reads/faa.sh
sbatch reads/lcr.sh
sbatch reads/loo.sh
#if [ "$0" == "--all" ]; then
#fi

