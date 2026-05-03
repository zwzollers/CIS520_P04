#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>

//FIXME - slight overhaul is necessary if getline doesn't work on beocat. If so, I'll need to adjust a couple functions.

typedef struct {
    //content of the current line
    char *text;

    //length of current line
    size_t length;

    //position in file
    unsigned long long line_num;
} LineInfo;

//ThreadTask contains all the relevant items and info for a specific thread to complete its' job
typedef struct {
    //shared batch of lines to process
    LineInfo *lines;

    //shared batch output for placing max-ASCII results
    int *results;

    //starting index that this thread should start working on
    size_t start_idx;

    //the index ONE AFTER the last line this thread should process (E.g. if ending on 4, this should contain value of 5.)
    size_t end_idx;
} ThreadTask;

/*
*
* Provides the largest single char ASCII value of a provided string of text
*
* @param text The text to be analyzed
* @param length the length of the provided string
* @returns largest single char ASCII value found in the string
*/
int get_max_ascii_from_line(const char *text, size_t length){
    size_t i;
    unsigned char max_val;

    //null check
    if(text == NULL || length == 0){
        return 0;
    }

    //default to first character provided as being the max
    max_val = (unsigned char)text[0];

    //if newly checked character's ASCII is larger than current max, set the new value as the max
    for(i = 1; i < length; i++){
        if((unsigned char) text[i] > max_val){
            max_val = (unsigned char)text[i];
        }
    }

    return (int)max_val;
}




/*
* 
* Thread entry function that process subset of lines in parallel.
* Every thread receives a ThreadTask describing its' assigned range,
* then computes maximum ASCII value for each line in the range.
*
* No manual synchronization is required because all threads write to their respective provided ranges
* on the same shared results array (and no rewrites should occur at any position, so no hangups either).
* 
* @param arg pointer to the ThreadTask struct containing relevant work details
* @returns NULL (apparently required by pthread interface)
*/
void *worker_fn(void *arg){
    ThreadTask *task;
    size_t i;

    //populate local Threadtask with argument value(s)
    task = (ThreadTask *)arg;
    if(task == NULL || task->lines == NULL || task->results == NULL){
        return NULL;
    }

    //computes max ascii value for each line within [start_idx, end_idx) and stores results at corresponding indicies
    for(i = task->start_idx; i < task->end_idx; i++){
        task->results[i] = get_max_ascii_from_line(task->lines[i].text, task->lines[i].length);
    }

    return NULL;
}

/*
* Parses string for positive numerical value and returns parsed value via out_result parameter
*
* @param text String of text to be parsed
* @param out_result location to store the result of parse (on success)
* @returns 1 on successful parse, 0 on failure
*/
int parse_positive_num(const char *text, size_t *out_result){
    unsigned long long value = 0;
    char extra = '\0';

    //null check
    if(text == NULL || out_result == NULL){
        return 0;
    }

    /*Parses real numbers, avoiding invalid strings.
    * Parses 'text' as an unsigned long long and reads the next char if applicable.
    * On success, only 'value' should have been assigned. On failure, either neither variable is assigned or both will be assigned.
    */
    if(sscanf(text, "%llu%c", &value, &extra) != 1){
        return 0;
    }

    //zero check (if user passed in 0 we'd be using 0 threads / 0 batch size, which makes no sense)
    if(value == 0ULL){
        return 0;
    }

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
int read_batch(FILE *fp, LineInfo *lines, size_t batch_capacity, size_t *lines_read, unsigned long long *next_line_num){
    char *buffer = NULL;
    size_t buffer_cap = 0;
    ssize_t bytes_read;

    //keeps track of how many lines placed into this batch so far.
    size_t count = 0;
    
    //"actual" length of line without newline characters
    size_t actual_len;

    //null checks
    if(fp == NULL || lines == NULL || lines_read == NULL || next_line_num == NULL){
        return 0;
    }

    //read until batch is full, EOF reached, or an error occurs
    while(count < batch_capacity){

        //read one full line into buffer
        bytes_read = getline(&buffer, &buffer_cap, fp);
        if(bytes_read == -1){
            break;
        }

        //remove trailing '/r' or '/n' from string 
        actual_len = (size_t)bytes_read;
        while(actual_len > 0 && (buffer[actual_len - 1] == '\n' || buffer[actual_len - 1] == '\r')){
            actual_len--;
        }

        //allocation and allocation fail check
        lines[count].text = malloc(actual_len + 1);
        if(lines[count].text == NULL){
            free(buffer);
            *lines_read = count;
            return 0;
        }

        //copy contents into the temporary buffer
        if(actual_len > 0){
            memcpy(lines[count].text, buffer, actual_len);
        }

        //For current Lineinfo, add string terminator, set length, and set pointer to current global line spot in file
        lines[count].text[actual_len] = '\0';
        lines[count].length = actual_len;
        lines[count].line_num = *next_line_num;

        //iterate global line number and batch count
        (*next_line_num)++;
        count++;

    }

    free(buffer);
    *lines_read = count;
    return 1;

}

/*
* Divides input batch into contiguous segments, assigns each segment to a thread,
* and computes max ASCII values into the results array.
*
* No manual synchronization is required because all threads write to their respective provided ranges
* on the same shared results array (and no rewrites should occur at any position, so no hangups either).
*
* @param lines current batch of lines needing processed
* @param results output where each respective line's max-ASCII value is stored
* @param line_count amount of valid lines in this batch
* @param num_threads amount of pthread workers requested
*
*/
int process_batch_parallel(LineInfo *lines, int *results, size_t line_count, size_t num_threads){
    pthread_t *threads = NULL;
    ThreadTask *tasks = NULL;

    //necessary if fewer lines avaiable than requested threads - count 
    size_t actual_threads;

    //minimum number of lines each thread gets
    size_t base_thread_count;

    //remaining thread count after equal disbursement
    size_t remainder_thread_count;

    //starting point for next threads' chunk of info
    size_t next_start;

    //iterator for below loop
    size_t i;

    //success/fail flag for if or when a pthread creation fails
    int status_check = 1;


    //null check
    if(lines == NULL || results == NULL){
        return 0;
    }

    //if no work is needed, return immediately
    //FIXME - need to doublecheck expectations here for whether this should be an "error" or "success, albeit empty".
    if(line_count == 0){
        return 1;
    }

    //adjust thread count if requested threads exceeed the number of lines
    // and provide 1 thread if somehow 0 were passed to this function (THIS SHOULD NOT BE POSSIBLE, but figured a check for this is good to have.)
    actual_threads = num_threads;
    if(actual_threads > line_count){
        actual_threads = line_count;
    }
    if(actual_threads == 0){
        actual_threads = 1;
    }

    threads = malloc(actual_threads * sizeof(*threads));
    tasks = malloc(actual_threads * sizeof(*tasks));

    //allocation null check
    if(threads == NULL || tasks == NULL){
        free(threads);
        free(tasks);
        return 0;
    }

    //thread work distribution (how the batch is being split)
    base_thread_count = line_count / actual_threads;
    remainder_thread_count = line_count % actual_threads;
    next_start = 0;

    for(i = 0; i < actual_threads; i++){
        //give out the "extra" lines if they are less than the remainder for some even-ish disbursement
        size_t chunk = base_thread_count;
        if(i < remainder_thread_count){
            chunk += 1;
        }

        //populate task information for the current thread and move next_start to next thread's starting position
        tasks[i].lines = lines;
        tasks[i].results = results;
        tasks[i].start_idx = next_start;
        tasks[i].end_idx = next_start + chunk;
        next_start += chunk;

        //attempt to create a worker thread:
        /*
        * &threads[i] for storing the thread handle in threads[i]
        * NULL for default thread attributes
        * worker_fn is the function that the new thread will run (uses its ThreadTask to provide relevant task details)
        * &tasks[i] provides the address of this thread's ThreadTask
        */
        if(pthread_create(&threads[i], NULL, worker_fn, &tasks[i]) != 0){
            actual_threads = i;
            status_check = 0;
            break;
        }

    }

    //join all successfully created threads
    for(i = 0; i < actual_threads; i++){
        pthread_join(threads[i], NULL);
    }

    free(threads);
    free(tasks);

    return status_check;


}

/*
* frees the provided array of LineInfo structs
*
* @param lines batch of lines to be freed
* @param count number of lines to be freed from the batch
* @returns nothing!
*/
void free_batch(LineInfo *lines, size_t count){
    size_t i;

    if(lines == NULL){
        return;
    }

    for(i = 0; i < count; i++){
        free(lines[i].text);
    }
}

/*
* Parses file line by line for maximum ASCII value found per line of text.
* Uses parallel threading to achieve final result.
*
* @param argc number of arguments
* @param argv command line arguments - expected to be: argv[0] = run command, argv[1] = input file, argv[2] = number of threads, (optional) argv[3] = batch size
* @returns 0 on success, non-zero on failure
*/
int main(int argc, char *argv[]){
    FILE *fp = NULL;
    LineInfo *lines = NULL;

    //holds our per-line max ASCII values
    int *results = NULL;

    size_t num_threads;
    size_t batch_lines;
    size_t lines_read;

    unsigned long long next_line_num = 0ULL;

    //indicator for whether to return success or failure is present during main program loop so that value is known outside of loop
    int errorPresent = 0;

    //Used in below loop
    size_t i;


    
    //argument count validation
    if(argc < 3 || argc > 4){
        fprintf(stderr, "Argument count error, usage is: %s <input_file> <num_threads> [batch_lines]\n", argv[0]);
        return EXIT_FAILURE;
    }

    //validate input for number of threads
    if(!parse_positive_num(argv[2], &num_threads)){
        fprintf(stderr, "Invalid num_threads: %s\n", argv[2]);
        return EXIT_FAILURE;
    }

    //validate batch_lines input, if present. Otherwise provide default batch size (50000).
    if(argc == 4){
        if(!parse_positive_num(argv[3], &batch_lines)){
            fprintf(stderr, "Invalid batch_lines: %s\n", argv[3]);
            return EXIT_FAILURE;
        }
    }
    else{
        batch_lines = 50000;
    }

    //attempt to open the file provided in command line argument, exit on fail
    fp = fopen(argv[1], "r");
    if(fp == NULL){
        perror("fopen");
        return EXIT_FAILURE;
    }

    //allocation of results and lines for saving line-by-line data and our max-ASCII findings
    lines = calloc(batch_lines, sizeof(*lines));
    results = malloc(batch_lines * sizeof(*results));

    //allocation null check, exit on fail
    if(lines == NULL || results == NULL){
        fprintf(stderr, "Memory allocation failed.\n");
        fclose(fp);
        free(lines);
        free(results);
        return EXIT_FAILURE;
    }

    //primary loop to gather all line data from the provided file - exits on error
    while(1){
        lines_read = 0;

        //read the current batch, free and leave loop on error
        if(!read_batch(fp, lines, batch_lines, &lines_read, &next_line_num)){
            fprintf(stderr, "Batch read failed.\n");
            errorPresent = 1;
            break;
        }

        if(lines_read == 0){
            break;
        }

        //run the current batch, free and leave loop on error
        if(!process_batch_parallel(lines, results, lines_read, num_threads)){
            fprintf(stderr, "Processing batch failed.\n");
            errorPresent = 1;
            free_batch(lines, lines_read);
            break;
        }

        //print finished answers in-order. (in the format of line_number:max_ascii)
        for(i = 0; i < lines_read; i++){
            fprintf(stdout, "%llu:%d\n", lines[i].line_num, results[i]);
        }

        //Free all dynamically allocated strings for this batch
        free_batch(lines, lines_read);

        //Reset LineInfo spots to have clean batch array for next loop iteration
        for(i = 0; i < lines_read; i++){
            lines[i].text = NULL;
            lines[i].length = 0;
            lines[i].line_num = 0ULL;
        }

    }


    fclose(fp);
    free(lines);
    free(results);

    if(errorPresent){
        return EXIT_FAILURE;
    }
    else{
        return EXIT_SUCCESS;
    }

}
