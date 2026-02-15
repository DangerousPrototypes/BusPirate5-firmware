#include <stdbool.h>
#include <stdio.h>
#include <string.h>
#include "pico/stdlib.h"
#include "hardware/spi.h"
#include "pirate.h"
#include "command_struct.h"
#include "fatfs/ff.h"
#include "pirate/storage.h"
#include "bytecode.h"
#include "mode/hwspi.h"
#include "lib/sfud/inc/sfud.h"
#include "lib/sfud/inc/sfud_def.h"
#include "spiflash.h"
#include "ui/ui_cmdln.h"
#include "ui/ui_help.h"
#include "ui/ui_prompt.h"
#include "system_config.h"
#include "pirate/amux.h"
#include "binmode/fala.h"
#include "ui/ui_hex.h"
#include "lib/bp_args/bp_cmd.h"

static const char* const usage[] = {
    "flash [probe|dump|erase|write|read|verify|test]\r\n\t[-f <file>] [-e(rase)] [-v(verify)] [-h(elp)]",
    "Initialize and probe:%s flash probe",
    "Show flash contents (x to exit):%s flash dump",
    "Show 16 bytes starting at address 0x60:%s flash dump -s 0x60 -b 16",
    "Erase and program, with verify:%s flash write -f example.bin -e -v",
    "Read to file:%s flash read -f example.bin",
    "Verify with file:%s flash verify -f example.bin",
    "Test chip (full erase/write/verify):%s flash test",
    "Force dump:%s flash read -o -b <bytes> -f <file>"
};

enum flash_actions {
    FLASH_PROBE = 0,
    FLASH_DUMP,
    FLASH_ERASE,
    FLASH_WRITE,
    FLASH_READ,
    FLASH_VERIFY,
    FLASH_TEST
};

static const bp_command_action_t flash_action_defs[] = {
    { FLASH_PROBE,  "probe",  T_HELP_FLASH_PROBE },
    { FLASH_DUMP,   "dump",   T_HELP_EEPROM_DUMP },
    { FLASH_ERASE,  "erase",  T_HELP_FLASH_ERASE },
    { FLASH_WRITE,  "write",  T_HELP_FLASH_WRITE },
    { FLASH_READ,   "read",   T_HELP_FLASH_READ },
    { FLASH_VERIFY, "verify", T_HELP_FLASH_VERIFY },
    { FLASH_TEST,   "test",   T_HELP_FLASH_TEST },
};

static const bp_command_opt_t flash_opts[] = {
    { "file",     'f', BP_ARG_REQUIRED, "<file>",    T_HELP_FLASH_FILE_FLAG },
    { "erase",    'e', BP_ARG_NONE,     NULL,        T_HELP_FLASH_ERASE_FLAG },
    { "verify",   'v', BP_ARG_NONE,     NULL,        T_HELP_FLASH_VERIFY_FLAG },
    { "start",    's', BP_ARG_REQUIRED, "<addr>",    UI_HEX_HELP_START },
    { "bytes",    'b', BP_ARG_REQUIRED, "<count>",   UI_HEX_HELP_BYTES },
    { "quiet",    'q', BP_ARG_NONE,     NULL,        UI_HEX_HELP_QUIET },
    { "nopager",  'c', BP_ARG_NONE,     NULL,        T_HELP_DISK_HEX_PAGER_OFF },
    { "override", 'o', BP_ARG_NONE,     NULL,        T_HELP_FLASH_OVERRIDE },
    { "yes",      'y', BP_ARG_NONE,     NULL,        T_HELP_FLASH_YES_OVERRIDE },
    { 0 }
};

static const bp_command_def_t flash_def = {
    .name         = "flash",
    .description  = T_HELP_FLASH,
    .actions      = flash_action_defs,
    .action_count = count_of(flash_action_defs),
    .opts         = flash_opts,
    .usage        = usage,
    .usage_count  = count_of(usage),
};

void flash(struct command_result* res) {
    char file[13];

    if (bp_cmd_help_check(&flash_def, res->help_flag)) {
        return;
    }
    if (!ui_help_check_vout_vref()) {
        return;
    }

    uint32_t flash_action = 0;
    if (!bp_cmd_get_action(&flash_def, &flash_action)) {
        bp_cmd_help_show(&flash_def);
        return;
    }

    // erase_flag
    bool erase_flag = bp_cmd_find_flag(&flash_def, 'e');
    // verify_flag
    bool verify_flag = bp_cmd_find_flag(&flash_def, 'v');
    // file to read/write/verify
    bool file_flag = bp_cmd_get_string(&flash_def, 'f', file, sizeof(file));
    if((flash_action == FLASH_WRITE || flash_action == FLASH_READ || flash_action == FLASH_VERIFY) && !file_flag) {
        printf("Missing file name (-f)\r\n");
        return;
    }

    // error if read/verify and erase flag is set, doesn't make sense to erase if just reading or verifying
    if((flash_action == FLASH_READ || flash_action == FLASH_VERIFY) && erase_flag) {
        printf("Erase flag (-e) cannot be used with read or verify actions\r\n");
        return;
    }

    // prompt yes/no for destructive action: erase, write, test (override with -y)
    if((flash_action == FLASH_ERASE || flash_action == FLASH_WRITE || flash_action == FLASH_TEST) && 
        (!bp_cmd_find_flag(&flash_def, 'y'))){
            printf("This action may modify the SPI flash contents. Do you want to continue?");
            prompt_result confirm_result;
            bool confirm;
            if (!ui_prompt_bool(&confirm_result, false, false, false, &confirm) || !confirm) {
                printf("Aborted by user\r\n");
                return;
            }
    }

    bool override_flag = bp_cmd_find_flag(&flash_def, 'o');

    // start and end rage? bytes to write/dump???
    sfud_flash flash_info = { .name = "SPI_FLASH", .spi.name = "SPI1" };
    uint8_t data[256];
    uint8_t data2[256];
    uint32_t start_address = 0;
    uint32_t end_address;

    //we manually control any FALA capture
    fala_start_hook();    

    printf("\r\nInitializing SPI flash...\r\n");
    if (spiflash_init(&flash_info) && !override_flag) {
        end_address = flash_info.chip.capacity;
    } else if (flash_action == FLASH_READ && override_flag) {
        if (!bp_cmd_get_uint32(&flash_def, 'b', &end_address)) {
            printf("Specify read length with the -b flag (-b 0x00ffff)\r\n");
            goto flash_cleanup;
        }
        printf("Force read of unknown flash chip\r\n");
        printf("Using command 0x03, reading %d bytes\r\n", end_address - start_address);
        spiflash_force_dump(start_address, end_address, sizeof(data), data, &flash_info, file);
        goto flash_cleanup;
    } else {
        goto flash_cleanup;
    }

    if(flash_action == FLASH_PROBE){
        spiflash_probe(); // always do by default
        goto flash_cleanup; // no need to continue
    }

    if(flash_action == FLASH_DUMP){
        spiflash_show_hex(sizeof(data), data, &flash_info);
        goto flash_cleanup; // no need to continue
    }

    if (flash_action == FLASH_ERASE || (erase_flag && FLASH_WRITE) || flash_action == FLASH_TEST) {
        if (!spiflash_erase(&flash_info)) {
            goto flash_cleanup;
        }
        if (verify_flag || flash_action == FLASH_TEST) {
            if (!spiflash_erase_verify(start_address, end_address, sizeof(data), data, &flash_info)) {
                goto flash_cleanup;
            }
        }
    }

    if (flash_action == FLASH_TEST) {
        if (!spiflash_write_test(start_address, end_address, sizeof(data), data, &flash_info)) {
            goto flash_cleanup;
        }
        if (!spiflash_write_verify(start_address, end_address, sizeof(data), data, &flash_info)) {
            goto flash_cleanup;
        }
    }

    if (flash_action == FLASH_WRITE) {
        if (!spiflash_load(start_address, end_address, sizeof(data), data, &flash_info, file)) {
            goto flash_cleanup;
        }
        if (verify_flag) {
            if (!spiflash_verify(start_address, end_address, sizeof(data), data, data2, &flash_info, file)) {
                goto flash_cleanup;
            }
        }
    }

    if (flash_action == FLASH_READ) {
        if (!spiflash_dump(start_address, end_address, sizeof(data), data, &flash_info, file)) {
            goto flash_cleanup;
        }
    }

    if (flash_action == FLASH_VERIFY) {
        if (!spiflash_verify(start_address, end_address, sizeof(data), data, data2, &flash_info, file)) {
            goto flash_cleanup;
        }
    }

flash_cleanup:
    //we manually control any FALA capture
    fala_stop_hook();
    fala_notify_hook();

}