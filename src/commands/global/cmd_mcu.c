#include <stdio.h>
#include "pico/stdlib.h"
#include "pirate.h"
#include "pirate/mcu.h"
#include "system_config.h"
#include "command_struct.h"
#include "msc_disk.h"
#include "ui/ui_statusbar.h"
#include "lib/bp_args/bp_cmd.h"

static const char* const reboot_usage[] = {
    "reboot",
    "Reboot the Bus Pirate:%s reboot",
};

static const char* const bootloader_usage[] = {
    "$",
    "Jump to bootloader for firmware updates:%s $",
};

const bp_command_def_t reboot_def = {
    .name         = "reboot",
    .description  = T_CMDLN_REBOOT,
    .actions      = NULL,
    .action_count = 0,
    .opts         = NULL,
    .usage        = reboot_usage,
    .usage_count  = count_of(reboot_usage),
};

const bp_command_def_t bootloader_def = {
    .name         = "$",
    .description  = T_CMDLN_BOOTLOAD,
    .actions      = NULL,
    .action_count = 0,
    .opts         = NULL,
    .usage        = bootloader_usage,
    .usage_count  = count_of(bootloader_usage),
};

void cmd_mcu_reset(void) {
    mcu_reset();
}

void cmd_mcu_reboot_handler(struct command_result* res) {
    if (bp_cmd_help_check(&reboot_def, res->help_flag)) {
        return;
    }
    ui_statusbar_deinit();
    busy_wait_ms(100);
    cmd_mcu_reset();
}

void cmd_mcu_jump_to_bootloader(void) {
    mcu_jump_to_bootloader();
}

void cmd_mcu_jump_to_bootloader_handler(struct command_result* res) {
    if (bp_cmd_help_check(&bootloader_def, res->help_flag)) {
        return;
    }

    printf("Jump to bootloader for firmware upgrades\r\n\r\n%s\r\n", BP_HARDWARE_VERSION);
    printf("Firmware download:\r\nhttps://forum.buspirate.com/t/bus-pirate-5-auto-build-main-branch/20/999999\r\n");
    #if BP_VER == 5
        printf("Hardware revision: %d\r\n", BP_REV);
        printf("Firmware file: bus_pirate5_rev%d.uf2\r\n", BP_REV);
        printf("A USB disk named \"RPI-RP2\" will appear\r\n");
    #elif BP_VER == XL5
        printf("Firmware file: bus_pirate5xl.uf2\r\n");
        printf("A USB disk named \"RP2350\" will appear\r\n");
    #elif BP_VER == 6
        printf("Firmware file: bus_pirate6.uf2\r\n");
        printf("A USB disk named \"RP2350\" will appear\r\n");
    #elif BP_VER == 7
        printf("Firmware file: bus_pirate7.uf2\r\n");
        printf("A USB disk named \"BP_BOOT\" will appear\r\n");
    #else
        #error "Unknown Bus Pirate version in cmd_mcu.c"
    #endif
    printf("Drag a firmware file to the disk to upgrade\r\n");

    // check help
    if (res->help_flag) {
        return;
    }
    printf("See you on the other side!");
    ui_statusbar_deinit();
    eject_usbmsdrive();
    busy_wait_ms(200);
    cmd_mcu_jump_to_bootloader();
}