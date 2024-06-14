#include <stdio.h>
#include<stdlib.h>
#include <string.h>
#include "blackman.h"
#include "miniaudio.h"

#include "morse.h"


double dit_length_in_sec(int wpm)
{
    return 1.2 / wpm;
}

int samples_per_dit(int wpm, int sample_rate)
{
    return dit_length_in_sec(wpm) * sample_rate;
}

int samples_per_ramp(double ramp_time, int sample_rate)
{
    return 2.7 * ramp_time * sample_rate;
}


void init_envelop(call_back_data_type* userData, int sample_rate, int wpm) {
    // setup dit and dah key envolop shapes
    int ramp_samples = samples_per_ramp(RAMP_TIME, sample_rate);
    int dit_length = samples_per_dit(wpm,sample_rate);

    double *pDitEnvelop = malloc(2 * dit_length * sizeof(double));
    generate_envelope(pDitEnvelop, dit_length, ramp_samples, 2 * dit_length);

    userData->envelop[DIT].envelop = pDitEnvelop;
    userData->envelop[DIT].length = 2 * dit_length;
    userData->envelop[DIT].playback_position = 0;

    double *pDahEnvelop = malloc(4 * dit_length * sizeof(double));
    generate_envelope(pDahEnvelop, 3 * dit_length, ramp_samples, 4 * dit_length);

    userData->envelop[DAH].envelop = pDahEnvelop;
    userData->envelop[DAH].length = 4 * dit_length;
    userData->envelop[DAH].playback_position = 0;
}


void generate_envelope(double *pOutput, int tone_samples, int ramp_samples, int length)
{    
    // initialize memory with 0
    memset(pOutput, 0, sizeof(double) * length);
    // set the tone samples to 1
    for (int i = 0; i < tone_samples; i++) pOutput[i] = 1.0;

    // QEX May/Jone 2006 3 / CW Shaping in DSP Software
    // generate ramp up
    backman_harris_step_response(pOutput, ramp_samples);
    // copy ramp up to ramp down (inverse order)
    for (int i = 0; i < tone_samples; i++)
        pOutput[tone_samples + ramp_samples - 1 - i] = pOutput[i];
}


int compare_sort(const void *arg1, const void *arg2)
{
    const char *a = *(char **)arg1;
    const char *b = *(char **)arg2;
    /* Compare all of both strings: */
    return strcmp(&a[0], &b[0]);
}

int compare_search(const void *key, const void *element)
{
    const char *a = (char *)key;
    const char *b = *(char **)element;
    /* Compare all of both strings: */
    return strcmp(a, &b[0]);
}

void convert_and_print_morse(char *dit_dah)
{
    size_t n = sizeof(morse_map) / sizeof(morse_map[0]);
    char **result = (char **)bsearch(dit_dah, &morse_map, n,
                                     sizeof(char *[2]), compare_search);
    if (result == NULL)
        printf("*");
    else
        printf("%s", result[1]);
    fflush(stdout);
}

void init_morse_map() {
    // sort the morse code map to run binary search later
    size_t n = sizeof(morse_map) / sizeof(morse_map[0]);
    qsort(&morse_map, n, sizeof(char *[2]), compare_sort);
}