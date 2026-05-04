#!/bin/bash
#SBATCH --job-name=openMP
#SBATCH --output=logs/omp-%x-%j.out
#SBATCH --time=12:00:00
#SBATCH --nodes=1
#SBATCH --ntasks=1
#SBATCH --cpus-per-task=1
#SBATCH --mem-per-cpu=64M
#SBATCH --constraint=moles

module reset
module load CMake/3.23.1-GCCcore-11.3.0 foss/2022a OpenMPI/4.1.4-GCC-11.3.0

echo "job_id=$SLURM_JOB_ID" 
echo "nodes=$SLURM_NNODES" 
echo "total_cores=$SLURM_NTASKS" 
echo "mem_per_cpu=${SLURM_MEM_PER_CPU}M"
echo "nodelist=$SLURM_JOB_NODELIST"

# Only prints the timing and node configs of the program, not the full output
#{ time mpirun -np $SLURM_NTASKS ./omp /homes/eyv/cis520/wiki_dump.txt > /dev/null; } 2>&1

# Prints the full output of the program, including timing and node configs
time ./omp ~eyv/cis520/wiki_dump.txt;
