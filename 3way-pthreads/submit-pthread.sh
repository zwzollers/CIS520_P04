#!/bin/bash
#SBATCH --job-name=3way-pthread
#SBATCH --constraint=moles
#SBATCH --nodes=1
#SBATCH --ntasks=1
#SBATCH --cpus-per-task=1
#SBATCH --mem-per-cpu=1G
#SBATCH --time=12:00:00
#SBATCH --output=pthread-%x-%j.out
#SBATCH --error=pthread-%x-%j.err

# Usage:
#   Basic:           sbatch submit-pthread.sh
#   Custom cores:    sbatch --cpus-per-task=4 submit-pthread.sh
#   Custom memory:   sbatch --cpus-per-task=4 --mem-per-cpu=512M submit-pthread.sh
#   Custom nodes:    sbatch --nodes=2 --cpus-per-task=4 --mem-per-cpu=512M submit-pthread.sh
#
# Example submissions:
#   sbatch --nodes=1 --cpus-per-task=1  --mem-per-cpu=64M   submit-pthread.sh
#   sbatch --nodes=1 --cpus-per-task=4  --mem-per-cpu=512M  submit-pthread.sh
#   sbatch --nodes=2 --cpus-per-task=4  --mem-per-cpu=1G    submit-pthread.sh
#   sbatch --nodes=4 --cpus-per-task=4  --mem-per-cpu=1G    submit-pthread.sh
#   sbatch --nodes=8 --cpus-per-task=4  --mem-per-cpu=1G    submit-pthread.sh

# Load required modules
module reset
module load CMake/3.23.1-GCCcore-11.3.0 foss/2022a OpenMPI/4.1.4-GCC-11.3.0

# Print job info for reference in output file
echo "======================================"
echo "Job ID:        $SLURM_JOB_ID"
echo "Node(s):       $SLURM_JOB_NODELIST"
echo "Num Nodes:     $SLURM_JOB_NUM_NODES"
echo "Cores/Node:    $SLURM_CPUS_ON_NODE"
echo "Total Cores:   $SLURM_CPUS_PER_TASK"
echo "Mem/CPU:       $SLURM_MEM_PER_CPU MB"
echo "======================================"

# Run the program
# $SLURM_CPUS_PER_TASK passes the allocated core count as the thread count
time ./3way-pthread ~eyv/cis520/wiki_dump.txt $SLURM_CPUS_PER_TASK $1

echo "======================================"
echo "Job complete"
echo "======================================"
