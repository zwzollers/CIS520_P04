3way-cuda - CUDA Implementation
==============================
------------------------------
Running Manually (for testing)
------------------------------
1. Load Module:
    module load CMake/3.23.1-GCCcore-11.3.0 foss/2022a OpenMPI/4.1.4-GCC-11.3.0 CUDA/11.7.0

2. compile with "make"

// Skeleton
    ./3way-cuda <input_file> [batch_size] [threads_per_block]

Example:
    ./3way-cuda /homes/eyv/cis520/wiki_dump.txt

Optional batch_size argument controls how many lines are read at once.
Optional threads_per_block argument controls CUDA block size (default = 256).

When using submit-cuda.sh:
    batch_size and threads_per_block can be passed as arguments:
        sbatch submit-cuda.sh 50000 256

------------------------------
Scheduling on Beocat via Slurm
------------------------------
Edit submit-cuda.sh to set your desired configuration:

    #SBATCH --nodes=1
    #SBATCH --ntasks=1
    #SBATCH --cpus-per-task=1
    #SBATCH --gres=gpu:1
    #SBATCH --mem-per-cpu=<memory>    (e.g. 1G, 512M, 64M)
    #SBATCH --time=<hh:mm:ss>

NOTE:
CUDA jobs require GPU resources. The "--constraint=moles" flag is NOT used
for CUDA jobs (this is assumed to be okay because this caveat
was never mentioned in the instructions from what I can tell). 
If GPU jobs fail to schedule, check available GPU partitions
using:

    kstat -g
    kstat -g -l

Submit a single job:

    sbatch submit-cuda.sh

Submit a single job with custom parameters:

    sbatch submit-cuda.sh 50000 256

Submit multiple runs for statistical analysis:

    ./run-all-cuda.sh

run-all-cuda.sh submits core-style, memory, and node configurations
(similar to pthreads), though CUDA performance is primarily GPU-bound.

------------------------------
Output
------------------------------

Output goes to one stdout file and one stderr file per job:

- cuda-%x-%j.out   node config and stdout for each job
- cuda-%x-%j.err   timing and error output for each job

When using run-all-cuda.sh, output files are placed in the logs directory:

- logs/cuda-n<num_nodes>-c<num_cores>-m<memory>-run<run_number>-%j.out
- logs/cuda-n<num_nodes>-c<num_cores>-m<memory>-run<run_number>-%j.err

Node config is printed to the stdout file. Runtime information from the time
command is printed to the stderr file.

Full program output (line-by-line ASCII results) is printed to stdout and may
be large unless redirected.

------------------------------
Notes
------------------------------

CUDA performance differs significantly from pthreads, MPI, and OpenMP:

- Increasing CPU cores has minimal impact on runtime
- Performance is dominated by GPU execution and memory transfer
- Node scaling does not improve performance (single GPU usage)
- Some variability may occur due to GPU scheduling and initialization overhead

CUDA is most effective for large, compute-heavy workloads with high parallelism.