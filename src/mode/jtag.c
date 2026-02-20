#include <stdbool.h>
#include <stdio.h>
#include "pico/stdlib.h"
#include <stdint.h>
#include "hardware/spi.h"
#include "pirate.h"
#include "system_config.h"
#include "command_struct.h"
#include "bytecode.h"
#include "pirate/bio.h"
#include "ui/ui_prompt.h"
#include "ui/ui_term.h"
#include "pirate/storage.h"
#include "ui/ui_help.h"
#include "pirate/hwspi.h"
#include "usb_rx.h"
#include "commands/jtag/bluetag.h"

// command configuration
const struct _mode_command_struct jtag_commands[] = {
    {   .func=&bluetag_handler,
        .def=&bluetag_def,
        .supress_fala_capture=true
    },
 
};
const uint32_t jtag_commands_count = count_of(jtag_commands);

static const char pin_labels[][5] = { "TRST", "SRST", "TCK", "TDI", "TDO", "TMS" };

//static struct _spi_mode_config mode_config;

uint32_t jtag_setup(void) {
 
    return 1;
}

uint32_t jtag_setup_exc(void) {
    system_bio_update_purpose_and_label(true, BIO2, BP_PIN_MODE, pin_labels[0]);
    system_bio_update_purpose_and_label(true, BIO3, BP_PIN_MODE, pin_labels[1]);
    system_bio_update_purpose_and_label(true, M_SPI_CLK, BP_PIN_MODE, pin_labels[2]);
    system_bio_update_purpose_and_label(true, M_SPI_CDO, BP_PIN_MODE, pin_labels[3]);
    system_bio_update_purpose_and_label(true, M_SPI_CDI, BP_PIN_MODE, pin_labels[4]);
    system_bio_update_purpose_and_label(true, M_SPI_CS, BP_PIN_MODE, pin_labels[5]);
    return 10000000;
}

void jtag_cleanup(void) {
    // release pin claims
    system_bio_update_purpose_and_label(false, BIO2, BP_PIN_MODE, 0);
    system_bio_update_purpose_and_label(false, BIO3, BP_PIN_MODE, 0);
    system_bio_update_purpose_and_label(false, M_SPI_CLK, BP_PIN_MODE, 0);
    system_bio_update_purpose_and_label(false, M_SPI_CDO, BP_PIN_MODE, 0);
    system_bio_update_purpose_and_label(false, M_SPI_CDI, BP_PIN_MODE, 0);
    system_bio_update_purpose_and_label(false, M_SPI_CS, BP_PIN_MODE, 0);
    bio_init();
}

bool jtag_preflight_sanity_check(void){
    return ui_help_sanity_check(true, 0x00);
}

void jtag_pins(void){
    printf("TRST\tSRST\tCS\tMISO\tCLK\tMOSI");
}


void jtag_settings(void) {
}


void jtag_help(void) {
    printf("JTAG mode for utilities such as blueTag\r\n");
    ui_help_mode_commands(jtag_commands, jtag_commands_count);
}

uint32_t jtag_get_speed(void) {
    return 10000000;
}