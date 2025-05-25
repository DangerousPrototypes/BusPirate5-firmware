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
#include "command_struct.h"       // File system related
#include "fatfs/ff.h"       // File system related
#include "pirate/storage.h" // File system related
#include "ui/ui_cmdln.h"    // This file is needed for the command line parsing functions
// #include "ui/ui_prompt.h" // User prompts and menu system
// #include "ui/ui_const.h"  // Constants and strings
#include "ui/ui_help.h"    // Functions to display help in a standardized way
#include "system_config.h" // Stores current Bus Pirate system configuration
#include "pirate/amux.h"   // Analog voltage measurement functions
#include "pirate/button.h" // Button press functions
#include "msc_disk.h"
#include "hardware/pio.h"
#include "bytecode.h"   // Bytecode structure for data IO
#include "pio_config.h"
#include "mode/i2s.h"
// This array of strings is used to display help USAGE examples for the dummy command
static const char* const usage[] = { "sine [Hz]",
                                     "Generate a sine wave at the default frequency (1000Hz): sine",                                 
                                     "Generate 2000Hz sine wave: sine 2000",
                                     "Generate 1000Hz sine wave for 5 seconds: sine 1000 -s 5" };

// This is a struct of help strings for each option/flag/variable the command accepts
// Record type 1 is a section header
// Record type 0 is a help item displayed as: "command" "help text"
// This system uses the T_ constants defined in translation/ to display the help text in the user's preferred language
// To add a new T_ constant:
//      1. open the master translation en-us.h
//      2. add a T_ tag and the help text
//      3. Run json2h.py, which will rebuild the translation files, adding defaults where translations are missing
//      values
//      4. Use the new T_ constant in the help text for the command
static const struct ui_help_options options[] = {
    { 1, "", T_HELP_DUMMY_COMMANDS },    // section heading
    { 0, "init", T_HELP_DUMMY_INIT },    // init is an example we'll find by position
    { 0, "test", T_HELP_DUMMY_TEST },    // test is an example we'll find by position
    { 1, "", T_HELP_DUMMY_FLAGS },       // section heading for flags
    { 0, "-b", T_HELP_DUMMY_B_FLAG },    //-a flag, with no optional string or integer
    { 0, "-i", T_HELP_DUMMY_I_FLAG },    //-b flag, with optional integer
    { 0, "-f", T_HELP_DUMMY_FILE_FLAG }, //-f flag, a file name string
};

void sine_handler(struct command_result* res) {
    uint32_t value; // somewhere to keep an integer value
    char file[13];  // somewhere to keep a string value (8.3 filename + 0x00 = 13 characters max)

    // the help -h flag can be serviced by the command line parser automatically, or from within the command
    // the action taken is set by the help_text variable of the command struct entry for this command
    // 1. a single T_ constant help entry assigned in the commands[] struct in commands.c will be shown automatically
    // 2. if the help assignment in commands[] struct is 0x00, it can be handled here (or ignored)
    // res.help_flag is set by the command line parser if the user enters -h
    // we can use the ui_help_show function to display the help text we configured above
    if (ui_help_show(res->help_flag, usage, count_of(usage), &options[0], count_of(options))) {
        return;
    }

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
    if(cmdln_args_uint32_by_position(1, &temp)){
        sine_frequency_hz = temp; // Get the sine frequency from the command line argument
    }

    command_var_t arg;
    cmdln_args_find_flag_uint32('s', &arg, &duration_seconds);
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
}