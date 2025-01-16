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
#include "hardware/pwm.h"
#include "pirate/bio.h"
#include "system_config.h"
#include "bytecode.h" //needed because modes.h has some functions that use it TODO: move all the opt args and bytecode stuff to a single helper file
#include "pirate.h"
#include "command_struct.h" //needed for same reason as bytecode and needs same fix
#include "commands.h"
#include "modes.h"
#include "binmode/binio.h"
// #include "pirate/pullup.h"
#include "pirate/psu.h"
#include "pirate/amux.h"
#include "binmode/sump.h"
#include "binio_helpers.h"
#include "tusb.h"
#include "commands/global/l_bitorder.h"
#include "commands/global/p_pullups.h"
#include "commands/global/cmd_mcu.h"
#include "commands/global/pwm.h"
#include "timestamp.h"
#include "ui/ui_const.h"
#include "binmode/binmodes.h"
#include "ui/ui_term.h"

uint8_t binmode_debug = 0;

void script_print(const char* str) {
    for (size_t i = 0; i < strlen(str); i++) {
        bin_tx_fifo_put(str[i]);
    }
}

void binmode_terminal_lock(bool lock) {
    system_config.binmode_lock_terminal = lock;
    if (!tud_cdc_n_connected(0)) {
        return;
    }
    if (lock) {
        printf("\r\n%sBinmode active. Terminal locked%s\r\n", ui_term_color_info(), ui_term_color_reset());
    } else {
        printf("\r\n%sTerminal unlocked%s\r\n", ui_term_color_info(), ui_term_color_reset());
    }
}

uint32_t binmode_config(uint8_t* binmode_args) {
    struct _bytecode result;
    struct _bytecode next;
    if (binmode_debug) {
        printf("[CONFIG] Setting up mode\r\n");
    }
    if (modes[system_config.mode].binmode_setup(binmode_args)) {
        return 1;
    }
    modes[system_config.mode].protocol_setup_exc();
    return 0;
}

/* move to protocol layer
uint32_t binmode_write(uint8_t *binmode_args){
    struct _bytecode result;
    struct _bytecode next;
    char c;

    //arg 0 and 1 are number of bytes to write
    uint16_t bytes_to_write=binmode_args[0]<<8 | binmode_args[1];
    //loop through the bytes
    for(uint32_t i=0;i<bytes_to_write;i++){
        bin_rx_fifo_get_blocking(&c);
        result.out_data=c;
        modes[system_config.mode].protocol_write(&result, &next);
        if(result.read_with_write){
            bin_tx_fifo_put(result.in_data);
        }
    }
    return 0;
}
*/

uint32_t binmode_write(uint8_t* binmode_args) {
    struct _bytecode result;
    struct _bytecode next;
    char c;

    result.out_data = binmode_args[0];
    modes[system_config.mode].protocol_write(&result, &next);
    if (result.read_with_write) {
        return result.in_data;
    }

    return 0;
}

uint32_t binmode_start(uint8_t* binmode_args) {
    struct _bytecode result;
    struct _bytecode next;
    modes[system_config.mode].protocol_start(&result, &next);
    return 0;
}

uint32_t binmode_start_alt(uint8_t* binmode_args) {
    struct _bytecode result;
    struct _bytecode next;
    modes[system_config.mode].protocol_start_alt(&result, &next);
    return 0;
}

uint32_t binmode_stop(uint8_t* binmode_args) {
    struct _bytecode result;
    struct _bytecode next;
    modes[system_config.mode].protocol_stop(&result, &next);
    return 0;
}

uint32_t binmode_stop_alt(uint8_t* binmode_args) {
    struct _bytecode result;
    struct _bytecode next;
    modes[system_config.mode].protocol_stop_alt(&result, &next);
    return 0;
}

// same as binmode_write, but with a read
/*uint32_t binmode_read(uint8_t *binmode_args){
    struct _bytecode result;
    struct _bytecode next;

    //arg 0 and 1 are number of bytes to read
    uint16_t bytes_to_read=binmode_args[0]<<8 | binmode_args[1];
    //loop through the bytes
    for(uint32_t i=0;i<bytes_to_read;i++){
        modes[system_config.mode].protocol_read(&result, &next);
        bin_tx_fifo_put(result.in_data);
    }
    modes[system_config.mode].protocol_read(&result, &next);
    return 0;
}*/

uint32_t binmode_read(uint8_t* binmode_args) {
    struct _bytecode result;
    struct _bytecode next;

    modes[system_config.mode].protocol_read(&result, &next);
    return result.in_data;
}

// might have to compare the function to the
// address of the dummy functions to see if
// these apply to the mode
uint32_t binmode_clkh(uint8_t* binmode_args) {
    struct _bytecode result;
    struct _bytecode next;
    modes[system_config.mode].protocol_clkh(&result, &next);
}

uint32_t binmode_clkl(uint8_t* binmode_args) {
    struct _bytecode result;
    struct _bytecode next;
    modes[system_config.mode].protocol_clkl(&result, &next);
}

uint32_t binmode_tick(uint8_t* binmode_args) {
    struct _bytecode result;
    struct _bytecode next;
    modes[system_config.mode].protocol_tick_clock(&result, &next);
}

uint32_t binmode_dath(uint8_t* binmode_args) {
    struct _bytecode result;
    struct _bytecode next;
    modes[system_config.mode].protocol_dath(&result, &next);
}

uint32_t binmode_datl(uint8_t* binmode_args) {
    struct _bytecode result;
    struct _bytecode next;
    modes[system_config.mode].protocol_datl(&result, &next);
}

uint32_t binmode_bitr(uint8_t* binmode_args) {
    struct _bytecode result;
    struct _bytecode next;
    modes[system_config.mode].protocol_bitr(&result, &next);
}

uint32_t binmode_reset(uint8_t* binmode_args) {
    if (binmode_debug) {
        printf("[RESET] Resetting mode\r\n");
    }
    // bin_tx_fifo_put(0);
    // busy_wait_ms(100);
    modes[system_config.mode].protocol_cleanup();
    system_config.mode = 0;
    modes[system_config.mode].protocol_setup();
    return 0;
}

uint32_t binmode_debug_level(uint8_t* binmode_args) {
    if (binmode_args[0] > 1) {
        return 1;
    }
    binmode_debug = binmode_args[0];
    if (binmode_debug) {
        printf("[DEBUG] Enabled\r\n");
    }
    return 0;
}

uint32_t binmode_psu_enable(uint8_t* binmode_args) {
    if (binmode_args[0] > 5 || binmode_args[1] > 99) {
        if (binmode_debug) {
            printf("[PSU] Invalid voltage: %d.%d\r\n", binmode_args[0], binmode_args[1]);
        }
        return 10;
    }
    float voltage = (float)(binmode_args[0]) + (float)((float)binmode_args[1] / 100.00);
    bool current_limit_override = false;
    float current = 0;
    if ((binmode_args[2] == 0xff) && (binmode_args[3] == 0xff)) {
        current_limit_override = true;
    } else {
        current = (binmode_args[2] << 8) + binmode_args[3];
    }
    if (binmode_debug) {
        printf("[PSU] Voltage: %f, Current: %f, Override: %d\r\n", voltage, current, current_limit_override);
    }
    return psu_enable(voltage, current, current_limit_override);
}

uint32_t binmode_psu_disable(uint8_t* binmode_args) {
    psu_disable();
    if (binmode_debug) {
        printf("[PSU] Disabled\r\n");
    }
    return 0;
}

uint32_t binmode_pullup_enable(uint8_t* binmode_args) {
    struct command_result res;
    if (binmode_debug) {
        pullups_enable_handler(&res);
        printf("\r\n");
    } else {
        pullups_enable();
    }
    return 0;
}

uint32_t binmode_pullup_disable(uint8_t* binmode_args) {
    struct command_result res;
    if (binmode_debug) {
        pullups_disable_handler(&res);
        printf("\r\n");
    } else {
        pullups_disable();
    }
    return 0;
}

uint32_t mode_list(uint8_t* binmode_args) {
    for (uint8_t i = 0; i < count_of(modes); i++) {
        if (modes[i].binmode_setup) {
            script_print(modes[i].protocol_name);
            if (i < (count_of(modes) - 1)) {
                bin_tx_fifo_put(',');
            }
        }
    }
    // bin_tx_fifo_put(';');
    return 0;
}

uint32_t mode_change(uint8_t* binmode_args) {
    // compare mode name to modes.protocol_name
    for (uint8_t i = 0; i < count_of(modes); i++) {
        if (modes[i].binmode_setup && strcmp(binmode_args, modes[i].protocol_name) == 0) {
            modes[system_config.mode].protocol_cleanup();
            system_config.mode = i;
            if (modes[system_config.mode].binmode_get_config_length() > BINMODE_MAX_ARGS) {
                if (binmode_debug) {
                    printf("[MODE] Config length too long for arguments buffer\r\n");
                }
                return 1;
            }
            if (binmode_debug) {
                printf("[MODE] Changed to %s, config length %d\r\n",
                       modes[system_config.mode].protocol_name,
                       modes[system_config.mode].binmode_get_config_length());
            }
            return 0;
        }
    }
    return 1;
}

uint32_t binmode_info(uint8_t* binmode_args) {
    script_print("BBIO2.000");
    return 0;
}

uint32_t binmode_hwversion(uint8_t* binmode_args) {
    script_print(BP_HARDWARE_VERSION);
    return 0;
}

uint32_t binmode_fwversion(uint8_t* binmode_args) {
    script_print(BP_FIRMWARE_HASH);
    return 0;
}

uint32_t binmode_bitorder_msb(uint8_t* binmode_args) {
    struct command_result res;
    if (binmode_debug) {
        bitorder_msb_handler(&res);
        printf("\r\n");
    } else {
        bitorder_msb();
    }
    return 0;
}

uint32_t binmode_bitorder_lsb(uint8_t* binmode_args) {
    struct command_result res;
    if (binmode_debug) {
        bitorder_lsb_handler(&res);
        printf("\r\n");
    } else {
        bitorder_lsb();
    }
    return 0;
}

uint32_t binmode_aux_direction_mask(uint8_t* binmode_args) {
    for (uint8_t i = 0; i < 8; i++) {
        if (binmode_args[0] & (1 << i)) {
            if (binmode_args[1] & 1 << i) {
                bio_output(i);
            } else {
                bio_input(i);
            }
            if (binmode_debug) {
                printf("[AUX] set pin %d direction to %d\r\n", i, (bool)(binmode_args[1] & (1 << i)));
            }
        }
    }
    return 0;
}

uint32_t binmode_aux_read(uint8_t* binmode_args) {
    uint32_t temp = gpio_get_all();
    temp = temp >> 8;
    temp = temp & 0xFF;
    if (binmode_debug) {
        printf("[AUX] read: %d\r\n", temp);
    }
    return temp;
}

// two byte argument: pins, output mask
uint32_t binmode_aux_write_mask(uint8_t* binmode_args) {
    for (uint8_t i = 0; i < 8; i++) {
        if (binmode_args[0] & (1 << i)) {
            bio_output(i); // for safety
            bio_put(i, (binmode_args[1] & 1 << i));
            if (binmode_debug) {
                printf("[AUX] write pin %d to %d\r\n", i, (bool)(binmode_args[1] & (1 << i)));
            }
        }
    }
    return 0;
}

// 1 argument - ADC#
/*
    HW_ADC_MUX_BPIO7, //0
    HW_ADC_MUX_BPIO6, //1
    HW_ADC_MUX_BPIO5, //2
    HW_ADC_MUX_BPIO4, //3
    HW_ADC_MUX_BPIO3, //4
    HW_ADC_MUX_BPIO2, //5
    HW_ADC_MUX_BPIO1, //6
    HW_ADC_MUX_BPIO0, //7
    HW_ADC_MUX_VUSB, //8
    HW_ADC_MUX_CURRENT_DETECT,    //9
    HW_ADC_MUX_VREG_OUT, //10
    HW_ADC_MUX_VREF_VOUT, //11
    CURRENT_SENSE //12 */
uint8_t binmode_adc_channel = 0;
uint32_t binmode_adc_select(uint8_t* binmode_args) {

    if (binmode_args[0] > 12) {
        if (binmode_debug) {
            printf("[ADC] Invalid channel %d\r\n", binmode_args[0]);
        }
        return 1;
    }

    if (binmode_args[0] > 11) {
        amux_select_input(binmode_args[0]);
    } else if (binmode_args[0] == 12) {
        amux_read_current(); // not really needed, but can be improved in the future
    }
    binmode_adc_channel = binmode_args[0];
    if (binmode_debug) {
        printf("[ADC] selected channel %d\r\n", binmode_args[0]);
    }
    return 0;
}

uint32_t binmode_adc_read(uint8_t* binmode_args) {
    uint32_t temp;
    if (binmode_adc_channel < 12) { // divide by two channels
        temp = amux_read(binmode_adc_channel);
        temp = ((6600 * temp) / 4096);
    } else {
        temp = amux_read_current();
        temp = ((temp * 3300) / 4096);
    }
    if (binmode_debug) {
        printf("[ADC] channel %d read %d.%d\r\n", binmode_adc_channel, temp / 1000, (temp % 1000) / 100);
    }
    bin_tx_fifo_put(temp / 1000);
    return (temp % 1000 / 100);
}

uint32_t binmode_adc_raw(uint8_t* binmode_args) {
    uint32_t temp;
    if (binmode_adc_channel < 12) { // divide by two channels
        temp = amux_read(binmode_adc_channel);
    } else {
        temp = amux_read_current();
    }
    if (binmode_debug) {
        printf("[ADC] channel %d raw read %d\r\n", binmode_adc_channel, temp);
    }
    bin_tx_fifo_put(temp >> 8);
    return temp & 0xFF;
}

// arguments 6 bytes: IO pin, frequency Hz, duty cycle 0-100
//  0 62500000 50
//  0x00 0x3 B9 AC A0 0x32
uint32_t binmode_pwm_enable(uint8_t* binmode_args) {

    // label should be 0, not in use
    // FREQ on the B channel should not be in use!
    // PWM should not already be in use on A or B channel of this slice

    // bounds check
    if ((binmode_args[0]) >= count_of(bio2bufiopin)) {
        return 1;
    }

// temp fix for power supply PWM sharing
#if BP5_REV <= 8
    if ((binmode_args[0]) == 0 || (binmode_args[0]) == 1) {
        return 1;
    }
#endif

    // not active or used for frequency
    if (!(system_config.pin_labels[(binmode_args[0]) + 1] == 0 &&
          !(system_config.freq_active &
            (0b11 << ((uint8_t)((binmode_args[0]) % 2 ? (binmode_args[0]) - 1 : (binmode_args[0]))))) &&
          !(system_config.pwm_active &
            (0b11 << ((uint8_t)((binmode_args[0]) % 2 ? (binmode_args[0]) - 1 : (binmode_args[0]))))))) {
        return 2;
    }

    uint slice_num = pwm_gpio_to_slice_num(bio2bufiopin[(uint8_t)binmode_args[0]]);
    uint chan_num = pwm_gpio_to_channel((uint8_t)binmode_args[0]);

    float freq_user_value, pwm_hz_actual_temp, pwm_ns_actual;
    uint32_t freq_int = binmode_args[1] << 24 | binmode_args[2] << 16 | binmode_args[3] << 8 | binmode_args[4];
    freq_user_value = (float)freq_int;

    if (binmode_debug) {
        printf("[PWM] %fHz\r\n", freq_user_value);
    }

    uint32_t pwm_divider, pwm_top, pwm_duty;
    if (pwm_freq_find(&freq_user_value, &pwm_hz_actual_temp, &pwm_ns_actual, &pwm_divider, &pwm_top)) {
        return 3;
    }
    pwm_duty = ((float)(pwm_top) * (float)((float)binmode_args[5] / 100.0)) + 1;

    system_config.freq_config[binmode_args[0]].period = pwm_hz_actual_temp;
    system_config.freq_config[binmode_args[0]].dutycycle = pwm_duty;

    pwm_set_clkdiv_int_frac(slice_num, pwm_divider >> 4, pwm_divider & 0b1111);
    pwm_set_wrap(slice_num, pwm_top);
    pwm_set_chan_level(slice_num, chan_num, pwm_duty);

    bio_buf_output((uint8_t)binmode_args[0]);
    gpio_set_function(bio2bufiopin[(uint8_t)binmode_args[0]], GPIO_FUNC_PWM);
    pwm_set_enabled(slice_num, true);

    // register the freq active, apply the pin label
    system_bio_update_purpose_and_label(true, (uint8_t)binmode_args[0], BP_PIN_PWM, ui_const_pin_states[3]);
    system_set_active(true, (uint8_t)binmode_args[0], &system_config.pwm_active);
    return 0;
}

uint32_t binmode_pwm_disable(uint8_t* binmode_args) {
    // bounds check
    if ((binmode_args[0]) >= count_of(bio2bufiopin)) {
        return 1;
    }

    if (!(system_config.pwm_active & (0x01 << binmode_args[0]))) {
        if (binmode_debug) {
            printf("[PWM] Not active on IO %d\r\n", binmode_args[0]);
        }
        return 2;
    }
    // disable
    pwm_set_enabled(pwm_gpio_to_slice_num(bio2bufiopin[(uint8_t)binmode_args[0]]), false);
    gpio_set_function(bio2bufiopin[(uint8_t)binmode_args[0]], GPIO_FUNC_SIO);
    bio_input((uint8_t)binmode_args[0]);

    // unregister, remove pin label
    system_bio_update_purpose_and_label(false, (uint8_t)binmode_args[0], 0, 0);
    system_set_active(false, (uint8_t)binmode_args[0], &system_config.pwm_active);

    if (binmode_debug) {
        printf("[PWM] Disabled\r\n");
    }
    return 0;
}

uint32_t binmode_pwm_raw(uint8_t* binmode_args) {
    // label should be 0, not in use
    // FREQ on the B channel should not be in use!
    // PWM should not already be in use on A or B channel of this slice

    // bounds check
    if ((binmode_args[0]) >= count_of(bio2bufiopin)) {
        return 1;
    }

// temp fix for power supply PWM sharing
#if BP5_REV <= 8
    if ((binmode_args[0]) == 0 || (binmode_args[0]) == 1) {
        return 1;
    }
#endif

    // not active or used for frequency
    if (!(system_config.pin_labels[(binmode_args[0]) + 1] == 0 &&
          !(system_config.freq_active &
            (0b11 << ((uint8_t)((binmode_args[0]) % 2 ? (binmode_args[0]) - 1 : (binmode_args[0]))))) &&
          !(system_config.pwm_active &
            (0b11 << ((uint8_t)((binmode_args[0]) % 2 ? (binmode_args[0]) - 1 : (binmode_args[0]))))))) {
        return 2;
    }

    uint slice_num = pwm_gpio_to_slice_num(bio2bufiopin[(uint8_t)binmode_args[0]]);
    uint chan_num = pwm_gpio_to_channel((uint8_t)binmode_args[0]);

    uint16_t pwm_divider = binmode_args[1] << 8 | binmode_args[2];
    uint16_t pwm_top = binmode_args[3] << 8 | binmode_args[4];
    uint16_t pwm_duty = binmode_args[5] << 8 | binmode_args[6];
    if (binmode_debug) {
        printf("[PWM] top %d, divider %d, duty %d\r\n", pwm_top, pwm_divider, pwm_duty);
    }

    pwm_set_clkdiv_int_frac(slice_num, pwm_divider >> 4, pwm_divider & 0b1111);
    pwm_set_wrap(slice_num, pwm_top);
    pwm_set_chan_level(slice_num, chan_num, pwm_duty);

    bio_buf_output((uint8_t)binmode_args[0]);
    gpio_set_function(bio2bufiopin[(uint8_t)binmode_args[0]], GPIO_FUNC_PWM);
    pwm_set_enabled(slice_num, true);

    // register the freq active, apply the pin label
    system_bio_update_purpose_and_label(true, (uint8_t)binmode_args[0], BP_PIN_PWM, ui_const_pin_states[3]);
    system_set_active(true, (uint8_t)binmode_args[0], &system_config.pwm_active);
    return 0;
}

uint32_t binmode_freq(uint8_t* binmode_args) {
    if (binmode_debug) {
        printf("[FREQ] %d.%d\r\n", binmode_args[0], binmode_args[1]);
    }
    return 0;
}

uint32_t binmode_freq_raw(uint8_t* binmode_args) {
    if (binmode_debug) {
        printf("[FREQ] %d.%d\r\n", binmode_args[0], binmode_args[1]);
    }
    return 0;
}

uint32_t binmode_delay_us(uint8_t* binmode_args) {
    if (binmode_debug) {
        printf("[DELAY] %dus\r\n", binmode_args[0]);
    }
    busy_wait_us(binmode_args[0]);
    return 0;
}

uint32_t binmode_delay_ms(uint8_t* binmode_args) {
    if (binmode_debug) {
        printf("[DELAY] %dms\r\n", binmode_args[0]);
    }
    busy_wait_ms(binmode_args[0]);
    return 0;
}

uint32_t binmode_bootloader(uint8_t* binmode_args) {
    if (binmode_debug) {
        printf("[BOOTLOADER] Jumping to bootloader\r\n");
        busy_wait_ms(100);
    }
    cmd_mcu_jump_to_bootloader();
    return 0;
}

uint32_t binmode_reset_buspirate(uint8_t* binmode_args) {
    if (binmode_debug) {
        printf("[RESET] Resetting Bus Pirate\r\n");
        busy_wait_ms(100);
    }
    cmd_mcu_reset();
    return 0;
}

uint32_t binmode_change_binmode(uint8_t* binmode_args) {
    if (binmode_debug) {
        printf("[BINMODE] Changing to %d\r\n", binmode_args[0]);
    }
    if (binmode_args[0] >= count_of(binmodes)) {
        return 1;
    }
    system_config.binmode_select = binmode_args[0];
    return 0;
}
