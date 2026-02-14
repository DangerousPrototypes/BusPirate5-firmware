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

static const struct ui_help_options options[] = {
    { 1, "", T_HELP_FLASH },               // flash command help
    //{ 0, "init", T_HELP_FLASH_INIT },      // init
    { 0, "probe", T_HELP_FLASH_PROBE },    // probe
    { 0, "dump", T_HELP_EEPROM_DUMP }, // dump
    { 0, "erase", T_HELP_FLASH_ERASE },    // erase
    { 0, "write", T_HELP_FLASH_WRITE },    // write
    { 0, "read", T_HELP_FLASH_READ },      // read
    { 0, "verify", T_HELP_FLASH_VERIFY },  // verify
    { 0, "test", T_HELP_FLASH_TEST },      // test
    { 0, "-f", T_HELP_FLASH_FILE_FLAG },   // file to read/write/verify
    { 0, "-e", T_HELP_FLASH_ERASE_FLAG },  // with erase (before write)
    { 0, "-v", T_HELP_FLASH_VERIFY_FLAG }, // with verify (after write)
    { 0, "-s", UI_HEX_HELP_START }, // start address for dump
    { 0, "-b", UI_HEX_HELP_BYTES }, // bytes to dump
    { 0, "-q", UI_HEX_HELP_QUIET}, // quiet mode, disable address and ASCII columns
    { 0, "-c", T_HELP_DISK_HEX_PAGER_OFF },
    { 0, "-o", T_HELP_FLASH_OVERRIDE }, // override flash chip detection, use with -b to specify bytes to read
    { 0, "-y", T_HELP_FLASH_YES_OVERRIDE }, // override yes/no prompt for destructive actions
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

const struct cmdln_action_t flash_actions[] = {
    { FLASH_PROBE, "probe" },
    { FLASH_DUMP, "dump" },
    { FLASH_ERASE, "erase" },
    { FLASH_WRITE, "write" },
    { FLASH_READ, "read" },
    { FLASH_VERIFY, "verify" },
    { FLASH_TEST, "test" }
};

void flash(struct command_result* res) {
    char file[13];

    if (ui_help_show(res->help_flag, usage, count_of(usage), &options[0], count_of(options))) {
        return;
    }
    if (!ui_help_check_vout_vref()) {
        return;
    }

    uint32_t flash_action = 0;
    // common function to parse the command line verb or action
    if(cmdln_args_get_action(flash_actions, count_of(flash_actions), &flash_action)){
        ui_help_show(true, usage, count_of(usage), &options[0], count_of(options)); // show help if requested
        return;
    }

    // erase_flag
    bool erase_flag = cmdln_args_find_flag('e' | 0x20);
    // verify_flag
    bool verify_flag = cmdln_args_find_flag('v' | 0x20);
    // file to read/write/verify
    command_var_t arg;
    bool file_flag = cmdln_args_find_flag_string('f' | 0x20, &arg, sizeof(file), file);
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
        (!cmdln_args_find_flag('y' | 0x20))){
            printf("This action may modify the SPI flash contents. Do you want to continue?");
            prompt_result confirm_result;
            bool confirm;
            if (!ui_prompt_bool(&confirm_result, false, false, false, &confirm) || !confirm) {
                printf("Aborted by user\r\n");
                return;
            }
    }

    bool override_flag = cmdln_args_find_flag('o' | 0x20);

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
        command_var_t arg;
        if (!cmdln_args_find_flag_uint32('b', &arg, &end_address)) {
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