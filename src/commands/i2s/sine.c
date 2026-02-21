// TODO: BIO use, pullups, psu
/*
    Welcome to dummy.c, a growing demonstration of how to add commands to the Bus Pirate firmware.
    You can also use this file as the basis for your own commands.
    Type "dummy" at the Bus Pirate prompt to see the output of this command
    Temporary info available at https://forum.buspirate.com/t/command-line-parser-for-developers/235
*/
#include <stdio.h>
#include <string.h>
#include <math.h>
#include "pico/stdlib.h"
#include "pirate.h"
#include "command_struct.h"
#include "fatfs/ff.h"
#include "pirate/storage.h"
#include "lib/bp_args/bp_cmd.h"
#include "ui/ui_help.h"
#include "system_config.h"
#include "pirate/amux.h"
#include "pirate/button.h"
#include "msc_disk.h"
#include "hardware/pio.h"
#include "bytecode.h"
#include "pio_config.h"
#include "mode/i2s.h"
// This array of strings is used to display help USAGE examples for the dummy command
static const char* const usage[] = { "sine [Hz] [-s seconds] [-f file]",
                                     "Generate a sine wave at the default frequency (1000Hz):%s sine",                                 
                                     "Generate 2000Hz sine wave:%s sine 2000",
                                     "Generate 1000Hz sine wave for 5 seconds:%s sine 1000 -s 5",
                                     "Read WAV file header:%s sine -f test.wav" };

static const bp_command_opt_t sine_opts[] = {
    { "seconds",  's', BP_ARG_REQUIRED, NULL, T_HELP_DUMMY_COMMANDS },
    { "file",     'f', BP_ARG_REQUIRED, NULL, T_HELP_DUMMY_COMMANDS },
    { 0 }
};

static const bp_command_positional_t sine_positionals[] = {
    { "Hz", NULL, T_HELP_DUMMY_COMMANDS, false },
    { 0 }
};

// Command definition (not static - will be extern'd)
const bp_command_def_t sine_def = {
    .name         = "sine",
    .description  = T_HELP_DUMMY_COMMANDS,
    .opts         = sine_opts,
    .positionals      = sine_positionals,
    .positional_count = 1,
    .usage        = usage,
    .usage_count  = count_of(usage),
};

// WAV file header structure
typedef struct {
    char chunk_id[4];        // "RIFF"
    uint32_t chunk_size;     // Size of the entire file minus 8 bytes
    char format[4];          // "WAVE"
    char subchunk1_id[4];    // "fmt "
    uint32_t subchunk1_size; // Size of the fmt chunk (16 for PCM)
    uint16_t audio_format;   // Audio format (1 for PCM)
    uint16_t num_channels;   // Number of channels (1 = mono, 2 = stereo)
    uint32_t sample_rate;    // Sampling rate (e.g., 44100 Hz)
    uint32_t byte_rate;      // Byte rate = SampleRate * NumChannels * BitsPerSample / 8
    uint16_t block_align;    // Block align = NumChannels * BitsPerSample / 8
    uint16_t bits_per_sample;// Bits per sample (e.g., 16 bits)
    char subchunk2_id[4];    // "data"
    uint32_t subchunk2_size; // Size of the data chunk (number of bytes of audio data)
} wav_header_t;

void sine_handler(struct command_result* res) {
    char file[13];  // somewhere to keep a string value (8.3 filename + 0x00 = 13 characters max)

    // Help check - new bp_cmd API
    if (bp_cmd_help_check(&sine_def, res->help_flag)) {
        return;
    }

    FIL file_handle;                                                  // file handle
    FRESULT fresult;  
    wav_header_t header;
    bool f_flag = bp_cmd_get_string(&sine_def, 'f', file, sizeof(file));

    if(!f_flag){
        printf("Set a file name with -f flag to read a WAV file header.\r\n");
        printf("Example: sine -f test.wav\r\n");
        return;
    }

    // open the file
    fresult = f_open(&file_handle, file, FA_READ); // open the file for reading
    if (fresult != FR_OK) {
        printf("Error opening file %s for reading\r\n", file);
        system_config.error = true; // set the error flag
        return;
    }
    // if the file was opened
    printf("File %s opened for reading\r\n", file);

    // read the file
    UINT bytes_read; // somewhere to store the number of bytes read
    fresult = f_read(&file_handle, &header, sizeof(header), &bytes_read); // read the data from the file
    if (fresult == FR_OK) {                                              // if the read was successful
        printf("Read %d bytes from file %s\r\n", bytes_read, file);
    } else {                                     // error reading file
        printf("Error reading file %s\r\n", file);
        return;
    }

    printf("Chunk ID: %.4s\r\n", header.chunk_id);
    printf("Chunk Size: %u\r\n", header.chunk_size);
    printf("Format: %.4s\r\n", header.format);
    printf("Subchunk1 ID: %.4s\r\n", header.subchunk1_id);
    printf("Subchunk1 Size: %u\r\n", header.subchunk1_size);
    printf("Audio Format: %u\r\n", header.audio_format);
    printf("Number of Channels: %u\r\n", header.num_channels);
    printf("Sample Rate: %u\r\n", header.sample_rate);
    printf("Byte Rate: %u\r\n", header.byte_rate);
    printf("Block Align: %u\r\n", header.block_align);
    printf("Bits Per Sample: %u\r\n", header.bits_per_sample);
    printf("Subchunk2 ID: %.4s\r\n", header.subchunk2_id);
    printf("Subchunk2 Size: %u\r\n", header.subchunk2_size);

    int16_t audio_data[1000];
    // read the audio data
    fresult = f_read(&file_handle, audio_data, sizeof(audio_data), &bytes_read); // read the audio data from the file
    if (fresult == FR_OK) {                                              // if the read was successful
        printf("Read %d bytes of audio data from file %s\r\n", bytes_read, file);
    } else {                                     // error reading file
        printf("Error reading audio data from file %s\r\n", file);
        return;
    }


    // loop to push the audio data to the PIO
    //uint32_t samples = bytes_read / sizeof(int16_t); // calculate the number of samples read
    uint32_t samples= 1000;

    for(uint32_t cnt=0; cnt<5; cnt++){
        printf("Pushing %d samples to PIO\r\n", samples);
        for (uint32_t i = 0; i < samples; i++) {
            // Send each sample, shift left to fit in 32 bits
            pio_sm_put_blocking(i2s_pio_config_out.pio, i2s_pio_config_out.sm, (audio_data[i]));
        }
    }

    // close the file
    fresult = f_close(&file_handle); // close the file
    if (fresult != FR_OK) {
        printf("Error closing file %s\r\n", file);
        system_config.error = true; // set the error flag
        return;
    }
    // if the file was closed
    printf("File %s closed\r\n", file);    



    return;













#if 0


    // 10 cycles of 1kHz at 44.1kHz = 10 * 44.1 = 441 samples
    #define MAX_TABLE_SIZE 500
    #define AMPLITUDE 32767.0  // Max amplitude for 16-bit signed
    #define CYCLES 10
    int16_t sine_table[MAX_TABLE_SIZE];
    uint32_t sample_frequency_hz = i2s_mode_config.freq; // Get the frequency from the mode config
    uint32_t sine_frequency_hz = 1000; // 1kHz sine wave
    int32_t cycle_multiplier = -1; // Default multiplier for cycles
    uint32_t table_size;
    uint32_t duration_seconds = 2; // Duration of the sine wave output in seconds
    
    uint32_t temp;
    if(bp_cmd_get_positional_uint32(&sine_def, 1, &temp)){
        sine_frequency_hz = temp; // Get the sine frequency from the command line argument
    }

    uint32_t dur;
    if (bp_cmd_get_uint32(&sine_def, 's', &dur)) {
        duration_seconds = dur;
    }
    printf("Sine Wave: %dHz @ %dHz sample rate, %d seconds\r\n", sine_frequency_hz, sample_frequency_hz, duration_seconds);    

    //determine if the sample/sine has a remainder
    // if sample/sine has a remainder, attempt the 10 cycle loop, if it will fit in MAX_TABLE_SIZE    
    if( (sample_frequency_hz % sine_frequency_hz) == 0) {
        cycle_multiplier = 1; // No remainder, use 1 cycle
    } else {
        for(uint32_t i = CYCLES; i > 1; i--) {
            // Check if the sample frequency can be evenly divided by sine frequency for 10 cycles
            if(((sample_frequency_hz * i) % sine_frequency_hz) == 0) {
                cycle_multiplier = i;
                break;
            }
        }   
    }

    // If no valid cycle multiplier found, return an error
    if(cycle_multiplier < 0) {
        printf("Error: Cannot create table for %dHz sine wave at %dHz sample frequency.\r\n", sine_frequency_hz, sample_frequency_hz);
        return;
    }    

    // Calculate the table size based on the sample frequency and sine frequency
    table_size = (sample_frequency_hz * cycle_multiplier) / sine_frequency_hz;

    if(table_size > MAX_TABLE_SIZE) {
        printf("Error: Cannot fit %d cycles of %dHz sine wave (%d samples) at %dHz sampling frequency in sine table.\r\n", cycle_multiplier, sine_frequency_hz, table_size, sample_frequency_hz);
        return;
    } 

    printf("Sine table: %d samples, %d cycles\r\nReady!\r\n", table_size, cycle_multiplier);

    for (uint32_t i = 0; i < table_size; i++) {
        // Each index represents: i / SAMPLE_RATE seconds
        // For 10 cycles: phase = 2*pi*FREQ*(i/SAMPLE_RATE)
        // But for exactly 10 cycles in 441 samples: phase = 2*pi*CYCLES*i/TABLE_SIZE
        double phase = 2.0 * M_PI * cycle_multiplier * i / table_size;
        sine_table[i] = (int16_t)(AMPLITUDE * sin(phase));
    }

    //calculate how many cycles for 1 second of output
    //taking into account the cycle multiplier, sample frequency and table size
    uint32_t cycles = (sine_frequency_hz / cycle_multiplier) * duration_seconds;
    //printf("Cycles per second: %d\r\n", cycles);

    for(int i=0; i<(cycles); i++) {
        // Send the sine wave samples to the PIO
        // The PIO will handle the timing and output
        for(int j=0; j<table_size; j++) {
            // Send each sample, shift left to fit in 32 bits
            pio_sm_put_blocking(i2s_pio_config_out.pio, i2s_pio_config_out.sm, sine_table[j] << 16 | (sine_table[j] & 0xFFFF));
        }
    }

    while(!pio_sm_is_tx_fifo_empty(i2s_pio_config_out.pio, i2s_pio_config_out.sm)) {
        // wait for the TX FIFO to be empty
    }

    #endif
}