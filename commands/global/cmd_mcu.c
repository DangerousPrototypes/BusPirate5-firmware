#include <stdio.h>
#include "pico/stdlib.h"
#include "pirate.h"
#include "pirate/mcu.h"
#include "system_config.h"
#include "opt_args.h"

void cmd_mcu_reset(void){
 	mcu_reset();
}

void cmd_mcu_reset_handler(struct command_result *res){
 	cmd_mcu_reset();
}

void cmd_mcu_jump_to_bootloader(void){
    mcu_jump_to_bootloader();
}

void cmd_mcu_jump_to_bootloader_handler(struct command_result *res){

    printf("Jump to bootloader\r\n");
    printf("Firmware download:\r\nhttps://forum.buspirate.com/t/bus-pirate-5-auto-build-main-branch/20/999999\r\n");
    printf("Hardware revision: %d\r\n", system_config.hardware_revision);
    printf("Firmware file: bus_pirate5_rev%d.uf2\r\n", system_config.hardware_revision);
    printf("A USB disk named \"RPI-RP2\" will appear\r\nDrag a firmware file to the disk to upgrade\r\n");

    //check help
	if(res->help_flag){
		return;
	}
    printf("Later Alligator!");
    busy_wait_ms(200);
    cmd_mcu_jump_to_bootloader();
}