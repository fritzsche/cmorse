#define MA_NO_DECODING
#define MA_NO_ENCODING
#define MINIAUDIO_IMPLEMENTATION
#include <stdio.h>
#include "miniaudio.h"
#include "blackman.h"


#define DEVICE_FORMAT       ma_format_f32
#define DEVICE_CHANNELS     1
#define DEVICE_SAMPLE_RATE  48000

#define LPF_FREQ     1000
#define LPF_ORDER    3
#define SIN_FREQ     500
#define SIN_AMP      1
#define WPM          20

#define RAMP_TIME   0.005


 #define max(a,b) \
   ({ __typeof__ (a) _a = (a); \
       __typeof__ (b) _b = (b); \
     _a > _b ? _a : _b; })

struct audioUserData {
  ma_waveform *pWaveForm;
  ma_lpf2 *pLpf;  
  uint32_t sample_per_dit;
  double *ramp;
  int ramp_samples;
  // number of samples processed
  long long sample_count;
};

typedef struct audioUserData callBackData;



double dit_length_in_sec(int wpm) {
    return  6.0 / ( 5.0 * wpm );
}

int samples_per_dit(int wpm, int sample_rate) {
    return dit_length_in_sec(wpm) * sample_rate;
}

void data_callback(ma_device* pDevice, void* pOutput, const void* pInput, ma_uint32 frameCount)
{
    callBackData *userData;    
    MA_ASSERT(pDevice->playback.channels = DEVICE_CHANNELS);
    userData = (callBackData*)pDevice->pUserData;

    MA_ASSERT(userData->pWaveForm != NULL);
    MA_ASSERT(userData->pLpf != NULL);
    MA_ASSERT(pDevice->playback.format == DEVICE_FORMAT);

   
   float *samples = (float *) pOutput;

    ma_waveform_read_pcm_frames(userData->pWaveForm, pOutput, frameCount, NULL);


    if (userData->sample_count < userData->ramp_samples ) {
        for(int i = 0; i<frameCount; i++) {
                
            if (userData->sample_count + i < userData->ramp_samples) {
              printf("\n%i/%i\n",i,i+userData->sample_count);     
              samples[i] *= userData->ramp[userData->sample_count+i];
              printf("(%f/%f)",samples[i],userData->ramp[userData->sample_count+i]);         
            }
//            if (i >= frameCount) exit;
//            printf("(%f)",samples[i-userData->sample_count]);            
//            samples[i-userData->sample_count] *= userData->ramp[i];
//            printf("|%f|",samples[i-userData->sample_count]);
        }

    }
 //   MA_ASSERT(1 == 0);
//    ma_lpf2_process_pcm_frames(userData->pLpf, pOutput, pOutput, frameCount);
    userData->sample_count += frameCount;

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
    userData.sample_count = 0;
 
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

    sineWaveConfig = ma_waveform_config_init(device.playback.format, device.playback.channels, device.sampleRate, ma_waveform_type_sine, SIN_AMP, SIN_FREQ);

    printf("Sample rate: %d   Channels: %d\n",device.sampleRate, device.playback.channels);

 //   double dit_len_in_s = 6.0 / ( 5.0 * WPM );
 //   double dit_len_in_s = dit_length_in_sec( WPM );
 //   double sample_len_in_s = 1.0 / device.sampleRate;

    userData.sample_per_dit = samples_per_dit(WPM,device.sampleRate);//dit_len_in_s / sample_len_in_s;

    ma_waveform_init(&sineWaveConfig, &sineWave);

// QEX May/Jone 2006 3 / CW Shaping in DSP Software
    int ramp_samples = 2.7 * RAMP_TIME * device.sampleRate;

    userData.ramp_samples = ramp_samples;

    userData.ramp = malloc(ramp_samples * sizeof(double));
    if (userData.ramp == NULL) {
        printf("Memory Allocation Failed!");
        return -7;
    }
    backman_harris_step_response(userData.ramp, ramp_samples);


    if (ma_device_start(&device) != MA_SUCCESS) {
        printf("Failed to start playback device.\n");
        ma_device_uninit(&device);
        return -5;
    }    

    printf("Press Enter to quit...\n");
    getchar();
    
    printf("Number of Samples: %lli\n", userData.sample_count );
    ma_device_uninit(&device);
    
    (void)argc;
    (void)argv;
    return 0;
}