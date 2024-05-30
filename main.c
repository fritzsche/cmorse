#define MA_NO_DECODING
#define MA_NO_ENCODING
#define MINIAUDIO_IMPLEMENTATION
#include "miniaudio.h"

#include "morse.h"
#include "blackman.h"
#include "midi.h"

#define DEVICE_FORMAT ma_format_f32
#define DEVICE_CHANNELS 1
#define DEVICE_SAMPLE_RATE 48000
#define DEVICE_FRAMES 64

#define SIN_FREQ 500
#define SIN_AMP 1
#define WPM 25

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
        // start of element 
        // check if key memory is set, but no current element
        if (ce == NONE && userData->key.memory[DIT] == true)
        {  // DIT
            ce = DIT;
            userData->current_element = DIT;
            userData->envelop[ce].playback_position = 0;
        }
        else
        {
            if (ce == NONE && userData->key.memory[DAH] == true)
            { // DAH
                ce = DAH;
                userData->current_element = DAH;
                userData->envelop[ce].playback_position = 0;
            }
        }
        if (ce != NONE )
        {
            // apply the envelop
            samples[i] *= userData->envelop[ce].envelop[userData->envelop[ce].playback_position++];
            // check at we at the end of the element
            if (userData->envelop[ce].playback_position == userData->envelop[ce].length)
            {
                // if at the end of an element the respectve key is not pressed delete the memory
                if (userData->key.state[ce] == 0) userData->key.memory[ce] = 0;
                // reset the payback position of the envolope at end of the element
                userData->envelop[ce].playback_position = 0;

                if (userData->key.memory[!ce]) userData->current_element = !ce;
                else userData->current_element = NONE;
            }
        }
        else
            samples[i] = 0;
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


 /*   userData.memory[DIT] = 0;
    userData.memory[DAH] = 0;
*/
    userData.key.memory[DIT] = 0;
    userData.key.memory[DAH] = 0;

    userData.key.state[DIT] = 0;
    userData.key.state[DAH] = 0;


    open_midi(&userData.key);

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

    userData.current_element = NONE;

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