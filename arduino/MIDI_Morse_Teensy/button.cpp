#include <Arduino.h>
#include "Button.h"

void Button::start(const int pinNumber) {
  // Setup the button store the button number
  Button::pinNumber = pinNumber;
  // Set Pullup
  pinMode(pinNumber, INPUT_PULLUP);
  // initialize with time 0
  lastStateChangeTime = 0;
  // initialize the current state of the pinNumber
  lastState = digitalRead(pinNumber);
}

int Button::currentState() {
  return lastState;
}
int Button::state() {
  // get current time and button value
  unsigned long now = millis();
  int value = digitalRead(pinNumber);

  // calculate the time difference in ms once the last change was reported
  unsigned long delta = now - lastStateChangeTime;
  // report no change:
  // 1) the delta has not passed OR 2) the button state has not changed  
  if ( ( delta <= DEBOUNCE ) || ( value == lastState )) return BUTTON_NO_CHANGE;

  lastStateChangeTime = now;
  lastState = value;

  return value;
}
