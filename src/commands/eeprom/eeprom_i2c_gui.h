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

/* Forward declarations — avoid pulling in full eeprom_base.h */
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

#endif /* EEPROM_I2C_GUI_H */
