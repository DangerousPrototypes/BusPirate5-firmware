#include <stdio.h>
#include "pico/stdlib.h"
#include <stdint.h>
#include "pirate.h"
#include "ui/ui_term.h"
#include "system_config.h"
#include "command_struct.h"
#include "bytecode.h"
#include "ui/ui_help.h"
#include "lib/bp_args/bp_cmd.h"
#include "lib/jep106/jep106.h"

static const char* const usage[] = {
    "jep106 <bank number> <vendor id>",
    "Lookup JEP106 ID (Micron):%s jep106 0x00 0x2c",
    "Lookup JEP106 ID (Sinker):%s jep106 0x0a 0xab"
};

static const bp_command_positional_t jep106_positionals[] = {
    { "bank",   "<bank number>", 0, true },
    { "id",     "<vendor id>",   0, true },
    { 0 }
};

const bp_command_def_t jep106_def = {
    .name         = "jep106",
    .description  = T_HELP_GLOBAL_JEP106_LOOKUP,
    .actions      = NULL,
    .action_count = 0,
    .opts         = NULL,
    .positionals      = jep106_positionals,
    .positional_count = 2,
    .usage        = usage,
    .usage_count  = count_of(usage),
};

void jep106_handler(struct command_result* res) {
    if (bp_cmd_help_check(&jep106_def, res->help_flag)) {
        return;
    }

    uint32_t bank;
    if(!bp_cmd_get_positional_uint32(&jep106_def, 1, &bank)){
        printf("Missing bank number argument\r\n\r\n");
        bp_cmd_help_show(&jep106_def);
        return;
    }
    
    uint32_t id;
    if(!bp_cmd_get_positional_uint32(&jep106_def, 2, &id)){
        printf("Missing vendor id argument\r\n\r\n");
        bp_cmd_help_show(&jep106_def);
        return;
    }

    //error if bank = 0 and id bit 7 is set
    if((bank == 0) && (id & 0x80)){
        printf("Invalid JEP106 ID: bank 0 cannot have bit 7 set in vendor id\r\n");
        return;
    }
    
    //error if bank > 0 and id bit 7 is clear
    if((bank > 0) && !(id & 0x80)){
        printf("Invalid JEP106 ID: bank > 0 must have bit 7 set in vendor id\r\n");
        return;
    }

    bank=(bank & 0xf); //only lower 4 bits are valid for bank
    id=(id & 0x7f); //SPD JEDEC ID is lower 7 bits, no parity bit. Bit 7 = 1 = bank > 0
    const char* manufacturer = jep106_table_manufacturer((uint8_t)bank, (uint8_t)id); //returns a string
    printf("JEP106 Bank: 0x%02X, ID: 0x%02X => Manufacturer: %s\r\n", (uint8_t)bank, (uint8_t)id, manufacturer);
}    