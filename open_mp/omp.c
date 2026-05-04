#include <omp.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>

/*
 * finds the max ASCII char value in a string
 *
 * @param text String of text to be parsed
 * @returns the max value
 */
char max_in_line(char* buffer) {

    char* ptr = buffer;
    char max = 0;

    while (*ptr != '\0') {
        if (*ptr > max) {
            max = *ptr;
        }
        ptr++;
    }

    return (max);
}

/*
 * find the maximum char in each line of a file
 *
 * @param text String of text to be parsed
 * @returns the max value
 */
int main(int argc, char *argv[])
{
    // file pointer for the file being read
    FILE* fp = NULL;

    // is the process finished
    int finished = 0;

    // line buffer one per thread
    char buffer[2000];

    //
    char results[100010];

    // the global line count
    unsigned long line = 0;

    // the line that each thread is working on
    unsigned long prv_line = 0;

    // check args
    if (argc != 2) {
        fprintf(stderr, "Invalid argument count, usage: %s <input_file> \n", argv[0]);
    }

    // open the file
    fp = fopen(argv[1], "r");

    if (fp == NULL)
    {
        perror("fopen");
    }

    // set up openMP to use thread
    #pragma omp parallel private(buffer, prv_line) shared(fp, finished, line, results) num_threads(40)
    while(1) {

        // this section must be done by only 1 thread at a time to avoid incorrect line values being stored
        #pragma omp critical
        {
            finished = (EOF == fscanf(fp, "%[^\n]\n", buffer));
            if (!finished) {
                prv_line = line;
                line += 1;
            }
        }

        if (finished) {
            break;
        }

        // process line
        results[prv_line] = max_in_line(buffer);
    }

    // close file
    fclose(fp);

    // print results
    for (unsigned long i = 0; i < line; i++) {
        printf("%lu: %d\n", i, (int)results[i]);
    }
}