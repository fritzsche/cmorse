#define MA_NO_DECODING
#define MA_NO_ENCODING
#define MINIAUDIO_IMPLEMENTATION

#include <stdio.h>
#include "miniaudio.h"
#include "blackman.h"
#include "morse.h"
#include "midi.h"

#define DEVICE_FORMAT ma_format_f32
#define DEVICE_CHANNELS 1
#define DEVICE_SAMPLE_RATE 48000
#define DEVICE_FRAMES 64

#define SIN_FREQ 500
#define SIN_AMP 1
#define WPM 20



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
    MA_ASSERT(tone_samples > ramp_samples);
    // initialize memory with 0
    memset(pOutput, 0, sizeof(double) * length);
    // set the tone samples to 1
    for (int i = 0; i < tone_samples; i++)
        pOutput[i] = 1.0;

    // QEX May/Jone 2006 3 / CW Shaping in DSP Software
    // generate ramp up
    backman_harris_step_response(pOutput, ramp_samples);
    // copy ramp up to ramp down (inverse order)
    for (int i = 0; i < tone_samples; i++)
        pOutput[tone_samples + ramp_samples - 1 - i] = pOutput[i];
    // Debug printf
    //    for (int i = 0; i < length; i++) printf("(%0.2f)", pOutput[i]);
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

    int ce = userData->current_element;

    for (int i = 0; i < frameCount; i++)
    {
        if (userData->memory[0] == 1)
        {
            samples[i] *= userData->envelop[ce].envelop[userData->envelop[ce].playback_position++];
            if (userData->envelop[ce].playback_position == userData->envelop[ce].length) {
                userData->memory[0] = 0;
                userData->envelop[ce].playback_position = 0;
            }    
        } else samples[i] = 0;

    }
    userData->sample_count += frameCount;

    (void)pInput; /* Unused. */
}

int main(int argc, char **argv)
{
    ma_waveform sineWave;
    ma_device_config deviceConfig;
    ma_device device;
    ma_waveform_config sineWaveConfig;
    call_back_data_type userData;

    open_midi(&userData.memory);

    userData.pWaveForm = &sineWave;
    userData.sample_count = 0;

    deviceConfig = ma_device_config_init(ma_device_type_playback);
    //    deviceConfig.playback.format = DEVICE_FORMAT; // DEVICE_FORMAT;
    deviceConfig.playback.channels = DEVICE_CHANNELS;
    //    deviceConfig.sampleRate        = DEVICE_SAMPLE_RATE;
    deviceConfig.dataCallback = data_callback;
    deviceConfig.pUserData = &userData;
    deviceConfig.periodSizeInFrames = DEVICE_FRAMES;

    if (ma_device_init(NULL, &deviceConfig, &device) != MA_SUCCESS)
    {
        printf("Failed to open playback device.\n");
        return -4;
    }
    printf("Device Name: %s\n", device.playback.name);

    sineWaveConfig = ma_waveform_config_init(device.playback.format, device.playback.channels, device.sampleRate, ma_waveform_type_sine, SIN_AMP, SIN_FREQ);
    printf("Sample rate: %d   Channels: %d\n", device.sampleRate, device.playback.channels);
    userData.sample_per_dit = samples_per_dit(WPM, device.sampleRate);
    ma_waveform_init(&sineWaveConfig, &sineWave);

    // setup dit and dah key envolop shapes
    int ramp_samples = samples_per_ramp(RAMP_TIME, device.sampleRate);
    int dit_length = samples_per_dit(WPM, device.sampleRate);

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

    userData.current_element = DAH;

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