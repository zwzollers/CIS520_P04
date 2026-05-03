#!/bin/bash

CORES=(1 2 4 8 16 32)
MEMS=(64M 128M 512M 1G 1536M 3G)
NODES=(1 2 4 8)

mkdir -p logs

# Core-style scaling
# CUDA uses one GPU, so cpus-per-task mostly affects CPU-side file reading/launch overhead,
# not the GPU kernel parallelism itself. Kept here to match the pthreads run matrix.
for run in 1 2 3; do
  for cores in "${CORES[@]}"; do
    sbatch \
      --nodes=1 \
      --ntasks=1 \
      --cpus-per-task="$cores" \
      --mem-per-cpu=1G \
      --output="logs/cuda-n1-c${cores}-m1G-run${run}-%j.out" \
      --error="logs/cuda-n1-c${cores}-m1G-run${run}-%j.err" \
      submit-cuda.sh 50000 256
  done
done

# Memory scaling
for run in 1 2 3; do
  for mem in "${MEMS[@]}"; do

    # Use smaller batches for lower-memory jobs, similar to the MPI README logic.
    if [ "$mem" = "64M" ]; then
      batch_size=2000
    elif [ "$mem" = "128M" ]; then
      batch_size=5000
    elif [ "$mem" = "512M" ]; then
      batch_size=20000
    else
      batch_size=50000
    fi

    sbatch \
      --nodes=1 \
      --ntasks=1 \
      --cpus-per-task=1 \
      --mem-per-cpu="$mem" \
      --output="logs/cuda-n1-c1-m${mem}-run${run}-%j.out" \
      --error="logs/cuda-n1-c1-m${mem}-run${run}-%j.err" \
      submit-cuda.sh "$batch_size" 256
  done
done

# Node-style scaling
# CUDA jobs generally run on one GPU attached to one node. These submissions keep
# the same node-count combinations as pthreads for comparison, but performance may
# not improve because this program only uses one GPU/process.
for run in 1 2 3; do
  for nodes in "${NODES[@]}"; do
    sbatch \
      --nodes="$nodes" \
      --ntasks=1 \
      --cpus-per-task=1 \
      --mem-per-cpu=1G \
      --output="logs/cuda-n${nodes}-c1-m1G-run${run}-%j.out" \
      --error="logs/cuda-n${nodes}-c1-m1G-run${run}-%j.err" \
      submit-cuda.sh 50000 256
  done
done
