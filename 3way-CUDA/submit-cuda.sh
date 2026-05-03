#!/bin/bash
#SBATCH --job-name=3way-cuda

#SBATCH --nodes=1
#SBATCH --ntasks=1
#SBATCH --cpus-per-task=1
#SBATCH --gres=gpu:1
#SBATCH --mem-per-cpu=1G
#SBATCH --time=12:00:00
#SBATCH --output=cuda-%x-%j.out
#SBATCH --error=cuda-%x-%j.err

# Usage:
#   Basic:              sbatch submit-cuda.sh
#   Custom memory:      sbatch --mem-per-cpu=512M submit-cuda.sh
#   Custom batch size:  sbatch submit-cuda.sh 50000
#   Custom CUDA block:  sbatch submit-cuda.sh 50000 256
#
# Example submissions:
#   sbatch --mem-per-cpu=64M    submit-cuda.sh 2000 256
#   sbatch --mem-per-cpu=128M   submit-cuda.sh 5000 256
#   sbatch --mem-per-cpu=512M   submit-cuda.sh 20000 256
#   sbatch --mem-per-cpu=1G     submit-cuda.sh 50000 256
#   sbatch --mem-per-cpu=1536M  submit-cuda.sh 50000 256
#   sbatch --mem-per-cpu=3G     submit-cuda.sh 50000 256
#


# Load required modules
module reset
module load CMake/3.23.1-GCCcore-11.3.0 foss/2022a OpenMPI/4.1.4-GCC-11.3.0 CUDA/11.7.0

# Optional arguments
BATCH_SIZE=${1:-50000}
THREADS_PER_BLOCK=${2:-256}

# Print job info for reference in output file
echo "======================================"
echo "Job ID:             $SLURM_JOB_ID"
echo "Node(s):            $SLURM_JOB_NODELIST"
echo "Num Nodes:          $SLURM_JOB_NUM_NODES"
echo "CPUs/Task:          $SLURM_CPUS_PER_TASK"
echo "Mem/CPU:            $SLURM_MEM_PER_CPU MB"
echo "GPUs:               $CUDA_VISIBLE_DEVICES"
echo "Batch Size:         $BATCH_SIZE"
echo "Threads/Block:      $THREADS_PER_BLOCK"
echo "======================================"

# Run the program
time ./3way-cuda ~eyv/cis520/wiki_dump.txt "$BATCH_SIZE" "$THREADS_PER_BLOCK"

echo "======================================"
echo "Job complete"
echo "======================================"
