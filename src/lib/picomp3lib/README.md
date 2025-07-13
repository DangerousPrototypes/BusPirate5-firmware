# picomp3lib
## mp3 C library for Raspberry Pico  
Based on the helix mp3 library created by RealNetworks in 2003    
Additions for ARM from AdaFruit https://github.com/adafruit/Adafruit_MP3  

## Further Changes
1. Size of frame returned by `MP3FrameInfo`
2. Improved `MULSHIFT32` for ARM Cortex M0+
3. CMake files, so can be used as a library for Pico

# Purpose
There are 4 main steps in decoding and playing an MP3 file on the RP Pico
1. Read the MP3 file from internal flash or SD Card
2. Extract MP3 frames from the file
3. Decode the MP3 frame into PCM data chunks
4. Generate sounds from the PCM data chunks through e.g. PWM or I2S

The original helix library (in the `src` directory) provides support for (3). Additions (in the `interface` directory) provides support for (1) and (2)

To initially experiment with the library, without having to be concerned with reading an SD Card, or configuring PWM or I2S, there
is an off target example in the `test` directory. This example can be built for the Raspberry Pi (or other Linux based systems). The example reads an MP3 file and writes a wav file containing the decoded contents.
Build instructions are contained in the [Readme](test/README.md)

# Directory Structure
## `interface` directory
Provides a high level interface to the MP3 decoder. Will also parse and decode wav files.  
Uses `FatFS` to access files.
See https://github.com/carlk3/no-OS-FatFS-SD-SPI-RPi-Pico for a Pico compatible `FatFS` library

The MP3 file to play is initially opened using
### `musicFileCreate`

The format of the file (to e.g. configure the output mechanism) can then be found using
### `musicFileGetSampleRate`
### `musicFileGetChannels`
### `musicFileIsStereo`

The file is decoded through calls to
### `musicFileRead`

The progress through the file can be determined through calls to
### `musicFileBytesLeft`

To terminate playback, the file is closed using
### `musicFileClose`

A simple on target example can be found at:
https://github.com/ikjordan/picomp3test

## `test` directory
An off target test harness that can be run on a Raspberry Pi (or other Linux systems), uses a shim layer to emulate `FatFS`. Decodes an MP3 file
and creates a wav file. See [Readme](test/README.md) for more details

## `src` directory
Base source code for library. The public interface consists 6 functions, however typically the higher level public interface
defined in [music_file.h](test/music_file.h) in the `interface` directory is used.
### `MP3InitDecoder`
Initialises the decoder.
### `MP3FreeDecoder`
Clears buffers
### `MP3Decode`
Decodes one frame of MP3 data
### `MP3GetLastFrameInfo`
Returns info about last MP3 frame decoded (number of samples decoded, sample rate, bitrate, etc). Typically called directly
after a call to MP3Decode
### `MP3GetNextFrameInfo`
Parses the next MP3 frame header
### `MP3FindSyncWord`
Locates the next byte-aligned sync word in the raw mp3 stream

# Usage
This library should be included in a project as a submodule.  
For examples of how to use this library as a submodule see:
1. https://github.com/ikjordan/picosounds
2. https://github.com/ikjordan/picomp3test
