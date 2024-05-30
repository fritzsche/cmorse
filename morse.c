#include <stdio.h>
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
