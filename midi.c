#include <stdio.h>
#include <dlfcn.h>

#include "midi.h"
#include "morse.h"

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
    int *mem = (int *)readProcRefCon;
    for (UInt32 i = 0; i < pktlist->numPackets; i++)
    {
        if (packet->length > 2) {             
            switch(packet->data[0]) {
                case MIDI_NOTE_ON:

                  if(packet->data[1] == MIDI_DIT) {
                    printf("PRESS Dit\n");
                    mem[DIT] = TRUE;
                  } else {
                    printf("PRESS Dah\n");
                    mem[DAH] = TRUE;
                  }
                  break;
                case MIDI_NOTE_OFF:
                 printf("Release\n");
                break;      
            }
        }
        packet = MIDIPacketNext(packet);
    }
}

int open_midi(void* pMemory)
{
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

    MyMIDIInputPortCreate(client, port_name, MyMIDIReadProc, pMemory, &inputPort);
    ItemCount numOfSources = MyMIDIGetNumberOfSources();
    if (numOfSources == 0)
    {
        printf("No MIDI sources found.\n");
        return 1;
    }

    MIDIEndpointRef source = MyMIDIGetSource(0); // use the first MIDI source
    MyMIDIPortConnectSource(inputPort, source, NULL);

    printf("Listening for MIDI events...\n");
    return 0;
}