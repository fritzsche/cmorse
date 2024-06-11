#define MA_NO_DECODING
#define MA_NO_ENCODING
#define MINIAUDIO_IMPLEMENTATION
#include "miniaudio.h"

#include "morse.h"
#include "blackman.h"
#include "midi.h"

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
#define DEVICE_FRAMES 64

#define SIN_FREQ 500
#define SIN_AMP 0.8
#define WPM 25

#define RING_BUFFER_SIZE 1024


void *decoder_pthreads(void *parm)
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
        else // no character in the buffer wait for short period of time
        {
#ifdef _WIN64
            Sleep(50);
#else
            usleep(50000);
#endif
        }
    }
}

// write a character to non-blocking ring buffer
extern inline void non_block_write(ma_rb *rb, char c)
{
    void *p_write = NULL;
    size_t number = 1;
    ma_rb_acquire_write(rb, &number, &p_write);
    MA_ASSERT(number == 1);
    //   char c = *result[1];
    ((char *)p_write)[0] = c;
    ma_rb_commit_write(rb, number);
}

void data_callback(ma_device *pDevice, void *pOutput, const void *pInput, ma_uint32 frameCount)
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
            if (userData->last_end && userData->sample_count - userData->last_end > 7 * userData->sample_per_dit)
                non_block_write(userData->pDecoderRb, DECODER_SPACE_CHAR);
            ce = DIT;
            userData->current_element = DIT;
            userData->envelop[ce].playback_position = 0;
        }
        else
        {
            if (ce == NONE && atomic_load(&(userData->key.memory[DAH])))
            { // DAH
                if (userData->last_end && userData->sample_count - userData->last_end > 7 * userData->sample_per_dit)
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
                        userData->last_end = userData->sample_count;
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

int main(int argc, char **argv)
{
    ma_waveform sineWave;
    ma_device_config deviceConfig;
    ma_device device;
    ma_waveform_config sineWaveConfig;
    ma_rb ring_buffer;

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

    init_morse_map();
    // Parameter option
    int c;
    int digit_optind = 0;
    int option_index = 0;

    int frames_per_pack = DEVICE_FRAMES;
    int wpm = WPM;
    int frequency = SIN_FREQ;

    static struct option long_options[] = {
        /*   NAME       ARGUMENT           FLAG  SHORTNAME */
        {"wpm", required_argument, NULL, 'w'},
        {"frequency", required_argument, NULL, 'f'},
        {"package", required_argument, NULL, 'p'},
        {NULL, 0, NULL, 0}};
    while ((c = getopt_long(argc, argv, "w:f:p:", // abc:d:012
                            long_options, &option_index)) != -1)
    {
        switch (c)
        {
        case 'w':
            wpm = atoi(optarg);
            break;
        case 'f':
            frequency = atoi(optarg);
            break;
        case 'p':
            frames_per_pack = atoi(optarg);
            break;
        case '?':
            break;
        }
    }

    atomic_store(&(userData.key.memory[DIT]), 0);
    atomic_store(&(userData.key.memory[DAH]), 0);

    atomic_store(&(userData.key.state[DIT]), 0);
    atomic_store(&(userData.key.state[DAH]), 0);

    open_midi(&userData.key);

    userData.pWaveForm = &sineWave;
    userData.pDecoderRb = &ring_buffer;
    userData.sample_count = 0;

    deviceConfig = ma_device_config_init(ma_device_type_playback);
    deviceConfig.playback.format = DEVICE_FORMAT; // DEVICE_FORMAT;
    deviceConfig.playback.channels = DEVICE_CHANNELS;
    //    deviceConfig.sampleRate        = DEVICE_SAMPLE_RATE;
    deviceConfig.dataCallback = data_callback;
    deviceConfig.pUserData = &userData;
    deviceConfig.periodSizeInFrames = frames_per_pack;

    if (ma_device_init(NULL, &deviceConfig, &device) != MA_SUCCESS)
    {
        printf("Failed to open playback device.\n");
        return -4;
    }
    printf("Device Name: %s\n", device.playback.name);

    sineWaveConfig = ma_waveform_config_init(device.playback.format, device.playback.channels, device.sampleRate, ma_waveform_type_sine, SIN_AMP, frequency);
    printf("Sample rate: %d   Channels: %d\n", device.sampleRate, device.playback.channels);
    userData.sample_per_dit = samples_per_dit(wpm, device.sampleRate);
    ma_waveform_init(&sineWaveConfig, &sineWave);

    // setup dit and dah key envolop shapes
    int ramp_samples = samples_per_ramp(RAMP_TIME, device.sampleRate);
    int dit_length = samples_per_dit(wpm, device.sampleRate);

    double *pDitEnvelop = malloc(2 * dit_length * sizeof(double));
    generate_envelope(pDitEnvelop, dit_length, ramp_samples, 2 * dit_length);

    double *pDahEnvelop = malloc(4 * dit_length * sizeof(double));
    generate_envelope(pDahEnvelop, 3 * dit_length, ramp_samples, 4 * dit_length);

    userData.envelop[DIT].envelop = pDitEnvelop;
    userData.envelop[DIT].length = 2 * dit_length;
    userData.envelop[DIT].playback_position = 0;

    userData.envelop[DAH].envelop = pDahEnvelop;
    userData.envelop[DAH].length = 4 * dit_length;
    userData.envelop[DAH].playback_position = 0;
    userData.current_element = NONE;
    userData.last_end = 0;

    if (ma_rb_init(RING_BUFFER_SIZE, NULL, NULL, userData.pDecoderRb) != MA_SUCCESS)
    {
        fprintf(stderr, "Failed to initialize ring buffer");
        return -1;
    }

    pthread_t thid;

    if (pthread_create(&thid, NULL, decoder_pthreads, userData.pDecoderRb) != 0)
    {
        perror("pthread_create() error");
        exit(1);
    }

    if (ma_device_start(&device) != MA_SUCCESS)
    {
        printf("Failed to start playback device.\n");
        ma_device_uninit(&device);
        return -5;
    }

    printf("Press Enter to quit...\n");
    getchar();

    printf("Number of Samples: %lli\n", userData.sample_count);
    ma_device_uninit(&device);

    (void)argc;
    (void)argv;
    return 0;
}