#include <stdio.h>

// Only Linux and Mac have library
#if defined(__linux__) || defined(__APPLE__)
#include <dlfcn.h>
#include <string.h>
#else // Windows
#include <windows.h>
#include <mmsystem.h>
#endif

#include "midi.h"
#include "serial.h"
#include "morse.h"

void update_keyer(int byte0, int byte1, key_state_type *p_key)
{
    switch (byte0 & 0xf0)
    {
    case MIDI_NOTE_ON:
        if (byte1 == MIDI_DIT)
        {
            atomic_store(&(p_key->memory[DIT]), SET);
            atomic_store(&(p_key->state[DIT]), SET);
        }
        else
        {
            atomic_store(&(p_key->memory[DAH]), SET);
            atomic_store(&(p_key->state[DAH]), SET);
        }
        break;
    case MIDI_NOTE_OFF:
        if (byte1 == MIDI_DIT)
            atomic_store(&(p_key->state[DIT]), UNSET);
        else
            atomic_store(&(p_key->state[DAH]), UNSET);
        break;
    }
}

#if defined(__linux__)
#include <alsa/asoundlib.h> /* for alsa interface   */
#include <pthread.h>        /* for threading        */

// void *asound;

int is_input(snd_ctl_t *ctl, int card, int device, int sub);
int is_output(snd_ctl_t *ctl, int card, int device, int sub);
void error(const char *format, ...);

struct asound_param
{
    snd_rawmidi_t *p_midiin;
    key_state_type *p_key;
};
typedef struct asound_param asound_param_type;

asound_param_type midi_parameter;

void errormessage(const char *format, ...)
{
    va_list ap;
    va_start(ap, format);
    vfprintf(stderr, format, ap);
    va_end(ap);
    putc('\n', stderr);
}

void *midiinfunction(void *arg)
{
    // this is the parameter passed via last argument of pthread_create():
    (snd_rawmidi_t *)arg;
    asound_param_type *parameter = (asound_param_type *)arg;

    snd_rawmidi_t *midiin = parameter->p_midiin;
    unsigned char buffer[3];
    int count = 0;
    int status;

    while (1)
    {
        if (midiin == NULL)
        {
            break;
        }
        if ((status = snd_rawmidi_read(midiin, buffer, 3)) < 0)
        {
            errormessage("Problem reading MIDI input: %s", snd_strerror(status));
        }

        update_keyer(buffer[0], buffer[1], parameter->p_key);
    }

    return NULL;
}

int is_input(snd_ctl_t *ctl, int card, int device, int sub)
{
    snd_rawmidi_info_t *info;
    int status;

    snd_rawmidi_info_alloca(&info);
    snd_rawmidi_info_set_device(info, device);
    snd_rawmidi_info_set_subdevice(info, sub);
    snd_rawmidi_info_set_stream(info, SND_RAWMIDI_STREAM_INPUT);

    if ((status = snd_ctl_rawmidi_info(ctl, info)) < 0 && status != -ENXIO)
    {
        return status;
    }
    else if (status == 0)
    {
        return 1;
    }

    return 0;
}

int is_output(snd_ctl_t *ctl, int card, int device, int sub)
{
    snd_rawmidi_info_t *info;
    int status;

    snd_rawmidi_info_alloca(&info);
    snd_rawmidi_info_set_device(info, device);
    snd_rawmidi_info_set_subdevice(info, sub);
    snd_rawmidi_info_set_stream(info, SND_RAWMIDI_STREAM_OUTPUT);

    if ((status = snd_ctl_rawmidi_info(ctl, info)) < 0 && status != -ENXIO)
    {
        return status;
    }
    else if (status == 0)
    {
        return 1;
    }
    return 0;
}

#define ALSA_MAX_DEV 20

typedef struct alsa_midi_dev
{
    int card;
    int device;
    int sub;
    char *sub_name;
} alsa_midi_dev_t;

alsa_midi_dev_t alsa_midi_dev_array_in[ALSA_MAX_DEV];
int alsa_midi_dev_number = -1;

void collect_subdevice_info(snd_ctl_t *ctl, int card, int device)
{
    snd_rawmidi_info_t *info;
    const char *name;
    const char *sub_name;
    int subs, subs_in, subs_out;
    int sub, in, out;
    int status;

    snd_rawmidi_info_alloca(&info);
    snd_rawmidi_info_set_device(info, device);

    snd_rawmidi_info_set_stream(info, SND_RAWMIDI_STREAM_INPUT);
    snd_ctl_rawmidi_info(ctl, info);
    subs_in = snd_rawmidi_info_get_subdevices_count(info);
    snd_rawmidi_info_set_stream(info, SND_RAWMIDI_STREAM_OUTPUT);
    snd_ctl_rawmidi_info(ctl, info);
    subs_out = snd_rawmidi_info_get_subdevices_count(info);
    subs = subs_in > subs_out ? subs_in : subs_out;

    sub = 0;
    in = out = 0;
    if ((status = is_output(ctl, card, device, sub)) < 0)
    {
        error("cannot get rawmidi information %d:%d: %s",
              card, device, snd_strerror(status));
        return;
    }
    else if (status)
        out = 1;

    if (status == 0)
    {
        if ((status = is_input(ctl, card, device, sub)) < 0)
        {
            error("cannot get rawmidi information %d:%d: %s",
                  card, device, snd_strerror(status));
            return;
        }
    }
    else if (status)
        in = 1;

    if (status == 0)
        return;

    name = snd_rawmidi_info_get_name(info);
    sub_name = snd_rawmidi_info_get_subdevice_name(info);
    sub = 0;
    for (;;)
    {
        /*  printf("%c%c  hw:%d,%d,%d  %s\n",
                 in ? 'I' : ' ', out ? 'O' : ' ',
                 card, device, sub, sub_name); */
        // collect the inbound device
        if (in)
        {
            if (ALSA_MAX_DEV <= alsa_midi_dev_number)
            {
                error("Too many midi devices.");
            }
            alsa_midi_dev_array_in[alsa_midi_dev_number].card = card;
            alsa_midi_dev_array_in[alsa_midi_dev_number].device = device;
            alsa_midi_dev_array_in[alsa_midi_dev_number].sub = sub;
            alsa_midi_dev_array_in[alsa_midi_dev_number].sub_name = strdup(sub_name);
            alsa_midi_dev_number++;
        }
        if (++sub >= subs)
            break;

        in = is_input(ctl, card, device, sub);
        out = is_output(ctl, card, device, sub);
    }
}

void collect_midi_devices_on_card(int card)
{
    snd_ctl_t *ctl;
    char name[32];
    int device = -1;
    int status;
    sprintf(name, "hw:%d", card);
    if ((status = snd_ctl_open(&ctl, name, 0)) < 0)
    {
        error("cannot open control for card %d: %s", card, snd_strerror(status));
        return;
    }
    do
    {
        status = snd_ctl_rawmidi_next_device(ctl, &device);
        if (status < 0)
        {
            error("cannot determine device number: %s", snd_strerror(status));
            break;
        }
        if (device >= 0)
        {
            collect_subdevice_info(ctl, card, device);
        }
    } while (device >= 0);
    snd_ctl_close(ctl);
}

void collect_midi_devices()
{
    int status;
    int card = -1;

    if (alsa_midi_dev_number == -1)
    {
        alsa_midi_dev_number = 0;
        if ((status = snd_card_next(&card)) < 0)
        {
            fprintf(stderr,"cannot determine card number: %s", snd_strerror(status));
            return;
        }

        while (card >= 0)
        {
            collect_midi_devices_on_card(card);
            if ((status = snd_card_next(&card)) < 0)
            {
                fprintf(stderr,"cannot determine card number: %s", snd_strerror(status));
                break;
            }
        }
    }
}

#endif

#if defined(__APPLE__)
#include <CoreMIDI/CoreMIDI.h>

// CoreMIDI-Handles
void *coreMIDI;

// Funktionstypen für CoreMIDI-Funktionen
typedef void (*MIDIPacketListCallback)(const void *pktlist, void *readProcRefCon, void *srcConnRefCon);
typedef void (*MIDIRCVCallback)(const MIDIPacketList *pktlist, void *readProcRefCon, void *srcConnRefCon);

OSStatus (*MyMIDIObjectGetStringProperty)(MIDIObjectRef obj, CFStringRef propertyID, CFStringRef __nullable *__nonnull str);

OSStatus (*MyMIDIClientCreate)(CFStringRef name, MIDINotifyProc __nullable notifyProc, void *__nullable notifyRefCon, MIDIClientRef *outClient);
OSStatus (*MyMIDIInputPortCreate)(MIDIClientRef client, CFStringRef portName, MIDIReadProc readProc, void *__nullable refCon, MIDIPortRef *outPort);
CFStringRef (*MyCFStringCreateWithCString)(CFAllocatorRef alloc, const char *cStr, CFStringEncoding encoding);
OSStatus (*MyMIDIPortConnectSource)(MIDIPortRef port, MIDIEndpointRef source, void *__nullable connRefCon);
Boolean (*MyCFStringGetCString)(CFStringRef theString, char *buffer, CFIndex bufferSize, CFStringEncoding encoding);
int (*MyMIDIGetSource)(ItemCount sourceIndex0);
ItemCount (*MyMIDIGetNumberOfSources)(void);

void MyMIDIReadProc(const MIDIPacketList *pktlist, void *readProcRefCon, void *srcConnRefCon)
{
    MIDIPacket *packet = (MIDIPacket *)pktlist->packet;
    key_state_type *key = (key_state_type *)readProcRefCon;
    for (UInt32 i = 0; i < pktlist->numPackets; i++)
    {
        if (packet->length > 2)
            update_keyer(packet->data[0], packet->data[1], key);
        packet = MIDIPacketNext(packet);
    }
}

#endif

#ifdef _WIN64
#define STR_VALUE(arg) #arg
#define FUNCTION_NAME(name) STR_VALUE(name)
#define MIDI_CAPS FUNCTION_NAME(midiInGetDevCaps)
typedef UINT (*midiInGetNumDevs_proc)(void);
typedef MMRESULT (*midiInOpen_proc)(LPHMIDIIN phmi, UINT uDeviceID, DWORD_PTR dwCallback, DWORD_PTR dwInstance, DWORD fdwOpen);
typedef MMRESULT (*midiInStart_proc)(HMIDIIN hmi);
typedef MMRESULT (*midiInGetDevCaps_proc)(UINT uDeviceID, LPMIDIINCAPS pmic, UINT cbmic);

void CALLBACK MidiInProc(HMIDIIN hMidiIn, UINT wMsg, DWORD_PTR dwInstance, DWORD_PTR dwParam1, DWORD_PTR dwParam2)
{
    key_state_type *p_key = (key_state_type *)dwInstance;
    if (wMsg == MIM_DATA)
    {
        update_keyer(dwParam1 & 0xf0, dwParam1 >> 8 & 0xff, p_key);
    }
}

#endif

int list_midi()
{

#if defined(__linux__)
    collect_midi_devices();
    if (alsa_midi_dev_number == 0)
    {
        fprintf(stderr,"No MIDI Device found\n");
    }
    printf("%i MIDI device(s)\n", alsa_midi_dev_number);
    for (int i = 0; i < alsa_midi_dev_number; i++)
    {
        printf("  %i hw:%d,%d,%d  %s\n",
               i,
               alsa_midi_dev_array_in[i].card,
               alsa_midi_dev_array_in[i].device,
               alsa_midi_dev_array_in[i].sub,
               alsa_midi_dev_array_in[i].sub_name);
    }
#endif
#ifdef _WIN64
    UINT nMidiDeviceNum;
    MIDIINCAPS mic;

    HMODULE winmm = (ma_handle)LoadLibrary("winmm.dll");
    if (winmm == NULL)
    {
        fprintf(stderr, "Error loading winmm\n");
        return -1;
    }

    midiInGetNumDevs_proc my_midiInGetNumDevs = (midiInGetNumDevs_proc)GetProcAddress(winmm, "midiInGetNumDevs");
    midiInOpen_proc my_midiInOpen = (midiInOpen_proc)GetProcAddress(winmm, "midiInOpen");
    midiInStart_proc my_midiInStart = (midiInStart_proc)GetProcAddress(winmm, "midiInStart");
    midiInGetDevCaps_proc my_midiInGetDevCaps = (midiInGetDevCaps_proc)GetProcAddress(winmm, MIDI_CAPS);
    nMidiDeviceNum = my_midiInGetNumDevs();
    if (nMidiDeviceNum == 0)
    {
        fprintf(stderr, "No Midi device found.\n");
        return 0;
    }
    printf("%lu available MIDI device(s)\n", nMidiDeviceNum);
    for (int i = 0; i < nMidiDeviceNum; i++)
    {
        /* Get info about the next device */
        if (!my_midiInGetDevCaps(i, &mic, sizeof(MIDIINCAPS)))
        {
            /* Display its Device ID and name */
            printf("  #%u: %s\r\n", i, mic.szPname);
        }
    }
#endif

#if defined(__APPLE__)
    // dynamic load CoreMidi
    coreMIDI = dlopen("/System/Library/Frameworks/CoreMIDI.framework/CoreMIDI", RTLD_NOW);
    if (!coreMIDI)
    {
        fprintf(stderr, "Error loading CoreMIDI: %s\n", dlerror());
        return 1;
    }

    // MyMIDIClientCreate = dlsym(coreMIDI, "MIDIClientCreate");
    // MyMIDIInputPortCreate = dlsym(coreMIDI, "MIDIInputPortCreate");
    MyCFStringCreateWithCString = dlsym(coreMIDI, "CFStringCreateWithCString");
    MyMIDIGetSource = dlsym(coreMIDI, "MIDIGetSource");
    MyMIDIPortConnectSource = dlsym(coreMIDI, "MIDIPortConnectSource");
    MyMIDIGetNumberOfSources = dlsym(coreMIDI, "MIDIGetNumberOfSources");
    MyMIDIObjectGetStringProperty = dlsym(coreMIDI, "MIDIObjectGetStringProperty");
    CFStringRef *MykMIDIPropertyName = dlsym(coreMIDI, "kMIDIPropertyName");
    MyCFStringGetCString = dlsym(coreMIDI, "CFStringGetCString");

    dlclose(coreMIDI);

    CFStringRef client_name = MyCFStringCreateWithCString(NULL, "MIDI Client", kCFStringEncodingMacRoman);
    CFStringRef port_name = MyCFStringCreateWithCString(NULL, "Input Port", kCFStringEncodingMacRoman);

    MIDIClientRef client;
    // MyMIDIClientCreate(client_name, NULL, NULL, &client);

    // MIDIPortRef inputPort;

    // MyMIDIInputPortCreate(client, port_name, MyMIDIReadProc, p_key_state, &inputPort);
    ItemCount numOfSources = MyMIDIGetNumberOfSources();
    if (numOfSources == 0)
    {
#ifdef SERIAL_SUPPORT
        return 0;
#endif
        printf("No MIDI sources found.\n");
        return 1;
    }

    CFStringRef name = NULL;
    printf("%lu available MIDI source(s)\n", numOfSources);
    for (int i = 0; i < numOfSources; i++)
    {
        char cName[128];
        MIDIEndpointRef endpoint = MyMIDIGetSource(i);
        OSStatus status = MyMIDIObjectGetStringProperty(endpoint, *(MykMIDIPropertyName), &name);
        if (status != noErr)
        {
            printf("Error retrieving name for device %d.\n", i);
            continue;
        }
        if (!MyCFStringGetCString(name, cName, sizeof(cName), kCFStringEncodingUTF8))
        {
            printf("Error converting name for device %d.\n", i);
            continue;
        }
        printf("  %d: %s \n", i, cName);
    }

#endif
    return 0;
}

int open_midi(void *p_key_state, int midi_device)
{
#ifdef _WIN64
    MIDIINCAPS mic;
    HMIDIIN hMidiDevice = NULL;
    DWORD nMidiPort = 0;
    UINT nMidiDeviceNum;
    MMRESULT rv;
    HMODULE winmm = (ma_handle)LoadLibrary("winmm.dll");

    if (midi_device >= 0)
        nMidiPort = midi_device;

    if (winmm == NULL)
    {
        fprintf(stderr, "Error loading winmm\n");
        exit(-1);
    }

    midiInGetNumDevs_proc my_midiInGetNumDevs = (midiInGetNumDevs_proc)GetProcAddress(winmm, "midiInGetNumDevs");
    midiInOpen_proc my_midiInOpen = (midiInOpen_proc)GetProcAddress(winmm, "midiInOpen");
    midiInStart_proc my_midiInStart = (midiInStart_proc)GetProcAddress(winmm, "midiInStart");
    midiInGetDevCaps_proc my_midiInGetDevCaps = (midiInGetDevCaps_proc)GetProcAddress(winmm, MIDI_CAPS);

    nMidiDeviceNum = my_midiInGetNumDevs();
    if (nMidiDeviceNum == 0)
    {
        fprintf(stderr, "No Midi device found.\n");
        exit(-1);
    }
    printf("Number of midi devices: %i \n", nMidiDeviceNum);
    /*    for (int i = 0; i < nMidiDeviceNum; i++)
        {
            // Get info about the next device
            if (!my_midiInGetDevCaps(i, &mic, sizeof(MIDIINCAPS)))
            {
                // Display its Device ID and name
                printf("Device ID #%u: %s\r\n", i, mic.szPname);
            }
        }
        */

    rv = my_midiInOpen(&hMidiDevice, nMidiPort, (DWORD_PTR)(void *)MidiInProc, (DWORD_PTR)p_key_state, CALLBACK_FUNCTION);
    if (rv != MMSYSERR_NOERROR)
    {
        fprintf(stderr, "midiInOpen() failed...rv=%d", rv);
        return -1;
    }
    rv = my_midiInStart(hMidiDevice);
    if (rv != MMSYSERR_NOERROR)
    {
        fprintf(stderr, "midiInStart() failed...rv=%d", rv);
        return -1;
    }

#endif

#if defined(__linux__)
    int status;
    int mode = 0;
    char name[32];

    pthread_t midiinthread;

    midi_parameter.p_key = p_key_state;
    midi_parameter.p_midiin = NULL;

    char *portname;
    if (midi_device == -1)
        portname = strdup("hw:1,0,0");
    else
    {
        collect_midi_devices();
        if (midi_device >= alsa_midi_dev_number)
        {
            fprintf(stderr, "Incorrect midi device number.\n");
            return -1;
        }
        sprintf(name, "hw:%d,%d,%d",
                alsa_midi_dev_array_in[midi_device].card,
                alsa_midi_dev_array_in[midi_device].device,
                alsa_midi_dev_array_in[midi_device].sub);
        portname = strdup(name);
    }

    if ((status = snd_rawmidi_open(&(midi_parameter.p_midiin), NULL, portname, mode)) < 0)
    {
        errormessage("Problem opening MIDI input: %s", snd_strerror(status));
        exit(1);
    }
    status = pthread_create(&midiinthread, NULL, midiinfunction, &midi_parameter);
    if (status == -1)
    {
        errormessage("Unable to create MIDI input thread.");
        exit(1);
    }
#endif

#if defined(__APPLE__)
    printf("Initializing MIDI...\n");
    // dynamic load CoreMidi
    coreMIDI = dlopen("/System/Library/Frameworks/CoreMIDI.framework/CoreMIDI", RTLD_NOW);
    if (!coreMIDI)
    {
        fprintf(stderr, "Error loading CoreMIDI: %s\n", dlerror());
        return 1;
    }

    MyMIDIClientCreate = dlsym(coreMIDI, "MIDIClientCreate");
    MyMIDIInputPortCreate = dlsym(coreMIDI, "MIDIInputPortCreate");
    MyCFStringCreateWithCString = dlsym(coreMIDI, "CFStringCreateWithCString");
    MyMIDIGetSource = dlsym(coreMIDI, "MIDIGetSource");
    MyMIDIPortConnectSource = dlsym(coreMIDI, "MIDIPortConnectSource");
    MyMIDIGetNumberOfSources = dlsym(coreMIDI, "MIDIGetNumberOfSources");
    MyMIDIObjectGetStringProperty = dlsym(coreMIDI, "MIDIObjectGetStringProperty");
    CFStringRef *MykMIDIPropertyName = dlsym(coreMIDI, "kMIDIPropertyName");
    MyCFStringGetCString = dlsym(coreMIDI, "CFStringGetCString");

    dlclose(coreMIDI);

    CFStringRef client_name = MyCFStringCreateWithCString(NULL, "MIDI Client", kCFStringEncodingMacRoman);
    CFStringRef port_name = MyCFStringCreateWithCString(NULL, "Input Port", kCFStringEncodingMacRoman);

    MIDIClientRef client;
    MyMIDIClientCreate(client_name, NULL, NULL, &client);

    MIDIPortRef inputPort;

    MyMIDIInputPortCreate(client, port_name, MyMIDIReadProc, p_key_state, &inputPort);
    ItemCount numOfSources = MyMIDIGetNumberOfSources();
    if (numOfSources == 0)
    {
        printf("No MIDI sources found.\n");
        return 1;
    }
    int my_midi_device = 0;
    if (midi_device >= 0)
        my_midi_device = midi_device;
    if (my_midi_device >= numOfSources)
    {
        printf("MIDI source device does not exist.\n");
        return 1;
    }

    printf("Midi Device selected: %d\n", my_midi_device);
    MIDIEndpointRef source = MyMIDIGetSource(my_midi_device); // use the first MIDI source
    MyMIDIPortConnectSource(inputPort, source, NULL);

    printf("Listening for MIDI events...\n");
#endif
    return 0;
}