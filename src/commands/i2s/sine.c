// Sine wave generator for I2S mode
#include <stdio.h>
#include <math.h>
#include "pico/stdlib.h"
#include "pirate.h"
#include "command_struct.h"
#include "lib/bp_args/bp_cmd.h"
#include "ui/ui_help.h"
#include "hardware/pio.h"
#include "pio_config.h"
#include "mode/i2s.h"

static const char* const usage[] = { "sine [Hz] [-s seconds]",
                                     "Generate a sine wave at the default frequency (1000Hz):%s sine",
                                     "Generate 2000Hz sine wave:%s sine 2000",
                                     "Generate 1000Hz sine wave for 5 seconds:%s sine 1000 -s 5" };

static const bp_command_opt_t sine_opts[] = {
    { "seconds", 's', BP_ARG_REQUIRED, NULL, T_HELP_DUMMY_COMMANDS },
    { 0 }
};

static const bp_command_positional_t sine_positionals[] = {
    { "Hz", NULL, T_HELP_DUMMY_COMMANDS, false },
    { 0 }
};

const bp_command_def_t sine_def = {
    .name         = "sine",
    .description  = T_HELP_DUMMY_COMMANDS,
    .opts         = sine_opts,
    .positionals      = sine_positionals,
    .positional_count = 1,
    .usage        = usage,
    .usage_count  = count_of(usage),
};

#define MAX_TABLE_SIZE 500
#define AMPLITUDE 32767.0  // Max amplitude for 16-bit signed
#define CYCLES 10

void sine_handler(struct command_result* res) {

    if (bp_cmd_help_check(&sine_def, res->help_flag)) {
        return;
    }

    int16_t sine_table[MAX_TABLE_SIZE];
    uint32_t sample_frequency_hz = i2s_mode_config.freq; // Get the frequency from the mode config
    uint32_t sine_frequency_hz = 1000; // 1kHz sine wave
    int32_t cycle_multiplier = -1; // Default multiplier for cycles
    uint32_t table_size;
    uint32_t duration_seconds = 2; // Duration of the sine wave output in seconds
    
    uint32_t temp;
    if (bp_cmd_get_positional_uint32(&sine_def, 1, &temp)) {
        sine_frequency_hz = temp;
    }

    uint32_t dur;
    if (bp_cmd_get_uint32(&sine_def, 's', &dur)) {
        duration_seconds = dur;
    }
    printf("Sine Wave: %dHz @ %dHz sample rate, %d seconds\r\n", sine_frequency_hz, sample_frequency_hz, duration_seconds);

    // Determine if the sample/sine has a remainder
    // If sample/sine has a remainder, attempt the 10 cycle loop, if it will fit in MAX_TABLE_SIZE
    if ((sample_frequency_hz % sine_frequency_hz) == 0) {
        cycle_multiplier = 1;
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

    if (table_size > MAX_TABLE_SIZE) {
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

    // Calculate how many cycles for the requested duration
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

    while (!pio_sm_is_tx_fifo_empty(i2s_pio_config_out.pio, i2s_pio_config_out.sm)) {
        // wait for the TX FIFO to be empty
    }
}