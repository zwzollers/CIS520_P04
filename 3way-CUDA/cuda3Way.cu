// includes, system
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <cuda_runtime.h>

typedef struct {
    //content of the current line
    char *text;

    //length of current line
    size_t length;

    //position in file
    unsigned long long line_num;
} LineInfo;

/*
*
* Checks for CUDA errors and exits if one occurred.
*
* @param msg short description of what CUDA operation was being checked
* @returns nothing
*/
void checkCUDAError(const char *msg){
    cudaError_t err = cudaGetLastError();

    if(cudaSuccess != err){
        fprintf(stderr, "Cuda error: %s: %s.\n", msg, cudaGetErrorString(err));
        exit(EXIT_FAILURE);
    }
}

/*
*
* CUDA kernel for computing the maximum ASCII value of each line.
* Each CUDA thread handles one line.
*
* @param d_text flattened batch text buffer
* @param d_offsets starting index for each line inside d_text
* @param d_lengths length of each line
* @param d_results output max ASCII value for each line
* @param line_count number of valid lines in this batch
*/
__global__ void max_ascii_kernel(const char *d_text, const size_t *d_offsets, const size_t *d_lengths, int *d_results, size_t line_count){
    size_t idx = (size_t)(blockIdx.x * blockDim.x + threadIdx.x);

    if(idx < line_count){
        size_t start = d_offsets[idx];
        size_t length = d_lengths[idx];
        size_t i;
        unsigned char max_val = 0;

        if(length > 0){
            max_val = (unsigned char)d_text[start];

            for(i = 1; i < length; i++){
                unsigned char current = (unsigned char)d_text[start + i];

                if(current > max_val){
                    max_val = current;
                }
            }
        }

        d_results[idx] = (int)max_val;
    }
}

/*
*
* Provides the largest single char ASCII value of a provided string of text.
* This CPU version is kept mainly as a useful debugging/reference function.
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

    //zero check
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
        lines[count].text = (char *)malloc(actual_len + 1);
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
* Packs a batch of LineInfo structs into flat text, offsets, and lengths arrays.
*
* @param lines current batch of lines
* @param line_count amount of valid lines in this batch
* @param out_text location to store allocated flat text buffer
* @param out_offsets location to store allocated offsets array
* @param out_lengths location to store allocated lengths array
* @param out_total_chars location to store total number of chars in flat text buffer
* @returns 1 on success, 0 on failure
*/
int pack_batch(LineInfo *lines, size_t line_count, char **out_text, size_t **out_offsets, size_t **out_lengths, size_t *out_total_chars){
    char *flat_text = NULL;
    size_t *offsets = NULL;
    size_t *lengths = NULL;
    size_t total_chars = 0;
    size_t current_offset = 0;
    size_t i;

    if(lines == NULL || out_text == NULL || out_offsets == NULL || out_lengths == NULL || out_total_chars == NULL){
        return 0;
    }

    for(i = 0; i < line_count; i++){
        total_chars += lines[i].length;
    }

    //Allocate at least one byte so pointers remain valid even if every line is empty.
    if(total_chars == 0){
        flat_text = (char *)malloc(1);
    }
    else{
        flat_text = (char *)malloc(total_chars);
    }

    offsets = (size_t *)malloc(line_count * sizeof(*offsets));
    lengths = (size_t *)malloc(line_count * sizeof(*lengths));

    if(flat_text == NULL || offsets == NULL || lengths == NULL){
        free(flat_text);
        free(offsets);
        free(lengths);
        return 0;
    }

    for(i = 0; i < line_count; i++){
        offsets[i] = current_offset;
        lengths[i] = lines[i].length;

        if(lines[i].length > 0){
            memcpy(flat_text + current_offset, lines[i].text, lines[i].length);
            current_offset += lines[i].length;
        }
    }

    *out_text = flat_text;
    *out_offsets = offsets;
    *out_lengths = lengths;
    *out_total_chars = total_chars;

    return 1;
}

/*
* Processes one batch of lines on the GPU.
*
* @param lines current batch of lines needing processed
* @param results output where each respective line's max-ASCII value is stored
* @param line_count amount of valid lines in this batch
* @param threads_per_block CUDA threads per block
* @returns 1 on success, 0 on failure
*/
int process_batch_cuda(LineInfo *lines, int *results, size_t line_count, size_t threads_per_block){
    char *flat_text = NULL;
    size_t *offsets = NULL;
    size_t *lengths = NULL;
    size_t total_chars = 0;

    char *device_text = NULL;
    size_t *device_offsets = NULL;
    size_t *device_lengths = NULL;
    int *device_results = NULL;

    size_t text_bytes;
    size_t array_bytes;
    size_t results_bytes;

    int block_size;
    int grid_size;

    if(lines == NULL || results == NULL){
        return 0;
    }

    if(line_count == 0){
        return 1;
    }

    if(threads_per_block == 0 || threads_per_block > 1024){
        return 0;
    }

    if(!pack_batch(lines, line_count, &flat_text, &offsets, &lengths, &total_chars)){
        return 0;
    }

    text_bytes = total_chars;
    if(text_bytes == 0){
        text_bytes = 1;
    }

    array_bytes = line_count * sizeof(size_t);
    results_bytes = line_count * sizeof(int);

    if(cudaMalloc((void **)&device_text, text_bytes) != cudaSuccess){
        free(flat_text);
        free(offsets);
        free(lengths);
        return 0;
    }

    if(cudaMalloc((void **)&device_offsets, array_bytes) != cudaSuccess){
        cudaFree(device_text);
        free(flat_text);
        free(offsets);
        free(lengths);
        return 0;
    }

    if(cudaMalloc((void **)&device_lengths, array_bytes) != cudaSuccess){
        cudaFree(device_text);
        cudaFree(device_offsets);
        free(flat_text);
        free(offsets);
        free(lengths);
        return 0;
    }

    if(cudaMalloc((void **)&device_results, results_bytes) != cudaSuccess){
        cudaFree(device_text);
        cudaFree(device_offsets);
        cudaFree(device_lengths);
        free(flat_text);
        free(offsets);
        free(lengths);
        return 0;
    }

    if(cudaMemcpy(device_text, flat_text, text_bytes, cudaMemcpyHostToDevice) != cudaSuccess){
        cudaFree(device_text);
        cudaFree(device_offsets);
        cudaFree(device_lengths);
        cudaFree(device_results);
        free(flat_text);
        free(offsets);
        free(lengths);
        return 0;
    }

    if(cudaMemcpy(device_offsets, offsets, array_bytes, cudaMemcpyHostToDevice) != cudaSuccess){
        cudaFree(device_text);
        cudaFree(device_offsets);
        cudaFree(device_lengths);
        cudaFree(device_results);
        free(flat_text);
        free(offsets);
        free(lengths);
        return 0;
    }

    if(cudaMemcpy(device_lengths, lengths, array_bytes, cudaMemcpyHostToDevice) != cudaSuccess){
        cudaFree(device_text);
        cudaFree(device_offsets);
        cudaFree(device_lengths);
        cudaFree(device_results);
        free(flat_text);
        free(offsets);
        free(lengths);
        return 0;
    }

    block_size = (int)threads_per_block;
    grid_size = (int)((line_count + threads_per_block - 1) / threads_per_block);

    max_ascii_kernel<<<grid_size, block_size>>>(device_text, device_offsets, device_lengths, device_results, line_count);

    if(cudaDeviceSynchronize() != cudaSuccess){
        checkCUDAError("kernel invocation");
        cudaFree(device_text);
        cudaFree(device_offsets);
        cudaFree(device_lengths);
        cudaFree(device_results);
        free(flat_text);
        free(offsets);
        free(lengths);
        return 0;
    }

    checkCUDAError("kernel invocation");

    if(cudaMemcpy(results, device_results, results_bytes, cudaMemcpyDeviceToHost) != cudaSuccess){
        cudaFree(device_text);
        cudaFree(device_offsets);
        cudaFree(device_lengths);
        cudaFree(device_results);
        free(flat_text);
        free(offsets);
        free(lengths);
        return 0;
    }

    checkCUDAError("memcpy");

    cudaFree(device_text);
    cudaFree(device_offsets);
    cudaFree(device_lengths);
    cudaFree(device_results);

    free(flat_text);
    free(offsets);
    free(lengths);

    return 1;
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
* Uses CUDA to process each batch.
*
* @param argc number of arguments
* @param argv command line arguments - expected to be:
* argv[0] = run command, argv[1] = input file, (optional) argv[2] = batch size, (optional) argv[3] = CUDA threads per block
* @returns 0 on success, non-zero on failure
*/
int main(int argc, char *argv[]){
    FILE *fp = NULL;
    LineInfo *lines = NULL;

    //holds our per-line max ASCII values
    int *results = NULL;

    size_t batch_lines;
    size_t lines_read;
    size_t threads_per_block;

    unsigned long long next_line_num = 0ULL;

    //indicator for whether to return success or failure is present during main program loop so that value is known outside of loop
    int errorPresent = 0;

    //Used in below loop
    size_t i;

    //argument count validation
    if(argc < 2 || argc > 4){
        fprintf(stderr, "Argument count error, usage is: %s <input_file> [batch_lines] [threads_per_block]\n", argv[0]);
        return EXIT_FAILURE;
    }

    //validate batch_lines input, if present. Otherwise provide default batch size (50000).
    if(argc >= 3){
        if(!parse_positive_num(argv[2], &batch_lines)){
            fprintf(stderr, "Invalid batch_lines: %s\n", argv[2]);
            return EXIT_FAILURE;
        }
    }
    else{
        batch_lines = 50000;
    }

    //validate threads_per_block input, if present. Otherwise provide default CUDA block size.
    if(argc == 4){
        if(!parse_positive_num(argv[3], &threads_per_block)){
            fprintf(stderr, "Invalid threads_per_block: %s\n", argv[3]);
            return EXIT_FAILURE;
        }

        if(threads_per_block > 1024){
            fprintf(stderr, "Invalid threads_per_block: %s. Maximum supported value is 1024.\n", argv[3]);
            return EXIT_FAILURE;
        }
    }
    else{
        threads_per_block = 256;
    }

    //attempt to open the file provided in command line argument, exit on fail
    fp = fopen(argv[1], "r");
    if(fp == NULL){
        perror("fopen");
        return EXIT_FAILURE;
    }

    //allocation of results and lines for saving line-by-line data and our max-ASCII findings
    lines = (LineInfo *)calloc(batch_lines, sizeof(*lines));
    results = (int *)malloc(batch_lines * sizeof(*results));

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

        //run the current batch on the GPU, free and leave loop on error
        if(!process_batch_cuda(lines, results, lines_read, threads_per_block)){
            fprintf(stderr, "CUDA processing batch failed.\n");
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

    cudaDeviceReset();

    if(errorPresent){
        return EXIT_FAILURE;
    }
    else{
        return EXIT_SUCCESS;
    }
}
