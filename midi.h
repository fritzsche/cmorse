#ifndef MIDI_H_ 
#define MIDI_H_

#define MIDI_NOTE_ON 0x90
#define MIDI_NOTE_OFF 0x80

#define MIDI_DAH 0x30
#define MIDI_DIT 0x32

int open_midi(void *);

#endif 