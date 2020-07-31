#!/bin/sh

rm slurm*

sbatch micro/faa.sh
sbatch micro/lcr.sh
sbatch micro/loo.sh
#if [ "$0" == "--all" ]; then
#fi
