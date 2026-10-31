/**
 * @file ui_cmd_menu.h
 * @brief Interactive VT100 wizard for command actions, device selection, files.
 * @details When a command is invoked with no arguments, this module presents
 *          a full interactive wizard using VT100 dropdown menus. The user
 *          navigates with arrow keys and Enter to select actions, devices,
 *          files, and options — matching all CLI features in a GUI.
 *
 * Typical wizard flow:
 *   1. ui_cmd_menu_open("eeprom")      — enter alt-screen
 *   2. ui_cmd_menu_pick_action(...)     — pick dump/erase/write/read/...
 *   3. ui_cmd_menu_pick_device(...)     — pick 24X02/25X512/DS2431/...
 *   4. ui_cmd_menu_pick_file(...)       — browse files on storage
 *   5. ui_cmd_menu_confirm(...)         — yes/no for destructive actions
 *   6. ui_cmd_menu_close()             — leave alt-screen
 *
 * Copyright (c) 2026 Bus Pirate project. MIT License.
 */

#ifndef UI_CMD_MENU_H
#define UI_CMD_MENU_H

#include <stdint.h>
#include <stdbool.h>

/* Forward declarations */
struct bp_command_def;

/* Maximum items in any single dropdown menu */
#define CMD_MENU_MAX_ITEMS 32

/* ── Session lifecycle ──────────────────────────────────────────────── */

/**
 * @brief Open the interactive menu session (enters alt-screen).
 * Must be called before any pick/status functions.
 * @param title  Title shown on the header bar (command name)
 */
void ui_cmd_menu_open(const char* title);

/**
 * @brief Close the interactive menu session (leaves alt-screen).
 * Call after the wizard is complete or cancelled.
 */
void ui_cmd_menu_close(void);

/**
 * @brief Display a summary line showing a completed selection.
 * Shows "label: value" on the screen at the next available row.
 * Call after each successful pick to build up a visible summary.
 * @param label  Left-hand label (e.g. "Action", "Device", "File")
 * @param value  Right-hand value (e.g. "write", "24X02", "data.bin")
 */
void ui_cmd_menu_status(const char* label, const char* value);

/* ── Pickers ────────────────────────────────────────────────────────── */

/**
 * @brief Pick an action from a command definition's action list.
 * @param def     Command definition (.actions, .action_count)
 * @param action  Output: selected action enum value
 * @return true if user selected, false if cancelled
 */
bool ui_cmd_menu_pick_action(const struct bp_command_def* def, uint32_t* action);

/**
 * @brief Pick a device from a name list.
 * Handles lists larger than the screen by paginating.
 * @param names     Array of device name strings (from eeprom_device_t.name etc.)
 * @param count     Number of entries
 * @param selected  Output: index of the selected device
 * @return true if user selected, false if cancelled
 */
bool ui_cmd_menu_pick_device(const char* const* names, uint8_t count, uint8_t* selected);

/**
 * @brief Pick a file from internal storage.
 * Lists all files (optionally filtered by extension) and lets
 * the user select one with arrow keys + Enter.
 * @param ext       File extension filter (e.g. "bin"), or NULL for all files
 * @param file_buf  Output buffer for the selected filename
 * @param buf_size  Size of file_buf
 * @return true if user selected a file, false if cancelled or no files found
 */
bool ui_cmd_menu_pick_file(const char* ext, char* file_buf, uint8_t buf_size);

/**
 * @brief Yes/No confirmation dialog.
 * @param message  Question text (e.g. "Erase chip?")
 * @return true if user confirmed (Enter/Y), false if cancelled (Esc/N)
 */
bool ui_cmd_menu_confirm(const char* message);

/**
 * @brief Prompt user for a numeric value (hex or decimal).
 * Displays an inline text input field on the wizard screen.
 * Input accepts 0x prefix for hex, otherwise decimal. Enter submits, Esc cancels.
 * @param prompt   Prompt text (e.g. "I2C address (7-bit)")
 * @param def_val  Default value shown in prompt
 * @param result   Output: parsed numeric value
 * @return true if user entered a value, false if cancelled
 */
bool ui_cmd_menu_pick_number(const char* prompt, uint32_t def_val, uint32_t* result);

/**
 * @brief Pick from a simple list of string labels.
 * Generic picker for arbitrary string arrays (not tied to command actions or devices).
 * @param menu_label  Label for the dropdown menu bar
 * @param items_str   Array of string labels
 * @param count       Number of entries
 * @param selected    Output: index of the selected item
 * @return true if user selected, false if cancelled
 */
bool ui_cmd_menu_pick_list(const char* menu_label,
                           const char* const* items_str,
                           uint8_t count,
                           uint8_t* selected);

#endif /* UI_CMD_MENU_H */
