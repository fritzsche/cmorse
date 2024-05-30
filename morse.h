#ifndef MORSE_H_ 
#define MORSE_H_

#include "miniaudio.h"

#define DIT 0
#define DAH 1
#define NONE -1
#define RAMP_TIME 0.005

struct envelop_data
{
    // envelop -1,...,+1
    double *envelop;
    // length of the envelop in frames
    int length;
    // store playback position
    int playback_position;
};

typedef struct envelop_data key_envelop_type;

struct call_back_data
{
    // waveform to output
    ma_waveform *pWaveForm;
    // Number of samples per dit
    int sample_per_dit;
    // number of samples processed
    long long sample_count;
    // DIT/DAH Memory
    int memory[2];
    // current element
    int current_element;
    // keying shape envelop data
    key_envelop_type envelop[2];
};

typedef struct call_back_data call_back_data_type;


#endif 