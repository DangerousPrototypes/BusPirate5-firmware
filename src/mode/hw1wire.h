/**
 * @file hw1wire.h
 * @brief Hardware 1-Wire mode interface.
 * @details Implements 1-Wire (Dallas/Maxim) protocol using PIO-based timing.
 *          Supports ROM search, device enumeration, and device-specific commands
 *          like DS18B20 temperature sensors and 1-Wire EEPROMs.
 */

/**
 * @brief Interactive 1-Wire mode setup.
 * @return 1 on success, 0 on failure
 */
uint32_t hw1wire_setup(void);

/**
 * @brief Execute 1-Wire mode setup.
 * @return 1 on success, 0 on failure
 */
uint32_t hw1wire_setup_exc(void);

/**
 * @brief Cleanup and disable 1-Wire mode.
 */
void hw1wire_cleanup(void);

/**
 * @brief Display 1-Wire pin assignments.
 */
void hw1wire_pins(void);

/**
 * @brief Display current 1-Wire settings.
 */
void hw1wire_settings(void);

/**
 * @brief Issue 1-Wire reset and presence detect.
 * @param result  Pointer to bytecode result structure
 * @param next    Pointer to next bytecode instruction
 */
void hw1wire_start(struct _bytecode* result, struct _bytecode* next);

/**
 * @brief Write data to 1-Wire bus.
 * @param result  Pointer to bytecode result structure
 * @param next    Pointer to next bytecode instruction
 */
void hw1wire_write(struct _bytecode* result, struct _bytecode* next);

/**
 * @brief Read data from 1-Wire bus.
 * @param result  Pointer to bytecode result structure
 * @param next    Pointer to next bytecode instruction
 */
void hw1wire_read(struct _bytecode* result, struct _bytecode* next);

/**
 * @brief Execute 1-Wire macro command.
 * @param macro  Macro number to execute
 */
void hw1wire_macro(uint32_t macro);

/**
 * @brief Display 1-Wire mode help.
 */
void hw1wire_help(void);

/**
 * @brief Get current 1-Wire speed.
 * @return Speed indicator (typically 0 for standard speed)
 */
uint32_t hw1wire_get_speed(void);

/**
 * @brief Perform 1-Wire mode sanity checks.
 * @return true if all checks pass, false otherwise
 */
bool hw1wire_preflight_sanity_check(void);

/**
 * @brief Configure 1-Wire mode for binary protocol.
 * @param bpio_mode_config  Binary protocol configuration
 * @return true on success, false on failure
 */
bool bpio_1wire_configure(bpio_mode_configuration_t *bpio_mode_config);

extern const struct _mode_command_struct hw1wire_commands[];
extern const uint32_t hw1wire_commands_count;
/*/
unsigned char OWReset(void);
unsigned char OWBit(unsigned char c);
unsigned char OWByte(unsigned char OWbyte);
void DS1wireReset(void);
void DS1wireID(unsigned char famID);
unsigned char OWFirst(void);
unsigned char OWNext(void);
unsigned char OWSearch(void);
unsigned char OWVerify(void);
unsigned char docrc8(unsigned char value);

#define OWWriteByte(d) OWByte(d)
#define OWReadByte() OWByte(0xFF)
#define OWReadBit() OWBit(1)
#define OWWriteBit(b) OWBit(b)
*/