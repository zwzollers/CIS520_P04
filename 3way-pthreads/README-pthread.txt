3way-pthread - pthreads Implementation
==============================
------------------------------
Running Manually (for testing)
------------------------------
1. Load Module:
    module load CMake/3.23.1-GCCcore-11.3.0 foss/2022a OpenMPI/4.1.4-GCC-11.3.0

2. compile with "make"

// Skeleton
    ./3way-pthread <input_file> <num_threads> [batch_size]

Example:
    ./3way-pthread /homes/eyv/cis520/wiki_dump.txt 4

Optional batch_size argument controls how many lines are read at once.
If batch_size is not provided, the program uses a default batch size of 50000.

------------------------------
Scheduling on Beocat via Slurm
------------------------------
Edit submit-pthread.sh to set your desired configuration:

    #SBATCH --nodes=<num_nodes>
    #SBATCH --ntasks=1
    #SBATCH --cpus-per-task=<num_threads>
    #SBATCH --mem-per-cpu=<memory>    (e.g. 1G, 512M, 64M)
    #SBATCH --time=<hh:mm:ss>

Submit a single job:

    sbatch submit-pthread.sh

Submit a single job with custom cores/memory:

    sbatch --cpus-per-task=4 --mem-per-cpu=512M submit-pthread.sh

Submit multiple runs for statistical analysis:

    ./run-all.sh

run-all.sh submits core scaling, memory scaling, and node scaling jobs.
Each configuration is submitted three times for repeated timing data.

------------------------------
Output
------------------------------

Output goes to one stdout file and one stderr file per job:

- pthread-%x-%j.out   node config and stdout for each single submit-pthread.sh job
- pthread-%x-%j.err   timing and error output for each single submit-pthread.sh job

When using run-all.sh, output files are placed in the logs directory:

- logs/pthread-n<num_nodes>-c<num_cores>-m<memory>-run<run_number>-%j.out
- logs/pthread-n<num_nodes>-c<num_cores>-m<memory>-run<run_number>-%j.err

Node config is printed to the stdout file. Runtime information from the time
command is printed to the stderr file. The full program output is not suppressed
in submit-pthread.sh, so each stdout file may contain the line-by-line ASCII
maximum output unless redirected elsewhere.
