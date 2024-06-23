#define MA_NO_DECODING
#define MA_NO_ENCODING
#define MINIAUDIO_IMPLEMENTATION
#include "miniaudio.h"

#include "morse.h"
#include "blackman.h"
#include "midi.h"

#include <time.h>
#include <stdint.h>

#include <pthread.h>
#ifdef _WIN64
#include <windows.h>
#endif

// #include <unistd.h>    /* for getopt */
#include <getopt.h>

#define VERSION "0.1"

#define DEVICE_FORMAT ma_format_f32
#define DEVICE_CHANNELS 1
#define DEVICE_SAMPLE_RATE 48000
#define DEVICE_FRAMES 128

#define SIN_FREQ 500
#define SIN_AMP 0.8
#define DEFAULT_WPM 25

#define RING_BUFFER_SIZE 1024

void ms_sleep(int ms)
{
#ifdef _WIN64
    Sleep(ms);
#else
    usleep(ms * 1000);
#endif
}

typedef struct straight_key_decoder_data
{
    char event;
    long long frame;
} straight_key_decoder_type;

typedef struct steight_conf
{
    // words per minute
    int wpm;
    // ringbuffer to handover dits and dahs to CW decoder
    ma_rb *p_ring_buffer;
    // sample rate
    int sample_rate;
    // frames each dit takes
    int frames_per_dit;
    // frames each dah takes
    int frames_per_dah;

} straight_conf_type;




long long get_milli_time()
{
    struct timespec t;
    clock_gettime(CLOCK_REALTIME, &t);
    return t.tv_sec * (long long)(1000) + t.tv_nsec / 1000000;
};

typedef struct dit_dah_buffer
{
    char dit_dah[DECODER_CHAR_BUFFER_SIZE];
    int position;

} dit_dah_buffer_type;

void init_dit_dah(dit_dah_buffer_type *b)
{
    memset(&(b->dit_dah), 0, sizeof(char) * DECODER_CHAR_BUFFER_SIZE);
    b->position = 0;
}
void add_dit_dah(dit_dah_buffer_type *b, char dit_dah)
{
    if(b->position >= DECODER_CHAR_BUFFER_SIZE - 1) init_dit_dah(b);
    b->dit_dah[b->position++] = dit_dah;
}
void output_dit_dah(dit_dah_buffer_type *b) 
{
    if(b->position != 0) convert_and_print_morse(b->dit_dah);
    init_dit_dah(b);
}



#define DECODER_SPEED_BUFFER_SIZE 16

typedef struct decoder_speed
{
    int frames_per_dit;
    int frames_pder_dah;
    int position;
    int speed[ DECODER_SPEED_BUFFER_SIZE ];
    
} decoder_speed_type;


void init_speed_buffer(decoder_speed_type *b) {
  memset(&(b->speed), 0, sizeof(int) * DECODER_SPEED_BUFFER_SIZE);
  b->position = 0;
}

void add_speed(decoder_speed_type *b, int speed)
{
 //   printf("(%i)",speed);
    if(b->position >= DECODER_SPEED_BUFFER_SIZE - 1) init_speed_buffer(b);
    b->speed[b->position++] = speed;
}


// decoder for straight key
void *straight_decoder_thread(void *parm)
{
    ma_rb *p_rb;
    dit_dah_buffer_type dit_dah;
    init_dit_dah(&dit_dah);
 
    decoder_speed_type speed_buffer;
    init_speed_buffer(&speed_buffer);

    straight_conf_type *conf = (straight_conf_type *)parm;
    p_rb = conf->p_ring_buffer;

    conf->frames_per_dit = samples_per_dit(conf->wpm, conf->sample_rate);
    conf->frames_per_dah = 3 * conf->frames_per_dit;

    printf("Decoder: %iwpm, sample rate: %i, dit frames: %i, dah frames: %i", conf->wpm, conf->sample_rate, conf->frames_per_dit, conf->frames_per_dah);

//#define NUM_SIG 64
//    int signal[NUM_SIG];
//    int pos = 0;

    float dit_length = -1;
    float dah_length = -1;
    // timer when the key was lifted, to check if character was finished
    long long up_milli = 0;

//    memset(&signal, 0, sizeof(int) * 64);

    long long last_down = -1;
    long long last_up = -1;

    for (;;)
    {
        void *read_pointer;
        size_t number = sizeof(straight_key_decoder_type);
        ma_rb_acquire_read(p_rb, &number, &read_pointer);
        if (number != 0 && number != sizeof(straight_key_decoder_type))
        {
            fprintf(stderr, "Could not get complete buffer for read. Size: %li but got %li", sizeof(straight_key_decoder_type), number);
            exit(-1);
        }
        if (number == sizeof(straight_key_decoder_type)) // we found a character
        {
            straight_key_decoder_type buffer = *((straight_key_decoder_type *)read_pointer);

            // memorize when we have pressed of lifted the key
            if (buffer.event == DECODER_STRAIGHT_KEY_DOWN)
                last_down = buffer.frame;
            if (buffer.event == DECODER_STRAIGHT_KEY_UP)
                last_up = buffer.frame;

            // key lifted up check now long the key was down and calculate if
            // it was dit or dah
            if (last_down > 0 && buffer.event == DECODER_STRAIGHT_KEY_UP)
            {
                // set time when to check character complete.
                // TODO: need to update once variable speed is implemented
                up_milli = get_milli_time() + 5 * dit_length_in_sec(conf->wpm) * 1000;

               

                int down_length = buffer.frame - last_down;
                add_speed(&speed_buffer, down_length);


                int dit_diff = abs(conf->frames_per_dit - down_length);
                int dah_diff = abs(conf->frames_per_dah - down_length);
                if (dit_diff < dah_diff)
                    add_dit_dah(&dit_dah,'.');
                else
                    add_dit_dah(&dit_dah,'-');
                fflush(stdout);
            }
            // key pressed check now long it was up and calculate if
            // this was long to be intra character / word space
            if (last_up > 0 && buffer.event == DECODER_STRAIGHT_KEY_DOWN)
            {
                up_milli = 0;
                int up_length = buffer.frame - last_up;
                if (up_length > 4 * conf->frames_per_dit)
                {
                    output_dit_dah(&dit_dah);
                    printf(" ");
                    fflush(stdout);
                }
                else if (up_length > 2 * conf->frames_per_dit)
                {
                    output_dit_dah(&dit_dah);
                }

            }

            ma_rb_commit_read(p_rb, sizeof(straight_key_decoder_type));
        }
        else if (up_milli > 0 && get_milli_time() > up_milli)
        {
            up_milli = 0;
            output_dit_dah(&dit_dah);
            fflush(stdout);
        }
        ms_sleep(50);
    }
}

// decoder for paddle key actions
void *paddle_decoder_thread(void *parm)
{
    ma_rb *p_rb;

    char decode_buffer[DECODE_MAX_ELEMENTS];
    int curr_pos = 0;

    p_rb = (ma_rb *)parm;
    for (;;)
    {
        void *read_pointer;
        size_t number = 1;
        ma_rb_acquire_read(p_rb, &number, &read_pointer);
        if (number == 1) // we found a character
        {
            char c = *((char *)read_pointer);
            switch (c)
            {
            case DECODER_SPACE_CHAR:
                printf(" ");
                fflush(stdout);
                curr_pos = 0;
                break;
            case DECODER_END_OF_CHAR:
                decode_buffer[curr_pos] = 0;
                convert_and_print_morse(decode_buffer);
                fflush(stdout);
                curr_pos = 0;
                break;

            default:
                if (curr_pos == DECODE_MAX_ELEMENTS - 1)
                {
                    curr_pos = 0;
                    printf("*");
                    fflush(stdout);
                }
                else
                    decode_buffer[curr_pos++] = c;
                break;
            }
            ma_rb_commit_read(p_rb, 1);
        }
        else
            ms_sleep(50); // no character in the buffer wait for short period of time
    }
}

// write a character to non-blocking ring buffer
extern inline void non_block_write(ma_rb *rb, unsigned char c)
{
    void *p_write = NULL;
    size_t number = 1;
    ma_rb_acquire_write(rb, &number, &p_write);
    MA_ASSERT(number == 1);
    ((char *)p_write)[0] = c;
    ma_rb_commit_write(rb, number);
}

extern inline void non_block_write_straight(ma_rb *rb, int e, long long f)
{
    void *p_write = NULL;
    straight_key_decoder_type buffer = {.event = e, .frame = f};

    size_t number = sizeof(straight_key_decoder_type);
    ma_rb_acquire_write(rb, &number, &p_write);
    if (number != sizeof(straight_key_decoder_type))
    {
        fprintf(stderr, "Can not aquire write!");
        exit(-1);
    }
    *((straight_key_decoder_type *)p_write) = buffer;
    ma_rb_commit_write(rb, number);
}

void straight_key_callback(ma_device *pDevice, void *pOutput, const void *pInput, ma_uint32 frameCount)
{
    call_back_data_type *userData;
    MA_ASSERT(pDevice->playback.channels = DEVICE_CHANNELS);
    userData = (call_back_data_type *)pDevice->pUserData;

    MA_ASSERT(userData->pWaveForm != NULL);
    MA_ASSERT(pDevice->playback.format == DEVICE_FORMAT);
    float *samples = (float *)pOutput;
    ma_waveform_read_pcm_frames(userData->pWaveForm, pOutput, frameCount, NULL);
    for (int i = 0; i < frameCount; i++)
    {
        // State: key uo (not pressed), register a key down --> ramp up
        if (userData->current_element == KEY_UP && (atomic_load(&(userData->key.state[DIT])) || atomic_load(&(userData->key.state[DAH]))))
        {
            userData->current_element = RAMP_UP;
            userData->envelop[RAMP_UP].playback_position = 0;
            non_block_write_straight(userData->pDecoderRb, 0, userData->sample_count);
        }
        // State: key down (pressed), but we register no key pressed --> ramp down
        if (userData->current_element == KEY_DOWN && (!atomic_load(&(userData->key.state[DIT])) && !atomic_load(&(userData->key.state[DAH]))))
        {
            userData->current_element = RAMP_DOWN;
            userData->envelop[RAMP_DOWN].playback_position = 0;
            non_block_write_straight(userData->pDecoderRb, 1, userData->sample_count);
        }
        switch (userData->current_element)
        {
        case RAMP_UP:
            samples[i] *= userData->envelop[RAMP_UP].envelop[userData->envelop[RAMP_UP].playback_position++];
            if (userData->envelop[RAMP_UP].playback_position >= userData->envelop[RAMP_UP].length)
            {
                userData->envelop[RAMP_UP].playback_position = 0;
                userData->current_element = KEY_DOWN;
            }
            break;
        case RAMP_DOWN:
            samples[i] *= userData->envelop[RAMP_DOWN].envelop[userData->envelop[RAMP_DOWN].playback_position++];
            if (userData->envelop[RAMP_DOWN].playback_position >= userData->envelop[RAMP_DOWN].length)
            {
                userData->envelop[RAMP_DOWN].playback_position = 0;
                userData->current_element = KEY_UP;
            }
            break;
        case KEY_UP:
            samples[i] = 0;
            break;
        case KEY_DOWN:
            break;
        }

        userData->sample_count++;
    }
}

void paddle_key_callback(ma_device *pDevice, void *pOutput, const void *pInput, ma_uint32 frameCount)
{
    call_back_data_type *userData;
    MA_ASSERT(pDevice->playback.channels = DEVICE_CHANNELS);
    userData = (call_back_data_type *)pDevice->pUserData;

    MA_ASSERT(userData->pWaveForm != NULL);
    MA_ASSERT(pDevice->playback.format == DEVICE_FORMAT);

    float *samples = (float *)pOutput;
    ma_waveform_read_pcm_frames(userData->pWaveForm, pOutput, frameCount, NULL);

    // apply the envolop
    int ce = userData->current_element;
    for (int i = 0; i < frameCount; i++)
    {
        // Check if a new element started:
        // check if key memory is set, but no current element
        if (ce == NONE && atomic_load(&(userData->key.memory[DIT])))
        { // DIT
            if (userData->last_element_end_frame && userData->sample_count - userData->last_element_end_frame > 7 * userData->sample_per_dit)
                non_block_write(userData->pDecoderRb, DECODER_SPACE_CHAR);
            ce = DIT;
            userData->current_element = DIT;
            userData->envelop[ce].playback_position = 0;
        }
        else
        {
            if (ce == NONE && atomic_load(&(userData->key.memory[DAH])))
            { // DAH
                if (userData->last_element_end_frame && userData->sample_count - userData->last_element_end_frame > 7 * userData->sample_per_dit)
                    non_block_write(userData->pDecoderRb, DECODER_SPACE_CHAR);
                ce = DAH;
                userData->current_element = DAH;
                userData->envelop[ce].playback_position = 0;
            }
        }
        if (ce != NONE)
        {
            // apply the envelop to the sample
            samples[i] *= userData->envelop[ce].envelop[userData->envelop[ce].playback_position++];
            // check at we at the end of the element
            if (userData->envelop[ce].playback_position == userData->envelop[ce].length)
            {
                if (userData->current_element == DIT)
                    non_block_write(userData->pDecoderRb, '.');
                else
                    non_block_write(userData->pDecoderRb, '-');
                // reset the payback position of the envolope at end of the element
                userData->envelop[ce].playback_position = 0;
                // if at the end of an element the respectve key is not pressed: delete the memory
                if (!atomic_load(&(userData->key.state[ce])))
                    atomic_store(&(userData->key.memory[ce]), 0);
                // play the opposite element is the opposite memory is set
                if (atomic_load(&(userData->key.memory[!ce])))
                    userData->current_element = !ce;
                else
                {
                    // check if memory of current element is still set
                    // if not set we are at the end of a character and stop playing elements
                    if (!atomic_load(&(userData->key.memory[ce])))
                    {
                        userData->current_element = NONE;
                        // store the table we finished the character
                        userData->last_element_end_frame = userData->sample_count;
                        non_block_write(userData->pDecoderRb, DECODER_END_OF_CHAR);
                    }
                }
            }
        }
        else
            samples[i] = 0;
        userData->sample_count++;
    }
    (void)pInput; /* Unused. */
}

void help()
{
    printf("cmorse [-w wpm] [-f frequency] [-p frames per package] [-h]\n\n");
    printf("   -w wpm --wpm wpm\n");
    printf("       Specify the speed of the keyer in words per minute(wpm).\n");
    printf("       The default speed is %dwpm.\n\n", DEFAULT_WPM);
    printf("   -f frequency --frequency frequency\n");
    printf("       Set the sidetone frequency in Hertz.\n");
    printf("       The default frequency of the sidetone is %dHz.\n\n", SIN_FREQ);
    printf("   -p frames --package frames\n");
    printf("       Specify the number of number of frames that is processed at\n");
    printf("       once by the audio subsystem. Typical values are 32,64 or 128 frames\n");
    printf("       The smaller the number of frames the lower the latency.\n");
    printf("       Default value for the number of frames is %d frames.\n\n", DEVICE_FRAMES);
    printf("   -s --straight\n");
    printf("       Staight key mode.\n\n");
    printf("   -h\n");
    printf("       Print this help text.\n\n\n");
}

typedef struct config_data
{
    int wpm;       // words per minutes
    int frequency; // frequency of sidetone
    int frames;    // frames in a audiobuffer
    char mode;     // keyer mode: Staight / Iambic B
} config_type;

void process_options(int argc, char **argv, config_type *conf)
{
    // Parameter option
    int c;
    int digit_optind = 0;
    int option_index = 0;

    conf->frames = DEVICE_FRAMES;
    conf->wpm = DEFAULT_WPM;
    conf->frequency = SIN_FREQ;
    conf->mode = IAMBIC_B;

    static struct option long_options[] = {

        {"wpm", required_argument, NULL, 'w'},
        {"frequency", required_argument, NULL, 'f'},
        {"package", required_argument, NULL, 'p'},
        {"straight", no_argument, NULL, 's'},
        {"help", no_argument, NULL, 'h'},
        {NULL, 0, NULL, 0}};
    while ((c = getopt_long(argc, argv, "w:f:p:hs",
                            long_options, &option_index)) != -1)
    {
        switch (c)
        {
        case 'w':
            conf->wpm = atoi(optarg);
            break;
        case 'f':
            conf->frequency = atoi(optarg);
            break;
        case 'p':
            conf->frames = atoi(optarg);
            break;
        case 'h':
            help();
            exit(-1);
            break;
        case 's':
            conf->mode = STRAIGHT_KEY;
            break;
        case '?':
            break;
        }
    }
}

int main(int argc, char **argv)
{
    ma_waveform sineWave;
    ma_device_config deviceConfig;
    ma_device device;
    ma_waveform_config sineWaveConfig;
    ma_rb ring_buffer;

    config_type conf;
    call_back_data_type userData;

    printf("cmorse %s - (c) 2024 by Thomas Fritzsche, DJ1TF\n\n", VERSION);

    // check if atomic int is lock free, that should be the case on all
    // major plattform (Intel/Arm etc.)
    // This can be just on a variable/instance
    atomic_int test_lock_free;
    if (!atomic_is_lock_free(&test_lock_free))
    {
        fprintf(stderr, "ERR: int is not a atomic type on this plattfrom.");
        return (-1);
    }
    // sort the morse decoder map so that bsearch can be used
    init_morse_map();

    process_options(argc, argv, &conf);

    open_midi(&userData.key);

    deviceConfig = ma_device_config_init(ma_device_type_playback);
    deviceConfig.playback.format = DEVICE_FORMAT;
    deviceConfig.playback.channels = DEVICE_CHANNELS;
    //    deviceConfig.sampleRate        = DEVICE_SAMPLE_RATE;
    if (conf.mode == STRAIGHT_KEY)
        deviceConfig.dataCallback = straight_key_callback;
    else
        deviceConfig.dataCallback = paddle_key_callback;
    deviceConfig.pUserData = &userData;
    deviceConfig.periodSizeInFrames = conf.frames;

    if (ma_device_init(NULL, &deviceConfig, &device) != MA_SUCCESS)
    {
        printf("Failed to open playback device.\n");
        return -4;
    }

    sineWaveConfig = ma_waveform_config_init(device.playback.format, device.playback.channels, device.sampleRate, ma_waveform_type_sine, SIN_AMP, conf.frequency);
    ma_waveform_init(&sineWaveConfig, &sineWave);

    printf("Audio Device Name: %s\n", device.playback.name);
    printf("Sample Rate: %d   Channels: %d   Frames: %d\n", device.sampleRate, device.playback.channels, device.playback.internalPeriodSizeInFrames);
    printf("Sidetone Frequency: %dHz   WPM: %d   \n", conf.frequency, conf.wpm);
    printf("Keyermode: %s\n", conf.mode == 'S' ? "Straight" : "Iambic B");

    atomic_store(&(userData.key.memory[DIT]), 0);
    atomic_store(&(userData.key.memory[DAH]), 0);
    atomic_store(&(userData.key.state[DIT]), 0);
    atomic_store(&(userData.key.state[DAH]), 0);

    userData.pWaveForm = &sineWave;
    userData.pDecoderRb = &ring_buffer;
    userData.sample_count = 0;
    userData.sample_per_dit = samples_per_dit(conf.wpm, device.sampleRate);
    if (conf.mode == STRAIGHT_KEY)
        init_ramp(&userData, device.sampleRate, conf.wpm);
    else
        init_envelop(&userData, device.sampleRate, conf.wpm);
    userData.current_element = NONE;
    userData.last_element_end_frame = 0;

    int memory_size = conf.mode == STRAIGHT_KEY ? sizeof(straight_key_decoder_type) * RING_BUFFER_SIZE : RING_BUFFER_SIZE;
    if (ma_rb_init(memory_size, NULL, NULL, userData.pDecoderRb) != MA_SUCCESS)
    {
        fprintf(stderr, "Failed to initialize ring buffer");
        return -1;
    }

    pthread_t thid;

    if (conf.mode == STRAIGHT_KEY)
    {
        straight_conf_type decoder_conf = {.wpm = conf.wpm, .sample_rate = device.sampleRate, .p_ring_buffer = userData.pDecoderRb};
        if (pthread_create(&thid, NULL, straight_decoder_thread, &decoder_conf) != 0)
        {
            perror("pthread_create() error");
            exit(-1);
        }
    }
    else if (pthread_create(&thid, NULL, paddle_decoder_thread, userData.pDecoderRb) != 0)
    {
        perror("pthread_create() error");
        exit(-1);
    }

    if (ma_device_start(&device) != MA_SUCCESS)
    {
        printf("Failed to start playback device.\n");
        ma_device_uninit(&device);
        exit(-2);
    }

    printf("\nPress Enter to quit...\n");
    getchar();

    printf("\nNumber of Samples: %lli\n", userData.sample_count);
    ma_device_uninit(&device);

    (void)argc;
    (void)argv;
    return 0;
}