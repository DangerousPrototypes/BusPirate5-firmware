#include <stdio.h>
#include "pico/stdlib.h"
#include <stdint.h>
#include "pirate.h"
#include "ui/ui_term.h"
#include "system_config.h"
#include "command_struct.h"
#include "bytecode.h"
#include "ui/ui_help.h"
#include "ui/ui_cmdln.h"
#include "lib/jep106/jep106.h"

static const char* const usage[] = {
    "jep106 <bank> <device id>",
    "Lookup JEP106 ID (Micron):%s jep106 0x00 0x2c",
    "Lookup JEP106 ID (Sinker):%s jep106 0x0a 0xab"
};

static const struct ui_help_options options[] = {
    { 1, "", T_HELP_GLOBAL_JEP106_LOOKUP }, 
    { 0, "-h", T_HELP_HELP }               // help flag
};


void jep106_handler(struct command_result* res) {
    if (ui_help_show(res->help_flag, usage, count_of(usage), &options[0], count_of(options))) {
        return;
    }

    uint32_t bank;
    if(!cmdln_args_uint32_by_position(1, &bank)){
        printf("Missing bank argument\r\n");
        return;
    }
    
    uint32_t id;
    if(!cmdln_args_uint32_by_position(2, &id)){
        printf("Missing device id argument\r\n");
        return;
    }

    //error if bank = 0 and id bit 7 is set
    if((bank == 0) && (id & 0x80)){
        printf("Invalid JEP106 ID: bank 0 cannot have bit 7 set in device id\r\n");
        return;
    }
    
    //error if bank > 0 and id bit 7 is clear
    if((bank > 0) && !(id & 0x80)){
        printf("Invalid JEP106 ID: bank > 0 must have bit 7 set in device id\r\n");
        return;
    }

    bank=(bank & 0xf); //only lower 4 bits are valid for bank
    id=(id & 0x7f); //SPD JEDEC ID is lower 7 bits, no parity bit. Bit 7 = 1 = bank > 0
    const char* manufacturer = jep106_table_manufacturer((uint8_t)bank, (uint8_t)id); //returns a string
    printf("JEP106 Bank: 0x%02X, ID: 0x%02X => Manufacturer: %s\r\n", (uint8_t)bank, (uint8_t)id, manufacturer);
}    