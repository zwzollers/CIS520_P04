#include <omp.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>

typedef struct line_result {
    unsigned long long idx;
    char max;
} LineResult;

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

    // zero check (if user passed in 0 we'd be using 0 processes / 0 batch size, which makes no sense)
    if (value == 0ULL)
    {
        return 0;
    }

    *out_result = (size_t)value;

    return 1;
}

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

int main(int argc, char *argv[])
{
    FILE* fp = NULL;
    int finished = 0;
    char buffer[3000];
    char results[1100000];

    fp = fopen(argv[1], "r");
    if (fp == NULL)
    {
        perror("fopen");
    }

    if (results == NULL)
    {
        fprintf(stderr, "Memory allocation failed.\n");
        fclose(fp);
        free(results);
        return(-1);
    }

    unsigned long line = 0;
    unsigned long prv_line = 0;

    printf("starting...\n");

    #pragma omp parallel private(buffer, prv_line) shared(fp, finished, line, results) num_threads(40)
    while(1) {

        if (finished) {
            break;
        }

        #pragma omp critical
        {
            finished = (EOF == fscanf(fp, "%[^\n]\n", buffer));
            prv_line = line;
            line += 1;
        }

        if (finished) {
            break;
        }
        
        results[prv_line] = max_in_line(buffer);
    }

    fclose(fp);

    for (unsigned long i = 0; i < line; i++) {
        printf("%lu: %d\n", i, (int)results[i]);
    }
}