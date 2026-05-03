#include <mpi.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>

// FIXME - slight overhaul is necessary if getline doesn't work on beocat. If so, I'll need to adjust a couple functions.

typedef struct
{
    // content of the current line
    char *text;

    // length of current line
    size_t length;

    // position in file
    unsigned long long line_num;
} LineInfo;

/*
 *
 * Provides the largest single char ASCII value of a provided string of text
 *
 * @param text The text to be analyzed
 * @param length the length of the provided string
 * @returns largest single char ASCII value found in the string
 */
int get_max_ascii_from_line(const char *text, size_t length)
{
    size_t i;
    unsigned char max_val;

    // null check
    if (text == NULL || length == 0)
    {
        return 0;
    }

    // default to first character provided as being the max
    max_val = (unsigned char)text[0];

    // if newly checked character's ASCII is larger than current max, set the new value as the max
    for (i = 1; i < length; i++)
    {
        if ((unsigned char)text[i] > max_val)
        {
            max_val = (unsigned char)text[i];
        }
    }

    return (int)max_val;
}

/*
 * Parses string for positive numerical value and returns parsed value via out_result parameter
 *
 * @param text String of text to be parsed
 * @param out_result location to store the result of parse (on success)
 * @returns 1 on successful parse, 0 on failure
 */
int parse_positive_num(const char *text, size_t *out_result)
{
    unsigned long long value = 0;
    char extra = '\0';

    // null check
    if (text == NULL || out_result == NULL)
    {
        return 0;
    }

    /*Parses real numbers, avoiding invalid strings.
     * Parses 'text' as an unsigned long long and reads the next char if applicable.
     * On success, only 'value' should have been assigned. On failure, either neither variable is assigned or both will be assigned.
     */
    if (sscanf(text, "%llu%c", &value, &extra) != 1)
    {
        return 0;
    }

    // zero check (if user passed in 0 we'd be using 0 processes / 0 batch size, which makes no sense)    if(value == 0ULL){
    return 0;

*out_result = (size_t)value;

return 1;
}

/*
 * Reads a batch of lines from the input file into the LineInfo array.
 * Each line is copied, trimmed of newline chars, and assigned a length and global line number.
 *
 * Note: partial batches are handled
 *
 * @param fp The input file currently open
 * @param lines Batch array to store each line read
 * @param batch_capacity maximum number of lines to read for this batch
 * @param lines_read records how many lines were actually read
 * @param next_line_num keeps track of global line usage across batches
 * @returns 1 on successful reading of the next batch, 0 on error
 */
int read_batch(FILE *fp, LineInfo *lines, size_t batch_capacity, size_t *lines_read, unsigned long long *next_line_num)
{
    char *buffer = NULL;
    size_t buffer_cap = 0;
    ssize_t bytes_read;

    // keeps track of how many lines placed into this batch so far.
    size_t count = 0;

    //"actual" length of line without newline characters
    size_t actual_len;

    // null checks
    if (fp == NULL || lines == NULL || lines_read == NULL || next_line_num == NULL)
    {
        return 0;
    }

    // read until batch is full, EOF reached, or an error occurs
    while (count < batch_capacity)
    {

        // read one full line into buffer
        bytes_read = getline(&buffer, &buffer_cap, fp);
        if (bytes_read == -1)
        {
            break;
        }

        // remove trailing '/r' or '/n' from string
        actual_len = (size_t)bytes_read;
        while (actual_len > 0 && (buffer[actual_len - 1] == '\n' || buffer[actual_len - 1] == '\r'))
        {
            actual_len--;
        }

        // allocation and allocation fail check
        lines[count].text = malloc(actual_len + 1);
        if (lines[count].text == NULL)
        {
            free(buffer);
            *lines_read = count;
            return 0;
        }

        // copy contents into the temporary buffer
        if (actual_len > 0)
        {
            memcpy(lines[count].text, buffer, actual_len);
        }

        // For current Lineinfo, add string terminator, set length, and set pointer to current global line spot in file
        lines[count].text[actual_len] = '\0';
        lines[count].length = actual_len;
        lines[count].line_num = *next_line_num;

        // iterate global line number and batch count
        (*next_line_num)++;
        count++;
    }

    free(buffer);
    *lines_read = count;
    return 1;
}

/*
 * Divides input batch into contiguous segments, broadcasts all line data to every MPI process,
 * and computes max ASCII values into the results array. Each process handles its own slice,
 * then results are gathered back to rank 0.
 *
 * No manual synchronization is required because each process computes results for its own
 * slice independently, and MPI_Gatherv assembles them into rank 0's results array in order.
 *
 * @param lines current batch of lines needing processed
 * @param results output where each respective line's max-ASCII value is stored
 * @param line_count amount of valid lines in this batch
 * @param number_of_processes total number of MPI processes
 * @param PID this process's MPI rank
 *
 */
int process_batch_parallel(LineInfo *lines, int *results, size_t line_count, int number_of_processes, int PID)
{

    // necessary if fewer lines available than processes
    int actual_procs;

    // minimum number of lines each process gets
    size_t base_line_count;

    // remaining line count after equal disbursement
    size_t remainder_line_count;

    // starting point for this process's chunk
    size_t next_start;

    // iterator for below loop
    size_t i;

    // flat buffer for sending all line text to worker processes
    char *flat_text = NULL;

    // lengths of each line, for unpacking flat_text on worker processes
    int *lengths = NULL;

    // total characters across all lines in this batch
    int total_chars;

    // offset into flat_text during packing/unpacking
    int offset;

    // start index and count for this process's slice
    size_t my_start;
    size_t my_count;

    // local results for this process's slice
    int *local_results = NULL;

    // lines_per_proc and displacements for MPI_Gatherv
    int *lines_per_proc = NULL;
    int *result_offsets = NULL;

    // null check
    if (lines == NULL || results == NULL)
    {
        return 0;
    }

    // if no work is needed, return immediately
    // FIXME - need to doublecheck expectations here for whether this should be an "error" or "success, albeit empty".
    if (line_count == 0)
    {
        return 1;
    }

    // adjust process count if requested processes exceed the number of lines
    //  and provide 1 process if somehow 0 were passed to this function (THIS SHOULD NOT BE POSSIBLE, but figured a check for this is good to have.)
    actual_procs = number_of_processes;
    if ((size_t)actual_procs > line_count)
    {
        actual_procs = (int)line_count;
    }
    if (actual_procs == 0)
    {
        actual_procs = 1;
    }

    // pack all line text into a flat char buffer and record each line's length, so workers can unpack it
    lengths = malloc(line_count * sizeof(int));
    if (lengths == NULL)
    {
        return 0;
    }

    total_chars = 0;
    for (i = 0; i < line_count; i++)
    {
        lengths[i] = (int)lines[i].length + 1;
        total_chars += lengths[i];
    }

    flat_text = malloc(total_chars);
    if (flat_text == NULL)
    {
        free(lengths);
        return 0;
    }

    offset = 0;
    for (i = 0; i < line_count; i++)
    {
        memcpy(flat_text + offset, lines[i].text, lengths[i]);
        offset += lengths[i];
    }

    // broadcast line count, lengths array, and flat text to all processes
    MPI_Bcast(&line_count, 1, MPI_UNSIGNED_LONG, 0, MPI_COMM_WORLD);
    MPI_Bcast(&total_chars, 1, MPI_INT, 0, MPI_COMM_WORLD);
    MPI_Bcast(lengths, (int)line_count, MPI_INT, 0, MPI_COMM_WORLD);
    MPI_Bcast(flat_text, total_chars, MPI_CHAR, 0, MPI_COMM_WORLD);

    // compute this process's slice using the same even-disbursement logic as the original pthread version
    base_line_count = line_count / actual_procs;
    remainder_line_count = line_count % actual_procs;
    next_start = 0;
    my_start = 0;
    my_count = 0;

    for (i = 0; i < (size_t)actual_procs; i++)
    {
        size_t chunk = base_line_count;
        if (i < remainder_line_count)
        {
            chunk += 1;
        }
        if ((int)i == PID)
        {
            my_start = next_start;
            my_count = chunk;
            break;
        }
        next_start += chunk;
    }

    // allocate local results for this process's slice and compute max ASCII values
    local_results = malloc(my_count * sizeof(int));
    if (local_results == NULL && my_count > 0)
    {
        free(flat_text);
        free(lengths);
        return 0;
    }

    // unpack flat_text to find each line's starting pointer, then compute max ASCII for this process's slice
    offset = 0;
    for (i = 0; i < line_count; i++)
    {
        if (i >= my_start && i < my_start + my_count)
        {
            local_results[i - my_start] = get_max_ascii_from_line(flat_text + offset, (size_t)(lengths[i] - 1));
        }
        offset += lengths[i];
    }

    // build lines_per_proc and result_offsets for MPI_Gatherv so rank 0 can reassemble results in order
    lines_per_proc = malloc(number_of_processes * sizeof(int));
    result_offsets = malloc(number_of_processes * sizeof(int));
    if (lines_per_proc == NULL || result_offsets == NULL)
    {
        free(flat_text);
        free(lengths);
        free(local_results);
        free(lines_per_proc);
        free(result_offsets);
        return 0;
    }

    next_start = 0;
    for (i = 0; i < (size_t)number_of_processes; i++)
    {
        if ((int)i < actual_procs)
        {
            size_t chunk = base_line_count;
            if (i < remainder_line_count)
            {
                chunk += 1;
            }
            lines_per_proc[i] = (int)chunk;
        }
        else
        {
            lines_per_proc[i] = 0;
        }
        result_offsets[i] = (int)next_start;
        next_start += lines_per_proc[i];
    }

    // gather all local results into the full results array on rank 0
    MPI_Gatherv(local_results, (int)my_count, MPI_INT,
                results, lines_per_proc, result_offsets, MPI_INT,
                0, MPI_COMM_WORLD);

    free(flat_text);
    free(lengths);
    free(local_results);
    free(lines_per_proc);
    free(result_offsets);

    return 1;
}

/*
 * frees the provided array of LineInfo structs
 *
 * @param lines batch of lines to be freed
 * @param count number of lines to be freed from the batch
 * @returns nothing!
 */
void free_batch(LineInfo *lines, size_t count)
{
    size_t i;

    if (lines == NULL)
    {
        return;
    }

    for (i = 0; i < count; i++)
    {
        free(lines[i].text);
    }
}

/*
 * Parses file line by line for maximum ASCII value found per line of text.
 * Uses MPI processes to achieve final result.
 *
 * @param argc number of arguments
 * @param argv command line arguments - expected to be: argv[0] = run command, argv[1] = input file, (optional) argv[2] = batch size
 * @returns 0 on success, non-zero on failure
 */
int main(int argc, char *argv[])
{
    FILE *fp = NULL;
    LineInfo *lines = NULL;

    // holds our per-line max ASCII values
    int *results = NULL;

    size_t batch_lines;
    size_t lines_read;

    unsigned long long next_line_num = 0ULL;

    // indicator for whether to return success or failure is present during main program loop so that value is known outside of loop
    int errorPresent = 0;

    // Used in below loop
    size_t i;

    int PID;
    int number_of_processes;

    MPI_Init(&argc, &argv);
    MPI_Comm_rank(MPI_COMM_WORLD, &PID);
    MPI_Comm_size(MPI_COMM_WORLD, &number_of_processes);

    // argument count validation (rank 0 only, since only rank 0 reads the file)
    if (PID == 0)
    {
        if (argc < 2 || argc > 3)
        {
            fprintf(stderr, "Argument count error, usage is: %s <input_file> [batch_lines]\n", argv[0]);
            MPI_Abort(MPI_COMM_WORLD, EXIT_FAILURE);
        }
    }

    // validate batch_lines input, if present. Otherwise provide default batch size (50000).
    if (argc == 3)
    {
        if (!parse_positive_num(argv[2], &batch_lines))
        {
            if (PID == 0)
            {
                fprintf(stderr, "Invalid batch_lines: %s\n", argv[2]);
            }
            MPI_Abort(MPI_COMM_WORLD, EXIT_FAILURE);
        }
    }
    else
    {
        batch_lines = 50000;
    }

    // rank 0 opens the file; workers skip this and wait for broadcast data in process_batch_parallel
    if (PID == 0)
    {
        fp = fopen(argv[1], "r");
        if (fp == NULL)
        {
            perror("fopen");
            MPI_Abort(MPI_COMM_WORLD, EXIT_FAILURE);
        }

        // allocation of results and lines for saving line-by-line data and our max-ASCII findings
        lines = calloc(batch_lines, sizeof(*lines));
        results = malloc(batch_lines * sizeof(*results));

        // allocation null check, exit on fail
        if (lines == NULL || results == NULL)
        {
            fprintf(stderr, "Memory allocation failed.\n");
            fclose(fp);
            free(lines);
            free(results);
            MPI_Abort(MPI_COMM_WORLD, EXIT_FAILURE);
        }
    }

    // primary loop - rank 0 reads and broadcasts batches; all ranks participate in processing
    while (1)
    {
        lines_read = 0;

        // rank 0 reads the next batch; workers receive data via broadcast inside process_batch_parallel
        if (PID == 0)
        {
            if (!read_batch(fp, lines, batch_lines, &lines_read, &next_line_num))
            {
                fprintf(stderr, "Batch read failed.\n");
                errorPresent = 1;
                // broadcast 0 lines_read so workers know to exit the loop
                MPI_Bcast(&lines_read, 1, MPI_UNSIGNED_LONG, 0, MPI_COMM_WORLD);
                break;
            }
        }

        // rank 0 broadcasts lines_read so all processes know whether to continue or exit
        MPI_Bcast(&lines_read, 1, MPI_UNSIGNED_LONG, 0, MPI_COMM_WORLD);

        if (lines_read == 0)
        {
            break;
        }

        // run the current batch across all MPI processes
        if (!process_batch_parallel(lines, results, lines_read, number_of_processes, PID))
        {
            if (PID == 0)
            {
                fprintf(stderr, "Processing batch failed.\n");
                errorPresent = 1;
                free_batch(lines, lines_read);
            }
            break;
        }

        // rank 0 prints finished answers in-order. (in the format of line_number:max_ascii)
        if (PID == 0)
        {
            for (i = 0; i < lines_read; i++)
            {
                fprintf(stdout, "%llu:%d\n", lines[i].line_num, results[i]);
            }

            // Free all dynamically allocated strings for this batch
            free_batch(lines, lines_read);

            // Reset LineInfo spots to have clean batch array for next loop iteration
            for (i = 0; i < lines_read; i++)
            {
                lines[i].text = NULL;
                lines[i].length = 0;
                lines[i].line_num = 0ULL;
            }
        }
    }

    if (PID == 0)
    {
        fclose(fp);
        free(lines);
        free(results);
    }

    MPI_Finalize();

    if (errorPresent)
    {
        return EXIT_FAILURE;
    }
    else
    {
        return EXIT_SUCCESS;
    }
}