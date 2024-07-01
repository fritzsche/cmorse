# Overview
The command line program cmorse is a morse training oscillator. Its a morse practice tool.
You can connect a morse paddle or straight key via a arduino to your computer as input device. The Arduino will emulate a USB Midi instrument and allows low latency keying.

## Features
* Keying mode: IAMBIC B / Straight Key
* Blackman Harris ramp-up for ultra soft keying without click
(Details: QEX May/June 2006 3 / CW Shaping in DSP Software)
* Output of decoded text keyed text according international morse code.
* Low latency optimized
* Mac (development system)/Linux/Windows 10/11 (MINGW64)

## Low Latency 
cmorse is written with low latency in mind to achieve 25-40 WPM keying speeds. To C programming language is used to support direct access to Audio/Midi interface of the operating systems. This is the main difference on other earlier attempts to implement high speed morse keying on a PC.

You need to have software/hardware to support it. Particular wireless (bluetooth) audio are tested to by horrible slow and simply not usable. You need in general switch the audio to the build in audio hardware.

Additionally your PC should not run performance intensive software in the background like Virus Scan or Windows Update.

## Building Software
The software was developed on Mac(Intel) and ported to Windows and Linux operating system. In this early stage I only provide the source code that need to be compiled using cmake.

On MacOS clang is used and GCC on Linux. Windows was tested to compile with MINGW64 that provide a minimal Posix libraries (e.g. pthreads), this is the reason why Microsoft compiler is likely unable to compile cmake.


## Arduino Hardware
To physically connect a morse key we provide arduino sketches for the Arduino Teensy and Arduino Micro in the "arduino" folder. Please notice that this sketches do only work on Arduino devices that supported by the [MIDIUSB](https://www.arduino.cc/reference/en/libraries/midiusb/) library or the respective Teensy Family and do for example NOT work on the Arduino Uno. 

By default the sketch would configure pin 2/3 as Pull-Up Input for dit/dah contacts and a common connection to ground. The connection from the Morse key can be directly to the respective pins.

With the Teensy 4.1 I measured the lowest latency and should be preferred if high speed keying is important.


# Usage

The program has the following command line parameters:
```
./cmorse --help
cmorse 0.1 - (c) 2024 by Thomas Fritzsche, DJ1TF

cmorse [-w wpm] [-f frequency] [-p frames per package] [-h]

   -w wpm --wpm wpm
       Specify the speed of the keyer in words per minute(wpm).
       The default speed is 25wpm.

   -f frequency --frequency frequency
       Set the sidetone frequency in Hertz.
       The default frequency of the sidetone is 500Hz.

   -p frames --package frames
       Specify the number of number of frames that is processed at
       once by the audio subsystem. Typical values are 32,64 or 128 frames
       The smaller the number of frames the lower the latency.
       Default value for the number of frames is 128 frames.

   -s --straight
       Straight key mode.

   -h
       Print this help text.
```

Please notice the cmorse will use the default audio device you setup on your computer and the first MIDI input derive. If you have connected multiple Midi hardware to your computer you need to disconnect it.

# Alpha Code
The code is in pre-beta stage and provided as is. There is no guarantee it works correct. So usage of this software is at your own risk. As of the early stage the software is likely to change frequently without prior notice.

# Version History
0.1 - Initial Alpha Version.

# License
* cmorse is provided as public domain.
* the library mini audio is used by cmorse to connect to the different audio system on Mac/Linux/Windows.