#!/bin/bash

CORES=(1 2 4 8 16 32)
MEMS=(64M 128M 512M 1G 1536M 3G)
NODES=(1 2 4 8)

mkdir -p logs

# Core scaling
for run in 1 2 3; do
  for cores in "${CORES[@]}"; do
    sbatch \
      --nodes=1 \
      --ntasks=1 \
      --cpus-per-task="$cores" \
      --mem-per-cpu=1G \
      --output="logs/pthread-n1-c${cores}-m1G-run${run}-%j.out" \
      --error="logs/pthread-n1-c${cores}-m1G-run${run}-%j.err" \
      run.sh
  done
done

# Memory scaling
for run in 1 2 3; do
  for mem in "${MEMS[@]}"; do
    sbatch \
      --nodes=1 \
      --ntasks=1 \
      --cpus-per-task=4 \
      --mem-per-cpu="$mem" \
      --output="logs/pthread-n1-c4-m${mem}-run${run}-%j.out" \
      --error="logs/pthread-n1-c4-m${mem}-run${run}-%j.err" \
      run.sh
  done
done

# Node scaling
for run in 1 2 3; do
  for nodes in "${NODES[@]}"; do
    sbatch \
      --nodes="$nodes" \
      --ntasks-per-node=1 \
      --cpus-per-task=4 \
      --mem-per-cpu=1G \
      --output="logs/pthread-n${nodes}-c4-m1G-run${run}-%j.out" \
      --error="logs/pthread-n${nodes}-c4-m1G-run${run}-%j.err" \
      run.sh
  done
done