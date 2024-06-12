#include <Arduino.h>
#include "Button.h"


void Button::start( const int pinNumber )  {
  // Setup the button store the button number
  Button::pinNumber = pinNumber;
  // Set Pullup 
  pinMode(pinNumber, INPUT_PULLUP);    
  // initialize with time 0
  lastChange = 0;  
  // initialize the current state of the pinNumber
  lastValue = digitalRead(pinNumber);    
}

int Button::state(){
  // get current time and button value
  long now = millis();
  int value = digitalRead(pinNumber);      

  // report no change if debounce time is not reached
  long delta = now - lastChange;
  if (delta < DEBOUNCE_MS || value == lastValue) return BUTTON_NO_CHANGE;
  lastChange = now;
  lastValue = value;  
  return value;
} 

