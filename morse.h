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

struct key_state
{
    // DIT/DAH Memory
    int memory[2];
    // State of the key (pressed of not)
    int state[2];
};
typedef struct key_state key_state_type;

struct call_back_data
{
    // waveform to output
    ma_waveform *pWaveForm;
    // Number of samples per dit
    int sample_per_dit;
    // number of samples processed
    long long sample_count;
//    // DIT/DAH Memory
//    int memory[2];
    // key state and memory
    key_state_type key;
    // current element
    int current_element;
    // keying shape envelop data
    key_envelop_type envelop[2];
};

typedef struct call_back_data call_back_data_type;
void generate_envelope(double *, int, int, int);
double dit_length_in_sec(int);
int samples_per_dit(int, int);
int samples_per_ramp(double, int);

#endif 