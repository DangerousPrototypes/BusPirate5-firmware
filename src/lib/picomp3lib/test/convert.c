// Test file to convert an mp3 file to wav
#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#ifndef OFF_TARGET
#include "pico/stdlib.h"
#include "f_util.h"
#include "hw_config.h"
#include "fs_mount.h"
#else
#include "stdlib.h"
#endif

#include <ff.h>
#include "wave.h"
#include "music_file.h"
#include "timing.h"

#define WORKING_SIZE        16000
#define RAM_BUFFER_LENGTH   6000

int16_t d_buff[RAM_BUFFER_LENGTH];
unsigned char working[WORKING_SIZE];

#ifndef OFF_TARGET
int process(int argc, char **argv);

// Do not pass arguments to main on target
int main()
{
    bool error = false;

    if (!set_sys_clock_khz(180000, true))
    {
        error = true;
    }    
    stdio_init_all();

    if (error)
    {
        printf("Failed to adjust clock");
        return -1;
    }

    // These files can be accessed at:
    // http://www.aoakley.com/articles/2018-08-24-stereo-test.php
    // https://www2.cs.uic.edu/~i101/SoundFiles/
    // http://nigelcoldwell.co.uk/audio/
    // https://ia902708.us.archive.org/16/items/Audio_MP3_test/20107TEST.mp3
    const char a[8][20] = {"stereo-test.mp3", "StarWars60.wav", "20107TEST.mp3", "abr032.mp3", "abr160.mp3", "mp3-032.mp3", "mp3-320.mp3", "preamble10.wav" };
    char in[25];
    char out[25];
    char* argv[3] = {NULL, in, out};
    char* ret;

    // Build list of files to convert
    for (int i = 0;i < 8; ++i)
    {
        strcpy(in, a[i]);
        strcpy(out, a[i]);

        // Create the output
        ret = strchr(out, '.');
        *ret = '\0';
        ret++;

        if (!strcmp(ret, "mp3"))
        {
            strcat(out, ".wav");
        }
        else
        {
            strcat(out, "_.wav");
        }

        if (process(3, argv))
        {
            printf("Error in conversion\n");
            return -1;
        }
    }
    printf("All decoded\n");
    return 0;
}
#endif

/*
 * Decode the file passed in, and write out a wav file 
 * containing the PCM data.
 * If configured for looping create a a wav file containing the 
 * PCM data decoded in the allowed time interval (5 secs off
 * target, 30 seconds on target)
 * 
 */
#ifdef OFF_TARGET
int main(int argc, char **argv)
#else
int process(int argc, char **argv)
#endif
{
    music_file mf;
    FIL outfile;

    // Parse the args
    if (argc != 3) 
    {
        printf("usage: decode infile outfile.wav\n");
        return -1;
    }
    
    InitTimer();

#ifndef OFF_TARGET
    fs_mount fs;

    // Mount the file system
    fsInitialise(&fs);
    if (!fsMount(&fs))
    {
        printf("cannot mount sd card\n");
        return -1;
    }
#endif

    printf("In: %s, Out: %s\n", argv[1], argv[2]);

    // Open the file, pass in the buffer used to store source data 
    // read from file
    if (!musicFileCreate(&mf, argv[1], working, WORKING_SIZE)) 
    {
        printf("mp3FileCreate error\n");
        return -1;

    }
    
    // Create the file to hold the PCM output
    if (f_open(&outfile, argv[2], FA_CREATE_ALWAYS | FA_WRITE) != FR_OK)
    {
        printf("Outfile open error\n");
        return -1;
    }

    // Get the input file sampling rate
    uint32_t sample_rate = musicFileGetSampleRate(&mf);
    uint16_t num_channels =  musicFileGetChannels(&mf);

    printf("Sample rate: %u Channels: %u\n", sample_rate, num_channels);

    // Write the wav header to the output file
    writeWavHeader(&outfile, sample_rate, num_channels);

    // loop through all of the data
    bool success = true;
    uint32_t num_samples = 0;
    UINT written = 0;
    uint32_t read = 0;

    int32_t start_time; 
    int32_t end_time;
    int32_t diff_time;
    int32_t total_dec_time = 0;

    // If looping stop after 5 secs (off target) or 30 secs (On target)
    int32_t stop_time = 
#ifdef NO_LOOP
                        999999;
#else
#ifdef OFF_TARGET
                        5;
#else
                        30;
#endif
#endif

    printf("Read start\n");

    do
    {
        // Receive the blocks
        start_time = ReadTimer();

        success = musicFileRead(&mf, d_buff, RAM_BUFFER_LENGTH, &read);
        end_time = ReadTimer();
        diff_time = CalcTimeDifference(start_time, end_time);
        total_dec_time += diff_time;

        // Write any data received
        if (success && read)
        {
            num_samples += read / num_channels;

            if ((f_write(&outfile, d_buff, read * sizeof(int16_t), &written) != FR_OK) || (written != read * sizeof(int16_t)))
            {
                printf("Error in f_write\n");
                success = false;
            }
        }
        else
        {
            if (!success)
            {
                printf("Error in mp3FileRead\n");
            }
            else
            {
                printf("Read complete\n");
            }
        }
    } while (success && read &&(total_dec_time / GetClockFrequency() < stop_time));
    
    if (success)
    {
        // update the file header of the output file
        updateWavHeader(&outfile, num_samples, num_channels);
    }

    // Report the performance
    float time_to_decode = (float)total_dec_time / GetClockFrequency();
    float time_decoded = (float)num_samples / mf.sample_rate;
    printf("Time to decode = %6.2f s. Decoded Length = %6.2f s. Ratio = %6.2f\n\n", 
           time_to_decode,
           time_decoded,
           time_decoded / time_to_decode) ;

    // Close the files
    f_close(&outfile);
    musicFileClose(&mf);

#ifndef OFF_TARGET
    fsUnmount(&fs);
#endif

    return 0;
}
