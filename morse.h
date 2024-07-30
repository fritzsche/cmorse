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

#ifndef WABUN
static char *morse_map[][2] = {
    // alpha
    { ".-",u8"a" },
    {"-...",u8"b"},
    {"-.-.",u8"c"},
    {"-..", u8"d"},
    {".", u8"e"},
    {"..-.",u8"f"},
    {"--.", u8"g"},
    {"....", u8"h"},
    {"..", u8"i"},
    {".---", u8"j"},
    {"-.-", u8"k"},
    {".-..", u8"l"},
    {"--", u8"m"},
    {"-.", u8"n"},
    {"---", u8"o"},
    {".--.", u8"p"},
    {"--.-", u8"q"},
    {".-.", u8"r"},
    {"...", u8"s"},
    {"-", u8"t"},
    {"..-", u8"u"},
    {"...-", u8"v"},
    {".--", u8"w"},
    {"-..-", u8"x"},
    {"-.--", u8"y"},
    {"--..", u8"z"},
    // numbers   
    {".----", u8"1"},
    {"..---", u8"2"},
    {"...--", u8"3"},
    {"....-", u8"4"},
    {".....", u8"5"},
    {"-....", u8"6"},
    {"--...", u8"7"},
    {"---..", u8"8"},
    {"----.", u8"9"},
    {"-----", u8"0"},
    // punctuation   
    {"--..--", u8","},
    {"..--..", u8"?"},
    {".-.-.-", u8"."},
    {"-...-", u8"="},
    {"-..-.", u8"/"},
    {"-.-.--", u8"!"},    
    // Deutsche Umlaute
    {".--.-", u8"ä"},
    {"---.", u8"ö"},
    {"..--", u8"ü"},
    {"...--..", u8"ß"},

    {"-.-.-", u8"<ka>"}, // Message begins / Start of work 
    {"...-.-", u8"<sk>"}, //  End of contact / End of work
    {".-.-.", u8"<ar>"}, // End of transmission / End of message
    {"-.--.", u8"<kn>"}, // Go ahead, specific named station.    
    {"........", u8"<error>"} // Error.       
   };

#else

static char *morse_map[][2] = {
    // kana
    { ".-",u8"い" },
    {".-.-",u8"ろ"},
    {"-...",u8"は"},
    {"-.-.", u8"に"},
    {"-..", u8"ほ"},
    {".",u8"へ"},
    {"..-..", u8"と"},
    {"..-.", u8"ち"},
    {"--.", u8"り"},
    {"....", u8"ぬ"},
    {"-.--.", u8"る"},
    {".---", u8"を"},
    {"-.-", u8"わ"},
    {".-..", u8"か"},
    {"--", u8"よ "},
    {"-.", u8"た"},
    {"---", u8"れ"},
    {"---.", u8"そ"},
    {".--.", u8"つ"},
    {"--.-", u8"ね"},
    {".-.", u8"な"},
    {"...", u8"ら"},
    {"-", u8"む"},
    {"..-", u8"う"},
    {"-.-.-", u8"さ"},
    {"-.-..", u8"き"},

    {"-..--", u8"ゆ"},
    {"-...-", u8"め"},   
    {"..-.-", u8"み"},
    {"--.-.", u8"し"},   
    {".--..", u8"ゑ"},
    {"--..-", u8"ひ"},   
    {"-..-.", u8"も"},
    {".---.", u8"せ"},   
    {"---.-", u8"す"},  
    {".-.-.", u8"ん"}, 

    // numbers   
    {".----", u8"1"},
    {"..---", u8"2"},
    {"...--", u8"3"},
    {"....-", u8"4"},
    {".....", u8"5"},
    {"-....", u8"6"},
    {"--...", u8"7"},
    {"---..", u8"8"},
    {"----.", u8"9"},
    {"-----", u8"0"},
    // punctuation   

    // Dakuten
    {"..", u8"゛"},
    // Handakuten
    {"..--.", u8"゜"},
    // Chōonpu
    {".--.-", u8"ー"},
    {".-.-.-", u8"、"},
    {".-.-..", u8"。"},
    {"-.--.-", u8"（"},
    {".-..-.", u8"）"},           


    {"-..---", u8"<DO>"}, // Begin of Wabun
    {"...-.", u8"<SN>"}, //  End of Wabun
    {"........", u8"<error>"} // Error.       
   };

#endif
#endif 