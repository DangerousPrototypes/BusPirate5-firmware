#include <stdio.h>
#include "pico/stdlib.h"
#include "pirate.h"
#include "system_config.h"
#include "command_struct.h"
#include "hardware/pio.h"
#include "hardware/timer.h"
#include "hardware/clocks.h"
#include "i2c_sniffer.pio.h" 
#include "pio_config.h"  
#include "bytecode.h" 
#include "mode/hwi2c.h"
#include "pirate/bio.h"
#include "ui/ui_help.h"    // Functions to display help in a standardized way
#include "usb_rx.h"
#include "usb_tx.h"
#include "ui/ui_cmdln.h"    // This file is needed for the command line parsing functions

static const char pin_labels[][5] = {
    "SDA",
    "SCL",
    "EV0",
    "EV1",
};

//help variables
const char* const i2c_sniff_help[] = {
    "sniff [-q]",
    "Start the I2C sniffer: sniff",
    "Supress (quiet) ACK in output: sniff -q",
    "",
    "pico-i2c-sniff by @jjsch-dev https://github.com/jjsch-dev/pico_i2c_sniffer"
};

const struct ui_help_options i2c_sniff_options[] = {
    { 1, "", T_HELP_FLAG },
    { 0, "", T_HELP_FLAG },
};

void i2c_sniff(struct command_result* res){ 
    //if -h show help
    if (ui_help_show(res->help_flag, i2c_sniff_help, count_of(i2c_sniff_help), &i2c_sniff_options[0], count_of(i2c_sniff_options))) {
        return;
    }

    //check arguments for quiet mode
    bool quiet = cmdln_args_find_flag('q');

    // Full speed for the PIO clock divider
    float div = 1;
    struct _pio_config pio_main, pio_data, pio_start, pio_stop;

    printf("I2C Sniffer\r\n");

    //tear down I2C PIO
    hwi2c_cleanup();

    //buffers to input/outputs
    bio_input(M_I2C_SDA); //SDA
    bio_input(M_I2C_SCL); //SCL    
    bio_output(BIO2); //EVENT CODE 1
    bio_output(BIO3); //EVENT CODE 2

    //setup pin labels
    system_bio_update_purpose_and_label(true, M_I2C_SDA, BP_PIN_MODE, pin_labels[0]);
    system_bio_update_purpose_and_label(true, M_I2C_SCL, BP_PIN_MODE, pin_labels[1]);
    system_bio_update_purpose_and_label(true, BIO2, BP_PIN_MODE, pin_labels[2]);
    system_bio_update_purpose_and_label(true, BIO3, BP_PIN_MODE, pin_labels[3]);


    // Initialize the four state machines that decode the i2c bus states.
    pio_main.pio = PIO_MODE_PIO;
    pio_main.sm = 0;
    pio_main.program = &i2c_main_program;
    pio_main.offset = pio_add_program(pio_main.pio, pio_main.program);
    i2c_main_program_init(pio_main.pio, pio_main.sm, pio_main.offset, div);


    pio_data.pio = PIO_MODE_PIO;
    pio_data.sm = 1;
    pio_data.program = &i2c_data_program;
    pio_data.offset = pio_add_program(pio_data.pio, pio_data.program);
    i2c_data_program_init(pio_data.pio, pio_data.sm, pio_data.offset, div);

    pio_start.pio = PIO_MODE_PIO;
    pio_start.sm = 2;
    pio_start.program = &i2c_start_program;
    pio_start.offset = pio_add_program(pio_start.pio, pio_start.program);
    i2c_start_program_init(pio_start.pio, pio_start.sm, pio_start.offset, div);

    pio_stop.pio = PIO_MODE_PIO;
    pio_stop.sm = 3;
    pio_stop.program = &i2c_stop_program;
    pio_stop.offset = pio_add_program(pio_stop.pio, pio_stop.program);
    i2c_stop_program_init(pio_stop.pio, pio_stop.sm, pio_stop.offset, div);

    // Start running our PIO program in the state machine
    pio_sm_set_enabled(pio_main.pio, pio_main.sm, true);
    pio_sm_set_enabled(pio_start.pio, pio_start.sm, true);
    pio_sm_set_enabled(pio_stop.pio, pio_stop.sm, true);
    pio_sm_set_enabled(pio_data.pio, pio_data.sm, true);

    printf("Press x to exit\r\n");

    if(quiet){  // we duplicate the loop to avoid the quiet test inside the time sensitive parts
                // this is cheating, we really should have a buffer and a separate thread to handle the output
        printf("Quiet mode enabled, ACKs will not be displayed\r\n");
        while(true){
            bool new_val = (pio_sm_get_rx_fifo_level(pio_main.pio, pio_main.sm ) > 0);
            if (new_val) {
                uint32_t val = pio_sm_get(pio_main.pio, pio_main.sm);
                // The format of the uint32_t returned by the sniffer is composed of two event
                // code bits (EV1 = Bit13, EV0 = Bit12), and when it comes to data, the nine least
                // significant bits correspond to (ACK = Bit0), and the value 8 bits
                // where (B0 = Bit1 and B7 = Bit8).
                uint32_t ev_code = (val >> 11) & 0x03;
                uint8_t  data = ((val >> 1) & 0xFF);
                bool ack = (val & 1) ? false : true;
                //printf("val: %x, ev_code: %x, data:%x, ack: %d \r\n", val, ev_code, data, ack);
                if (ev_code == EV_START) {
                    printf("[");
                } else if (ev_code == EV_STOP) {
                    printf("]\r\n");
                } else if (ev_code == EV_DATA) {
                    if(ack){
                        printf(" 0x%02X\r\n", data);
                    }else{
                        printf(" 0x%02X-\r\n", data);
                    }
                } else {
                    printf("U\r\n");
                }            
            }

            // x to exit
            char c;
            if(rx_fifo_try_get(&c)){
                if(c == 'x'){
                    break;
                }
            }

        }
    }else{
        while(true){
            bool new_val = (pio_sm_get_rx_fifo_level(pio_main.pio, pio_main.sm ) > 0);
            if (new_val) {
                uint32_t val = pio_sm_get(pio_main.pio, pio_main.sm);
                // The format of the uint32_t returned by the sniffer is composed of two event
                // code bits (EV1 = Bit13, EV0 = Bit12), and when it comes to data, the nine least
                // significant bits correspond to (ACK = Bit0), and the value 8 bits
                // where (B0 = Bit1 and B7 = Bit8).
                uint32_t ev_code = (val >> 11) & 0x03;
                uint8_t  data = ((val >> 1) & 0xFF);
                bool ack = (val & 1) ? false : true;
                //printf("val: %x, ev_code: %x, data:%x, ack: %d \r\n", val, ev_code, data, ack);
                if (ev_code == EV_START) {
                    printf("[");
                } else if (ev_code == EV_STOP) {
                    printf("]\r\n");
                } else if (ev_code == EV_DATA) {
                    printf(" 0x%02X%c", data, (ack ? '+' : '-'));
                } else {
                    printf("U\r\n");
                }            
            }

            // x to exit
            char c;
            if(rx_fifo_try_get(&c)){
                if(c == 'x'){
                    break;
                }
            }

        }
    }

    //remove pin labels
    system_bio_update_purpose_and_label(false, M_I2C_SDA, BP_PIN_MODE, 0);
    system_bio_update_purpose_and_label(false, M_I2C_SCL, BP_PIN_MODE, 0);
    system_bio_update_purpose_and_label(false, BIO2, BP_PIN_MODE, 0);
    system_bio_update_purpose_and_label(false, BIO3, BP_PIN_MODE, 0);

    //remove sniff PIO programs
    pio_remove_program(pio_main.pio, pio_main.program, pio_main.offset);
    pio_remove_program(pio_data.pio, pio_data.program, pio_data.offset);
    pio_remove_program(pio_start.pio, pio_start.program, pio_start.offset);
    pio_remove_program(pio_stop.pio, pio_stop.program, pio_stop.offset);

    //on exit, restore the I2C PIO
    hwi2c_setup_exc();

    #if 0
    multicore_launch_core1(core1_print);

    // Initialize the ram FIFO with a depth of 40K captures.
    if (!ram_fifo_init(40000)) {
        while(true);
    }
    
    gpio_put(led_pin, true);

    // CPU 0 captures the data generated by the i2c sniffer and uses the multicore 
    // FIFO (hardware) to send it to CPU 1, which is in charge of processing and printing 
    // it on the console. If the multicore Fifo is full, it stores them in the ram Fifo. 
    // All this without blocking to be able to attend the FIFO of the sniffer.
    while (true) {
        bool new_val = (pio_sm_get_rx_fifo_level(pio, sm_main) > 0);
        if (new_val) {
            capture_val = pio_sm_get(pio, sm_main);
        }
        // NOTE: activates a flag when it detects that RAM FIFO overflows
        if (multicore_fifo_wready()) {
            if (!ram_fifo_is_empty()) {
                if (new_val) {
                    if (!ram_fifo_set(capture_val) ){
                        ram_fifo_overflow = true;
                    }
                }
                capture_val = ram_fifo_get();
                new_val = true;
            } 
            if (new_val) {
                multicore_fifo_push_blocking(capture_val);
            }
        } else if (new_val) {
            if (!ram_fifo_set(capture_val)) {
                ram_fifo_overflow = true;
            }
        }
    }
    #endif
}