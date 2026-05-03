#!/bin/bash
#SBATCH --job-name=3way-mpi
#SBATCH --output=logs/mpi-%j.out
#SBATCH --time=0:30:00
#SBATCH --nodes=4
#SBATCH --ntasks-per-node=4
#SBATCH --mem-per-cpu=512M
#SBATCH --constraint=moles

# run < for i in $(seq 1 20); do sbatch run.sh; done > to submit 20 jobs
module load CMake/3.23.1-GCCcore-11.3.0 foss/2022a OpenMPI/4.1.4-GCC-11.3.0

mkdir -p logs

case $SLURM_MEM_PER_CPU in
    64)   BATCH_SIZE=2000  ;;
    128)  BATCH_SIZE=5000  ;;
    512)  BATCH_SIZE=20000 ;;
    *)    BATCH_SIZE=50000 ;;
esac

echo "job_id=$SLURM_JOB_ID nodes=$SLURM_NNODES total_cores=$SLURM_NTASKS mem_per_cpu=${SLURM_MEM_PER_CPU}M batch_size=$BATCH_SIZE nodelist=$SLURM_JOB_NODELIST"

# Only prints the timing and node configs of the program, not the full output
{ time mpirun -np $SLURM_NTASKS ./3way-mpi /homes/eyv/cis520/wiki_dump.txt $BATCH_SIZE > /dev/null; } 2>&1

# Prints the full output of the program, including timing and node configs
#{ time mpirun -np $SLURM_NTASKS ./3way-mpi /homes/eyv/cis520/wiki_dump.txt $BATCH_SIZE; } 2>&1