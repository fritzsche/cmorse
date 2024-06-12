/*
 * MIDI-Morse (C) Thomas Fritzsche
 */


#include "button.h"

//#define DEBUG_SERIAL

#define DIT_PIN 2
#define DAH_PIN 3

#define PITCH_DIT 48
#define PITCH_DAH 50

#define CHANNEL 0
#define VELOCITY 99

#define LED_PIN 13

Button ditPaddle, dahPaddle;

void setup() {
#ifdef DEBUG_SERIAL
  Serial.begin(115200);
  Serial.println("MIDI-Morse (C) Thomas Fritzsche");
#endif
  // set led pin to output
  pinMode(LED_PIN, OUTPUT);
  digitalWrite(LED_PIN, LOW);
  // setup paddle
  ditPaddle.start(DIT_PIN);
  dahPaddle.start(DAH_PIN);
}


void sendMidi(int state, int note) {
  if (state == BUTTON_NO_CHANGE) return;
  if (state == BUTTON_DOWN) usbMIDI.sendNoteOn(note, VELOCITY, CHANNEL);
  else usbMIDI.sendNoteOff(note, VELOCITY, CHANNEL);
  usbMIDI.send_now();

  if (ditPaddle.currentState() == BUTTON_DOWN || dahPaddle.currentState() == BUTTON_DOWN)
    digitalWrite(LED_PIN, HIGH);
  else
    digitalWrite(LED_PIN, LOW);
}

void loop() {
  sendMidi(ditPaddle.state(), PITCH_DIT);
  sendMidi(dahPaddle.state(), PITCH_DAH);
}