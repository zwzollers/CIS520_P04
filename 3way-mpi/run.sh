#!/bin/bash
#SBATCH --job-name=3way-mpi
#SBATCH --output=logs/mpi-%j.out
#SBATCH --time=0:30:00
#SBATCH --nodes=1
#SBATCH --ntasks-per-node=4
#SBATCH --mem-per-cpu=1G
#SBATCH --constraint=moles

# run < for i in $(seq 1 10); do sbatch run.sh; done > to submit 10 jobs
module load CMake/3.23.1-GCCcore-11.3.0 foss/2022a OpenMPI/4.1.4-GCC-11.3.0

mkdir -p logs

echo "job_id=$SLURM_JOB_ID nodes=$SLURM_NNODES total_cores=$SLURM_NTASKS mem_per_cpu=${SLURM_MEM_PER_CPU}M nodelist=$SLURM_JOB_NODELIST"

{ time mpirun -np $SLURM_NTASKS ./3way-mpi /homes/eyv/cis520/wiki_dump.txt > logs/output_${SLURM_JOB_ID}.txt; } 2> logs/mpi-n${SLURM_NNODES}-c${SLURM_NTASKS}-m${SLURM_MEM_PER_CPU}-${SLURM_JOB_ID}.err
