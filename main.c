#define MA_NO_DECODING
#define MA_NO_ENCODING
#define MINIAUDIO_IMPLEMENTATION
#include "miniaudio.h"

#include <stdio.h>

#include <conio.h>


#define DEVICE_FORMAT       ma_format_f32
#define DEVICE_CHANNELS     1
#define DEVICE_SAMPLE_RATE  48000

#define LPF_FREQ     1000
#define LPF_ORDER    3

#define WPM          20


struct audioUserData {
  ma_waveform *pWaveForm;
  ma_lpf2 *pLpf;
};

typedef struct audioUserData callBackData;

void data_callback(ma_device* pDevice, void* pOutput, const void* pInput, ma_uint32 frameCount)
{
    ma_waveform* pSineWave;
    ma_lpf2* pLpf;

    callBackData *userData;    
    MA_ASSERT(pDevice->playback.channels == DEVICE_CHANNELS);
    userData = (callBackData*)pDevice->pUserData;

    pSineWave = userData->pWaveForm;
    MA_ASSERT(pSineWave != NULL);
    pLpf = userData->pLpf;
    MA_ASSERT(pLpf != NULL);

    ma_waveform_read_pcm_frames(pSineWave, pOutput, frameCount, NULL);

    ma_lpf2_process_pcm_frames(pLpf, pOutput, pOutput, frameCount);

    (void)pInput;   /* Unused. */
}

int main(int argc, char** argv)
{
    ma_waveform sineWave;
    ma_device_config deviceConfig;
    ma_device device;
    ma_waveform_config sineWaveConfig;
    ma_lpf2_config lpfConfig;
    ma_lpf2 lpf;   

    callBackData userData;

    userData.pWaveForm = &sineWave;
    userData.pLpf = &lpf; 
 
    deviceConfig = ma_device_config_init(ma_device_type_playback);
    deviceConfig.playback.format   = DEVICE_FORMAT; //DEVICE_FORMAT;
    deviceConfig.playback.channels = DEVICE_CHANNELS;
//    deviceConfig.sampleRate        = DEVICE_SAMPLE_RATE;
    deviceConfig.dataCallback      = data_callback;
    deviceConfig.pUserData         = &userData;
    deviceConfig.periodSizeInFrames = 64;

    if (ma_device_init(NULL, &deviceConfig, &device) != MA_SUCCESS) {
        printf("Failed to open playback device.\n");
        return -4;
    }

    printf("Device Name: %s\n", device.playback.name);


   lpfConfig = ma_lpf2_config_init(device.playback.format, device.playback.channels, device.sampleRate, LPF_FREQ, 0.707);
   if( ma_lpf2_init(&lpfConfig, NULL, &lpf) != MA_SUCCESS) {
    printf("Failed to init lpf\n");
    return -6;
   }

    sineWaveConfig = ma_waveform_config_init(device.playback.format, device.playback.channels, device.sampleRate, ma_waveform_type_sine, 0.2, 500);


    ma_waveform_init(&sineWaveConfig, &sineWave);

    if (ma_device_start(&device) != MA_SUCCESS) {
        printf("Failed to start playback device.\n");
        ma_device_uninit(&device);
        return -5;
    }    

    printf("Press Enter to quit...\n");
    getchar();
    
    ma_device_uninit(&device);
    
    (void)argc;
    (void)argv;
    return 0;
}