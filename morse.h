#ifndef MORSE_H_ 
#define MORSE_H_

#include "miniaudio.h"
#include <stdatomic.h>

#define DIT 0
#define DAH 1
#define NONE -1

#define SET 1
#define UNSET 0

#define STRAIGHT_KEY 'S'
#define IAMBIC_B     'B'

#define KEY_DOWN 2    
#define KEY_UP   NONE

#define RAMP_DOWN 0    
#define RAMP_UP   1

#define RAMP_TIME 0.005
#define DECODE_MAX_ELEMENTS 10
#define DECODER_END_OF_CHAR '*'
#define DECODER_SPACE_CHAR ' '

#define DECODER_STRAIGHT_KEY_DOWN 0
#define DECODER_STRAIGHT_KEY_UP 1
#define DECODER_CHAR_BUFFER_SIZE 32

typedef struct envelop_data
{
    // envelop -1,...,+1
    double *envelop;
    // length of the envelop in frames
    int length;
    // store playback position
    int playback_position;
}  key_envelop_type;

typedef struct key_state
{
    // DIT/DAH Memory
    atomic_int memory[2];
    // State of the key (pressed of not)
    atomic_int state[2];
} key_state_type;

typedef struct call_back_data
{
    // waveform to output
    ma_waveform *pWaveForm;
    // Number of samples per dit
    int sample_per_dit;
    // number of samples processed
    long long sample_count;
    // key state and memory
    key_state_type key;
    // current element
    int current_element;
    // keying shape envelop data
    key_envelop_type envelop[2];
    // frame the last element end
    long long last_element_end_frame;
    // ringbuffer to handover dits and dahs to CW decoder
    ma_rb *pDecoderRb;
} call_back_data_type;

void generate_envelope(double *, int, int, int);
double dit_length_in_sec(int);
int samples_per_dit(int wpm, int sample_rate);
int samples_per_ramp(double, int);

void convert_and_print_morse(char *dit_dah);
void init_morse_map();


void init_envelop(call_back_data_type* userData, int sample_rate, int wpm);
void init_ramp(call_back_data_type* userData, int sample_rate, int wpm);

static char *morse_map[][2] = {
    // alpha
    { ".-","a" },
    {"-...","b"},
    {"-.-.","c"},
    {"-..", "d"},
    {".", "e"},
    {"..-.", "f"},
    {"--.", "g"},
    {"....", "h"},
    {"..", "i"},
    {".---", "j"},
    {"-.-", "k"},
    {".-..", "l"},
    {"--", "m"},
    {"-.", "n"},
    {"---", "o"},
    {".--.", "p"},
    {"--.-", "q"},
    {".-.", "r"},
    {"...", "s"},
    {"-", "t"},
    {"..-", "u"},
    {"...-", "v"},
    {".--", "w"},
    {"-..-", "x"},
    {"-.--", "y"},
    {"--..", "z"},
    // numbers   
    {".----", "1"},
    {"..---", "2"},
    {"...--", "3"},
    {"....-", "4"},
    {".....", "5"},
    {"-....", "6"},
    {"--...", "7"},
    {"---..", "8"},
    {"----.", "9"},
    {"-----", "0"},
    // punctuation   
    {"--..--", ","},
    {"..--..", "?"},
    {".-.-.-", "."},
    {"-...-", "="},
    {"-..-.", "/"},
    {"-.-.--", "!"},    
    // Deutsche Umlaute
    {".--.-", "ä"},
    {"---.", "ö"},
    {"..--", "ü"},
    {"...--..", "ß"},

    {"-.-.-", "<ka>"}, // Message begins / Start of work 
    {"...-.-", "<sk>"}, //  End of contact / End of work
    {".-.-.", "<ar>"}, // End of transmission / End of message
    {"-.--.", "<kn>"}, // Go ahead, specific named station.    
    {"........", "<error>"} // Go ahead, specific named station.       
   };

#endif 