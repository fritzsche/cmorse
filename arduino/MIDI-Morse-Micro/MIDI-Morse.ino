/*
 * MIDI-Morse (C) Thomas Fritzsche
 */

#include "MIDIUSB.h"
#include "button.h"
#include "pitchToNote.h"

//#define DEBUG_SERIAL

void noteOn(byte channel, byte pitch, byte velocity) {
  midiEventPacket_t noteOn = { 0x09, 0x90 | channel, pitch, velocity };
  MidiUSB.sendMIDI(noteOn);
}

void noteOff(byte channel, byte pitch, byte velocity) {
  midiEventPacket_t noteOff = { 0x08, 0x80 | channel, pitch, velocity };
  MidiUSB.sendMIDI(noteOff);
}

Button ditPaddle, dahPaddle;

void setup() {
#ifdef DEBUG_SERIAL
  Serial.begin(115200);
  Serial.println("MIDI-Morse (C) Thomas Fritzsche");
#endif
  ditPaddle.start(2);
  dahPaddle.start(3);
}


void sendMidi(int state, int note) {
  if (state == BUTTON_NO_CHANGE) return;
  if (state == BUTTON_DOWN) noteOn(0, note, 64);
  else noteOff(0, note, 64);
  MidiUSB.flush();
#ifdef DEBUG_SERIAL
  Serial.print(state);
  Serial.print("/");
  Serial.println(note);
  if (state == 0) Serial.println();
#endif
}

void loop() {
  sendMidi(ditPaddle.state(), pitchC3);
  sendMidi(dahPaddle.state(), pitchD3);
}