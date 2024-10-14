/*
 * This file is part of the Bus Pirate project (http://code.google.com/p/the-bus-pirate/).
 *
 * Written and maintained by the Bus Pirate project.
 *
 * To the extent possible under law, the project has
 * waived all copyright and related or neighboring rights to Bus Pirate. This
 * work is published from United States.
 *
 * For details see: http://creativecommons.org/publicdomain/zero/1.0/.
 *
 *    This program is distributed in the hope that it will be useful,
 *    but WITHOUT ANY WARRANTY; without even the implied warranty of
 *    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 */

/* Binary access modes for Bus Pirate scripting */

// #include <stdio.h>
#include <string.h>
#include "pico/stdlib.h"
#include "pirate.h"
#include "queue.h"
#include "usb_rx.h"
#include "usb_tx.h"
#include "pirate.h"
// #include "sump.h"
#include "opt_args.h" //needed for same reason as bytecode and needs same fix
#include "bytecode.h"
#include "modes.h"
#include "binio_helpers.h"
#include "tusb.h"
#include "binmode/binio.h"
#include "system_config.h"

struct _binmode_struct {
    uint32_t (*func)(uint8_t* data);
    int arg_count;
};

enum {
    BM_RESET = 0,
    BM_DEBUG_LEVEL,        // 1
    BM_POWER_EN,           // 2
    BM_POWER_DIS,          // 3
    BM_PULLUP_EN,          // 4
    BM_PULLUP_DIS,         // 5
    BM_LIST_MODES,         // 6
    BM_CHANGE_MODE,        // 7
    BM_INFO,               // 8
    BM_VERSION_HW,         // 9
    BM_VERSION_FW,         // 10
    BM_BITORDER_MSB,       // 11
    BM_BITORDER_LSB,       // 12
    BM_AUX_DIRECTION_MASK, // 13
    BM_AUX_READ,           // 14
    BM_AUX_WRITE_MASK,     // 15
    BM_ADC_SELECT,         // 16
    BM_ADC_READ,           // 17
    BM_ADC_RAW,            // 18
    BM_PWM_EN,             // 19
    BM_PWM_DIS,            // 20
    BM_PWM_RAW,            // 21
    // BM_PWM_ACTUAL_FREQ, //22
    BM_FREQ,            // 23
    BM_FREQ_RAW,        // 24
    BM_DELAY_US,        // 25
    BM_DELAY_MS,        // 26
    BM_BOOTLOADER,      // 27
    BM_RESET_BUSPIRATE, // 28
    BM_PRINT_STRING,    // 29
    BM_CHANGE_BINMODE,
    // BM_BINMODE_VERSION, //30
    // self test
    // disable all interrupt
    // enable all interrupt
    // LEDs?

    BM_CONFIG,
    BM_WRITE,
    BM_START,
    BM_START_ALT,
    BM_STOP,
    BM_STOP_ALT,
    BM_READ,
    BM_CLKH,
    BM_CLKL,
    BM_TICK,
    BM_DATH,
    BM_DATL,
    BM_BITR,
};

static const struct _binmode_struct binmode_commands[] = {
    [BM_RESET] = { &binmode_reset, 0 },
    [BM_DEBUG_LEVEL] = { &binmode_debug_level, 1 },
    [BM_POWER_EN] = { &binmode_psu_enable, 4 },
    [BM_POWER_DIS] = { &binmode_psu_disable, 0 },
    [BM_PULLUP_EN] = { &binmode_pullup_enable, 0 },
    [BM_PULLUP_DIS] = { &binmode_pullup_disable, 0 },
    [BM_LIST_MODES] = { &mode_list, 0 },
    [BM_CHANGE_MODE] = { &mode_change, -1 },
    [BM_INFO] = { &binmode_info, 0 },
    [BM_VERSION_HW] = { &binmode_hwversion, 0 },
    [BM_VERSION_FW] = { &binmode_fwversion, 0 },
    [BM_BITORDER_MSB] = { &binmode_bitorder_msb, 0 },
    [BM_BITORDER_LSB] = { &binmode_bitorder_lsb, 0 },
    [BM_AUX_DIRECTION_MASK] = { &binmode_aux_direction_mask, 2 },
    [BM_AUX_READ] = { &binmode_aux_read, 0 },
    [BM_AUX_WRITE_MASK] = { &binmode_aux_write_mask, 2 },
    [BM_ADC_SELECT] = { &binmode_adc_select, 1 },
    [BM_ADC_READ] = { &binmode_adc_read, 0 },
    [BM_ADC_RAW] = { &binmode_adc_raw, 0 },
    [BM_PWM_EN] = { &binmode_pwm_enable, 6 },
    [BM_PWM_DIS] = { &binmode_pwm_disable, 1 },
    [BM_PWM_RAW] = { &binmode_pwm_raw, 7 },
    [BM_FREQ] = { &binmode_freq, 2 },
    [BM_FREQ_RAW] = { &binmode_freq_raw, 2 },
    [BM_DELAY_US] = { &binmode_delay_us, 1 },
    [BM_DELAY_MS] = { &binmode_delay_ms, 1 },
    [BM_BOOTLOADER] = { &binmode_bootloader, 0 },
    [BM_RESET_BUSPIRATE] = { &binmode_reset_buspirate, 0 },
    [BM_PRINT_STRING] = { 0, 0 },
    [BM_CHANGE_BINMODE] = { &binmode_change_binmode, 1 },
    // mode commands
    [BM_CONFIG] = { &binmode_config, 0 },
    [BM_WRITE] = { &binmode_write, 1 },
    [BM_START] = { &binmode_start, 0 },
    [BM_START_ALT] = { &binmode_start_alt, 0 },
    [BM_STOP] = { &binmode_stop, 0 },
    [BM_STOP_ALT] = { &binmode_stop_alt, 0 },
    [BM_READ] = { &binmode_read, 0 },
    [BM_CLKH] = { &binmode_clkh, 0 },
    [BM_CLKL] = { &binmode_clkl, 0 },
    [BM_TICK] = { &binmode_tick, 0 },
    [BM_DATH] = { &binmode_dath, 0 },
    [BM_DATL] = { &binmode_datl, 0 },
    [BM_BITR] = { &binmode_bitr, 0 },
};

enum binmode_statemachine {
    BINMODE_COMMAND = 0,
    BINMODE_GET_ARGS,
    BINMODE_GET_NULL_TERM,
    BIMNODE_DO_COMMAND,
    BINMODE_PRINT_STRING,
};

const char dirtyproto_mode_name[] = "Binmode test framework";

// handler needs to be cooperative multitasking until mode is enabled
void dirtyproto_mode(void) {
    static uint8_t binmode_state = BINMODE_COMMAND;
    static uint8_t binmode_command;
    static uint8_t binmode_args[BINMODE_MAX_ARGS];
    static uint8_t binmode_arg_count = 0;
    static uint8_t binmode_arg_total = 0;
    // static uint8_t binmode_null_count=0;
    // could activate binmode just by opening the port?
    // if(!tud_cdc_n_connected(1)) return false;

    char c;
    uint32_t temp;
    if (bin_rx_fifo_try_get(&c)) {
        switch (binmode_state) {
            case BINMODE_COMMAND:
                if (c >= count_of(binmode_commands)) {
                    if (binmode_debug) {
                        printf("[MAIN] Invalid command %d\r\n", c);
                    }
                    bin_tx_fifo_put(1);
                    break;
                }

                if (binmode_debug) {
                    printf("[MAIN] Global command %d, args: %d\r\n", c, binmode_commands[c].arg_count);
                }
                binmode_command = c;
                binmode_arg_count = 0;
                binmode_arg_total = 0;

                if (binmode_command == BM_PRINT_STRING) {
                    binmode_state = BINMODE_PRINT_STRING;
                    break;
                } else if (binmode_command == BM_CONFIG) {
                    binmode_state = BINMODE_GET_ARGS;
                    binmode_arg_total = modes[system_config.mode].binmode_get_config_length();
                    if (binmode_debug) {
                        printf("[MAIN] Mode config length %d\r\n", binmode_arg_total);
                    }
                    if (binmode_arg_total == 0) {
                        goto do_binmode_command;
                    }
                    break;
                }

                if (binmode_commands[c].arg_count > 0) {
                    binmode_arg_total = binmode_commands[c].arg_count;
                    binmode_state = BINMODE_GET_ARGS;
                } else if (binmode_commands[c].arg_count < 0) {
                    binmode_state = BINMODE_GET_NULL_TERM;
                } else {
                    goto do_binmode_command;
                }
                break;
            case BINMODE_GET_ARGS:
                binmode_args[binmode_arg_count] = c;
                binmode_arg_count++;
                if (binmode_arg_count == binmode_arg_total) {
                    goto do_binmode_command;
                }
                break;
            case BINMODE_GET_NULL_TERM:
                binmode_args[binmode_arg_count] = c;
                binmode_arg_count++;
                if (c == 0x00) {
                    goto do_binmode_command;
                }
                if (binmode_arg_count >= BINMODE_MAX_ARGS) {
                    binmode_state = BINMODE_COMMAND;
                    if (binmode_debug) {
                        printf("[MAIN] Null terminated data too long\r\n");
                    }
                    bin_tx_fifo_put(1);
                }
                break;
            case BIMNODE_DO_COMMAND:
            do_binmode_command:
                temp = binmode_commands[binmode_command].func(binmode_args);
                if (binmode_debug) {
                    printf("[MAIN] Command %d returned %d\r\n", binmode_command, temp);
                }
                bin_tx_fifo_put(temp);
                binmode_state = BINMODE_COMMAND;
            case BINMODE_PRINT_STRING:
                if (c == 0x00) {
                    printf("\r\n");
                    binmode_state = BINMODE_COMMAND;
                    break;
                }
                printf("%c", c);
                break;
        }
    }
}
