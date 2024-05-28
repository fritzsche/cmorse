
#include <stdio.h>
#include <CoreMIDI/CoreMIDI.h>
//#include <CoreFoundation/CoreFoundation.h>
#include <dlfcn.h>


// CoreMIDI-Handles
void *coreMIDI;
void *coreFoundation;

//OSStatus (*MIDIClientCreate)(CFStringRef clientName, MIDINotifyProc notifyProc, void *notifyRefCon, MIDIClientRef *outClient);
//OSStatus (*MIDIInputPortCreate)(MIDIClientRef client, CFStringRef portName, MIDIPacketListCallback readProc, void *refCon, MIDIPortRef *outPort);



// Funktionstypen für CoreMIDI-Funktionen
typedef void (*MIDIPacketListCallback)(const void *pktlist, void *readProcRefCon, void *srcConnRefCon);
typedef void (*MIDIRCVCallback)(const MIDIPacketList *pktlist, void *readProcRefCon, void *srcConnRefCon);


OSStatus (*MyMIDIClientCreate)(	CFStringRef					name,
					MIDINotifyProc __nullable	notifyProc,
					void * __nullable			notifyRefCon,
					MIDIClientRef *				outClient );

OSStatus (*MyMIDIInputPortCreate)(	MIDIClientRef		client,
						CFStringRef			portName,
						MIDIReadProc		readProc,
						void * __nullable	refCon,
						MIDIPortRef * 		outPort );                    

CFStringRef (*MyCFStringCreateWithCString)(CFAllocatorRef alloc, const char *cStr, CFStringEncoding encoding);

int (*MyMIDIGetSource)(	ItemCount sourceIndex0 );	


//gcc -o midi_input midi_input.c -framework CoreMIDI -framework CoreFoundation

void MyMIDINotifyProc(const MIDINotification *message, void *refCon)
{
    printf("MIDI Notify: ");
    switch (message->messageID)
    {
    case kMIDIMsgObjectAdded:
        printf("MIDI Object Added\n");
        break;
    case kMIDIMsgObjectRemoved:
        printf("MIDI Object Removed\n");
        break;
    case kMIDIMsgPropertyChanged:
        printf("MIDI Property Changed\n");
        break;
    case kMIDIMsgSetupChanged:
        printf("MIDI Setup Changed\n");
        break;
    case kMIDIMsgIOError:
        printf("MIDI I/O Error\n");
        break;
    default:
        printf("Unknown MIDI message\n");
        break;
    }
}


void MyMIDIReadProc(const MIDIPacketList *pktlist, void *readProcRefCon, void *srcConnRefCon) {
    MIDIPacket *packet = (MIDIPacket *)pktlist->packet;

    for (UInt32 i = 0; i < pktlist->numPackets; i++) {
        printf("MIDI Event: ");
        for (UInt32 j = 0; j < packet->length; j++) {
            printf("%02X ", packet->data[j]);
        }
        printf("\n");
        packet = MIDIPacketNext(packet);
    }
}

int open_midi()
{
    // dynamic load CoreMidi
    coreMIDI = dlopen("/System/Library/Frameworks/CoreMIDI.framework/CoreMIDI", RTLD_NOW);
    if (!coreMIDI) {
        fprintf(stderr, "Error loading CoreMIDI: %s\n", dlerror());
        return 1;
    }

    // dynamic load of CoreFoundation
    coreFoundation = dlopen("/System/Library/Frameworks/CoreFoundation.framework/CoreFoundation", RTLD_NOW);
    if (!coreFoundation) {
        fprintf(stderr, "Fehler beim Laden der CoreFoundation-Bibliothek: %s\n", dlerror());
        return 1;
    }

    MyMIDIClientCreate = dlsym(coreMIDI, "MIDIClientCreate");
    MyMIDIInputPortCreate = dlsym(coreMIDI, "MIDIInputPortCreate");
    MyCFStringCreateWithCString = dlsym(coreMIDI, "CFStringCreateWithCString");
    MyMIDIGetSource = dlsym(coreMIDI, "MIDIGetSource");



   CFStringRef client_name = MyCFStringCreateWithCString(NULL, "MIDI Client", kCFStringEncodingMacRoman);
   CFStringRef port_name = MyCFStringCreateWithCString(NULL, "Input Port", kCFStringEncodingMacRoman);


    MIDIClientRef client;

    MyMIDIClientCreate(client_name, NULL, NULL, &client);


    dlclose(coreMIDI);
    dlclose(coreFoundation);
    
    MIDIPortRef inputPort; 
    MyMIDIInputPortCreate(client, port_name, MyMIDIReadProc, NULL, &inputPort); 

/*
    ItemCount numOfSources = MIDIGetNumberOfSources();
    if (numOfSources == 0)
    {
        printf("No MIDI sources found.\n");
        return 1;
    }
*/
    MIDIEndpointRef source = MyMIDIGetSource(0); // We use the first MIDI source
    MIDIPortConnectSource(inputPort, source, NULL);

    printf("Listening for MIDI events...\n");
    printf("Press Ctrl+C to exit.\n");

 //   CFRunLoopRun();

}