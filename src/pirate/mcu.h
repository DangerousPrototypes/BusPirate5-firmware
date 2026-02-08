/**
 * @file mcu.h
 * @brief MCU utility functions.
 * @details Provides MCU-specific operations like reset, bootloader, unique ID.
 */

/**
 * @brief Get MCU unique 64-bit identifier.
 * @return  Unique ID
 */
uint64_t mcu_get_unique_id(void);

/**
 * @brief Reset MCU.
 */
void mcu_reset(void);

/**
 * @brief Jump to USB bootloader mode.
 */
void mcu_jump_to_bootloader(void);

/**
 * @brief Detect RP2040/RP2350 chip revision.
 * @return  Revision number
 */
uint8_t mcu_detect_revision(void);