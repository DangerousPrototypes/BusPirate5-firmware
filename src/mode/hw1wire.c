#include <stdbool.h>
#include <stdio.h>
#include "pico/stdlib.h"
#include <stdint.h>
#include <string.h>
#include "pirate.h"
#include "system_config.h"
#include "command_struct.h"
#include "bytecode.h"
#include "mode/hw1wire.h"
#include "pirate/bio.h"
#include "ui/ui_prompt.h"
#include "pirate/storage.h"
#include "ui/ui_term.h"
#include "hardware/pio.h"
#include "pico/binary_info.h"
#include "hardware/clocks.h"
#include "ui/ui_help.h"
#include "commands/1wire/scan.h"
#include "commands/1wire/demos.h"
#include "commands/eeprom/eeprom_1wire.h"

#define BP_OLD_HW1WIRE
#ifdef BP_OLD_HW1WIRE
#include "pirate/hw1wire_pio.h"
#else
#include "pirate/onewire_library.h"
#endif

// command configuration
const struct _mode_command_struct hw1wire_commands[] = {
    {   .command="scan", 
        .func=&onewire_test_romsearch, 
        .description_text=T_HELP_1WIRE_SCAN, 
        .supress_fala_capture=true
    },
    {   .command="eeprom", 
        .func=&onewire_eeprom_handler, 
        .description_text=T_HELP_1WIRE_EEPROM,
        .supress_fala_capture=true
    },
    {   .command="ds18b20", 
        .func=&onewire_test_ds18b20_conversion, 
        .description_text=T_HELP_1WIRE_DS18B20,
        .supress_fala_capture=true
    },  
};
const uint32_t hw1wire_commands_count = count_of(hw1wire_commands);

static const char pin_labels[][5] = { "OWD" };

uint32_t hw1wire_setup(void) {
    return 1;
}

uint32_t hw1wire_setup_exc(void) {
    system_bio_update_purpose_and_label(true, M_OW_OWD, BP_PIN_MODE, pin_labels[0]);
#ifdef BP_OLD_HW1WIRE
    onewire_init(bio2bufiopin[M_OW_OWD], bio2bufdirpin[M_OW_OWD]);
#else
    ow_init(8, bio2bufdirpin[M_OW_OWD], bio2bufiopin[M_OW_OWD]);
#endif
    return 1;
}

bool hw1wire_preflight_sanity_check(void) {
    //return ui_help_sanity_check(true, 1<<M_OW_OWD);
    return true;
}

void hw1wire_start(struct _bytecode* result, struct _bytecode* next) {
    result->data_message = GET_T(T_HW1WIRE_RESET);

    ui_help_sanity_check(true, 1<<M_OW_OWD);

#ifdef BP_OLD_HW1WIRE
    uint8_t device_detect = onewire_reset();
#else
    uint8_t device_detect = ow_reset();
#endif

    if (device_detect) {
        // result->error_message = GET_T(T_HW1WIRE_PRESENCE_DETECT);
    } else {
        result->error_message = GET_T(T_HW1WIRE_NO_DEVICE);
        result->error = SERR_ERROR;
    }
}

void hw1wire_write(struct _bytecode* result, struct _bytecode* next) {
#ifdef BP_OLD_HW1WIRE
    onewire_tx_byte(result->out_data);
    onewire_wait_for_idle();
#else
    ow_send(result->out_data);
#endif
}

void hw1wire_read(struct _bytecode* result, struct _bytecode* next) {
#ifdef BP_OLD_HW1WIRE
    result->in_data = onewire_rx_byte();
    onewire_wait_for_idle(); //temp test
#else
    result->in_data = ow_read();
#endif
}

void hw1wire_cleanup(void) {
#ifdef BP_OLD_HW1WIRE
    onewire_cleanup();
#else
    ow_cleanup();
#endif
    bio_init();
    system_bio_update_purpose_and_label(false, M_OW_OWD, BP_PIN_MODE, 0);
}

// MACROS
void hw1wire_macro(uint32_t macro) {
    switch (macro) {
        case 0:
            printf(" 0. Macro list\r\n");
            break;
        default:
            printf("%s\r\n", GET_T(T_MODE_ERROR_MACRO_NOT_DEFINED));
            system_config.error = 1;
    }
}

void hw1wire_help(void) {
    ui_help_mode_commands(hw1wire_commands, hw1wire_commands_count);
}

uint32_t hw1wire_get_speed(void) {
    return 220000;
}


//-----------------------------------------
//
// Flatbuffer/binary access functions
//-----------------------------------------

bool bpio_1wire_configure(bpio_mode_configuration_t *bpio_mode_config){
    return true;  
}

uint32_t bpio_1wire_transaction(struct bpio_data_request_t *request) {
    if(request->debug) printf("[1WIRE] Performing transaction\r\n");

    if(request->start_main||request->start_alt) {
        if(request->debug) printf("[1WIRE] RESET\r\n");
        uint8_t device_detect = onewire_reset();
        if(!device_detect) {
            if(request->debug) printf("[1WIRE] No device detected\r\n");
            return true; // no device detected, but we can continue
        } else {
            if(request->debug) printf("[1WIRE] Device detected\r\n");
        }
    }

    if(request->bytes_write > 0) {
        if(request->debug) printf("[1WIRE] Writing %d bytes\r\n", request->bytes_write);
        for(uint32_t i = 0; i < request->bytes_write; i++) {
            onewire_tx_byte((uint8_t)request->data_buf[i]);
        }
        onewire_wait_for_idle(); // wait for the bus to be idle after writing
    }

    if(request->bytes_read > 0) {
        if(request->debug) printf("[1WIRE] Reading %d bytes\r\n", request->bytes_read);
        // read data
        uint8_t *data_buf = (uint8_t *)request->data_buf;
        for(uint32_t i = 0; i < request->bytes_read; i++) {
            data_buf[i] = onewire_rx_byte();
            onewire_wait_for_idle(); //temp test
        } 
    }

    return false;
}