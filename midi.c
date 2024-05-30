#include <stdio.h>
#include <dlfcn.h>

#include "midi.h"
#include "morse.h"

void update_keyer(int byte0, int byte1, key_state_type *p_key)
{
    switch (byte0)
    {
    case MIDI_NOTE_ON:
        if (byte1 == MIDI_DIT)
        {
            p_key->memory[DIT] = SET;
            p_key->state[DIT] = SET;
        }
        else
        {
            p_key->memory[DAH] = SET;
            p_key->state[DAH] = SET;
        }
        break;
    case MIDI_NOTE_OFF:
        if (byte1 == MIDI_DIT)
            p_key->state[DIT] = UNSET;
        else
            p_key->state[DAH] = UNSET;
        break;
    }
}

#if defined(__linux__)
#include <alsa/asoundlib.h> /* for alsa interface   */
#include <pthread.h>        /* for threading        */

void *asound;
const char *(*my_snd_strerror)(int errnum);
int (*my_snd_rawmidi_open)(snd_rawmidi_t **in_rmidi, snd_rawmidi_t **out_rmidi,
                           const char *name, int mode);
ssize_t (*my_snd_rawmidi_read)(snd_rawmidi_t *rmidi, void *buffer, size_t size);

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
        if ((status = my_snd_rawmidi_read(midiin, buffer, 3)) < 0)
        {
            errormessage("Problem reading MIDI input: %s", my_snd_strerror(status));
        }

        update_keyer(buffer[0],buffer[1],parameter->p_key);

    }

    return NULL;
}

#endif

#if defined(__APPLE__)
#include <CoreMIDI/CoreMIDI.h>

// CoreMIDI-Handles
void *coreMIDI;

// Funktionstypen für CoreMIDI-Funktionen
typedef void (*MIDIPacketListCallback)(const void *pktlist, void *readProcRefCon, void *srcConnRefCon);
typedef void (*MIDIRCVCallback)(const MIDIPacketList *pktlist, void *readProcRefCon, void *srcConnRefCon);

OSStatus (*MyMIDIClientCreate)(CFStringRef name, MIDINotifyProc __nullable notifyProc, void *__nullable notifyRefCon, MIDIClientRef *outClient);
OSStatus (*MyMIDIInputPortCreate)(MIDIClientRef client, CFStringRef portName, MIDIReadProc readProc, void *__nullable refCon, MIDIPortRef *outPort);
CFStringRef (*MyCFStringCreateWithCString)(CFAllocatorRef alloc, const char *cStr, CFStringEncoding encoding);
OSStatus (*MyMIDIPortConnectSource)(MIDIPortRef port, MIDIEndpointRef source, void *__nullable connRefCon);
int (*MyMIDIGetSource)(ItemCount sourceIndex0);
ItemCount (*MyMIDIGetNumberOfSources)(void);

void MyMIDIReadProc(const MIDIPacketList *pktlist, void *readProcRefCon, void *srcConnRefCon)
{
    MIDIPacket *packet = (MIDIPacket *)pktlist->packet;
    key_state_type *key = (key_state_type *)readProcRefCon;
    for (UInt32 i = 0; i < pktlist->numPackets; i++)
    {
        if (packet->length > 2) update_keyer(packet->data[0],packet->data[1],key);
        packet = MIDIPacketNext(packet);
    }
}

#endif

int open_midi(void *p_key_state)
{
#if defined(__linux__)
    int status;
    int mode = 0;

    const char *libasoundNames[] = {
        "libasound.so.2",
        "libasound.so"};
    size_t i;

    for (i = 0; i < 2; ++i)
    {
        asound = dlopen(libasoundNames[i], RTLD_NOW);
        if (asound != NULL)
        {
            break;
        }
    }
    if (asound == NULL)
    {
        errormessage("Problem opening dynamic library asound");
        exit(1);
    }

    my_snd_strerror = dlsym(asound, "snd_strerror");
    my_snd_rawmidi_open = dlsym(asound, "snd_rawmidi_open");
    my_snd_rawmidi_read = dlsym(asound, "snd_rawmidi_read");

    pthread_t midiinthread;
    //   snd_rawmidi_t *midiin = NULL;

    //   asound_param_type midi_parameter;

    midi_parameter.p_key = p_key_state;
    midi_parameter.p_midiin = NULL;
    const char *portname = "hw:1,0,0";

    if ((status = my_snd_rawmidi_open(&(midi_parameter.p_midiin), NULL, portname, mode)) < 0)
    {
        errormessage("Problem opening MIDI input: %s", my_snd_strerror(status));
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

    MIDIEndpointRef source = MyMIDIGetSource(0); // use the first MIDI source
    MyMIDIPortConnectSource(inputPort, source, NULL);

    printf("Listening for MIDI events...\n");
#endif
    return 0;
}