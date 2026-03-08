/**
 * @file eeprom_i2c_gui.h
 * @brief Fullscreen interactive GUI for I2C EEPROM operations.
 *
 * Provides a menu-bar-driven fullscreen interface for configuring and
 * executing I2C EEPROM operations (dump, erase, write, read, verify, test).
 * Launched when the user types "eeprom" with no arguments in I2C mode.
 *
 * Copyright (c) 2026 Bus Pirate project. MIT License.
 */

#ifndef EEPROM_I2C_GUI_H
#define EEPROM_I2C_GUI_H

#include <stdbool.h>
#include <stdint.h>
#include "ui/ui_mem_gui.h"

/* forward declarations - avoid pulling in full eeprom_base.h */
struct eeprom_device_t;
struct eeprom_info;

/**
 * @brief Launch the fullscreen I2C EEPROM GUI.
 *
 * Takes over the terminal (alt-screen), presents a menu-bar interface
 * for selecting action, device, file, and options, then executes the
 * configured operation.  Returns when the user quits (Ctrl-Q / Esc).
 *
 * @param devices      Array of supported I2C EEPROM device definitions.
 * @param device_count Number of entries in the devices array.
 * @param args         EEPROM info struct to populate with user selections.
 *                     On return, contains the fully configured operation
 *                     (or is left partially filled if the user quit early).
 * @return true if user quit without executing, false if an operation was run.
 */
bool eeprom_i2c_gui(const struct eeprom_device_t* devices,
                     uint8_t device_count,
                     struct eeprom_info* args);

/**
 * @brief Read-only I2C EEPROM size detection.
 *
 * Uses address mirroring, I2C ACK scanning, and sequential wrap tests
 * to identify the chip without any writes.  Covers all 14 chips in the
 * standard 24X device table.
 *
 * Before probing, performs a full bus scan (0x08-0x77) and warns via
 * ops->warning() if devices outside the EEPROM's own address range
 * (base..base+8) are found, since those may cause false ACK hits.
 * Pass ops=NULL to suppress the bus scan warning.
 *
 * @param i2c_addr  7-bit I2C base address (typically 0x50)
 * @param devices   device table to search
 * @param count     number of entries in device table
 * @param ops       UI ops for warning output (NULL = silent)
 * @return          index into devices[] on success, or:
 *                   -1  uniform data at address 0, cannot detect read-only
 *                   -2  no match found / ambiguous ACK pattern
 *                   -3  I2C read error
 */
int eeprom_i2c_detect_size(uint8_t i2c_addr,
                            const struct eeprom_device_t *devices,
                            uint8_t count,
                            const ui_mem_gui_ops_t *ops);

#endif /* EEPROM_I2C_GUI_H */
