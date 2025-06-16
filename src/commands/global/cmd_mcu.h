#include <stdio.h>
#include "pico/stdlib.h"
#include "pirate.h"
#include "pirate/mcu.h"
#include "system_config.h"

void cmd_mcu_reset(void);
void cmd_mcu_reboot_handler(struct command_result* res);
void cmd_mcu_jump_to_bootloader(void);
void cmd_mcu_jump_to_bootloader_handler(struct command_result* res);