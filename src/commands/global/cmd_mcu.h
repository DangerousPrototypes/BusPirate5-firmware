/**
 * @file cmd_mcu.h
 * @brief MCU control commands interface.
 * @details Provides commands for MCU reset and bootloader entry.
 */

#include <stdio.h>
#include "pico/stdlib.h"
#include "pirate.h"
#include "pirate/mcu.h"
#include "system_config.h"

/**
 * @brief Reset MCU.
 */
void cmd_mcu_reset(void);

/**
 * @brief Handler for MCU reboot command.
 * @param res  Command result structure
 */
void cmd_mcu_reboot_handler(struct command_result* res);

/**
 * @brief Jump to USB bootloader.
 */
void cmd_mcu_jump_to_bootloader(void);

/**
 * @brief Handler for bootloader command.
 * @param res  Command result structure
 */
void cmd_mcu_jump_to_bootloader_handler(struct command_result* res);