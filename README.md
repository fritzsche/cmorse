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
* Support serial usb connected morse key.

## Low Latency 
cmorse is written with low latency in mind to achieve 25-40 WPM keying speeds. To C programming language is used to support direct access to Audio/Midi interface of the operating systems. This is the main difference on other earlier attempts to implement high speed morse keying on a PC.

You need to have software/hardware to support it. Particular wireless (bluetooth) audio are tested to by horrible slow and simply not usable. You need in general switch the audio to the build in audio hardware.

Additionally your PC should not run performance intensive software in the background like Virus Scan or Windows Update.

# Windows and low latency
Low latency audio is particular on windows a problem. Standard windows drivers I tested had been often of very bad quality.
Additionally the windows audio interface adds already some significant latency in the so called shared mode. This can create problems is noticeable from 20-25 WPM and gets a problem from > 30 WPM.
Starting with version 0.2 you can run the audio interface in the so called "exclusive" mode. In exclusive mode cmorse can directly access the audio device with significant reduced latency. This is not working stable for all hardware/driver I have tested. In my experience buffer side should be 128. The best results I received with a USB Audio interface (Focusrite Scarlett) that allowed a stable sampling rate of 98 kHz. This setup is running on the Windows standard driver not on ASIO. ASIO is not (yet?) supported as of copyright concerns and the audio interface running good enough.


## Building Software
The software was developed on Mac(Intel) and ported to Windows and Linux operating system. In this early stage I only provide the source code that need to be compiled using cmake.

On MacOS clang is used and GCC on Linux. Windows was tested to compile with MINGW64 that provide a minimal Posix libraries (e.g. pthreads), this is the reason why Microsoft compiler is likely unable to compile cmake.


## Arduino Hardware
To physically connect a morse key we provide arduino sketches for the Arduino Teensy and Arduino Micro in the "arduino" folder. Please notice that this sketches do only work on Arduino devices that supported by the [MIDIUSB](https://www.arduino.cc/reference/en/libraries/midiusb/) library or the respective Teensy Family and do for example NOT work on the Arduino Uno. 

By default the sketch would configure pin 2/3 as Pull-Up Input for dit/dah contacts and a common connection to ground. The connection from the Morse key can be directly to the respective pins.

With the Teensy 4.1 I measured the lowest latency and should be preferred if high speed keying is important.


## Serial USB device
Starting with version 0.2 cmorse supports connecting your a morse code via a serial interface.
Disclaimer: There are many different devices available and I can only support my own HW. You are responsible for your setup.
I personally use a USD-Serial adapter that claims to have an original FTDI-Chip.
The common ground is connected to **RTS**, the left (dit) paddel is connected to **CTS** and the right (dah) paddel is connected to **DCD**.
Technically there is key bouncing, but the IAMBIC B mode as correctly implemented here does not have any problem with bouncing as keying is controlled by the keying memory. So it working perfectly without HW or software de-bouncing.

The setup is identical to the setup SDR Control Mac software by Marcus Roskosch. You find more information in the manual of this software.


# Usage

The program has the following command line parameters:
```
./cmorse --help
cmorse 0.2 - (c) 2024 by Thomas Fritzsche, DJ1TF

cmorse [-w wpm] [-f frequency] [-p frames per package] [-h] [-m device]

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

   -l --list
       List all the available midi source devices and audio devices

   -m device --midi device
       Use specified midi source device

   -c device --serial device
       Use specified serial source device.

   -a device --audio device
       Use specified audio device for output.

   -s --straight
       Straight key mode.

   -x --exclusive (Windows only)
        Use the WASAPI exclusive mode. **Experimental**       

   -v --volume
       volume in percent (0-100).

   -d --disable-decoder
       Disable the decoded text output.

   -h
       Print this help text.
```

Please notice the cmorse will use the default audio device you setup on your computer and the first MIDI input derive. 
Using the option "--list" you receive a list of all hardware devices: midi / serial usb adapter and audio devices. 





# Alpha Code
The code is in pre-beta stage and provided as is. There is no guarantee it works correct. So usage of this software is at your own risk. As of the early stage the software is likely to change frequently without prior notice.

# Version History
0.4 - Add --disable-decoder option
0.3 - Add parameter to specify volume
0.2 - Windows Audio Exclusive mode for lower latency
    - support of serial usb adapter.
0.1 - Initial Alpha Version.

# License
* cmorse is provided as public domain.
* the library mini audio is used by cmorse to connect to the different audio system on Mac/Linux/Windows.