/*
 * This file is part of the Bus Pirate project (http://code.google.com/p/the-bus-pirate/).
 *
 * Written and maintained by the Bus Pirate project.
 *
 * To the extent possible under law, the project has
 * waived all copyright and related or neighboring rights to Bus Pirate. This
 * work is published from United States.
 *
 * For details see: http://creativecommons.org/publicdomain/zero/1.0/.
 *
 *    This program is distributed in the hope that it will be useful,
 *    but WITHOUT ANY WARRANTY; without even the implied warranty of
 *    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 */

/**
 * @file pic24_icsp.h
 * @brief PIC24FJ 2-wire ICSP (Microchip DS39768) on three buffered IO pins.
 * @details Plain ICSP: SIX runs one instruction on the PIC, REGOUT reads its VISI
 *          register. No programming executive (Enhanced ICSP), and executive memory,
 *          with the FRC calibration words, is never erased or written.
 *          Addresses are PC addresses (two per instruction), words are 24 bits.
 */

#ifndef PIC24_ICSP_H
#define PIC24_ICSP_H

#include <stdint.h>

#define PIC24_ICSP_ROW_WORDS 64u        ///< Instructions per row write (NVMCON 0x4001)
#define PIC24_ICSP_ERASED 0xFFFFFFu     ///< An erased instruction word
#define PIC24_ICSP_DEVID_ADDR 0xFF0000u ///< DEVID, then DEVREV at +2

/**
 * @brief Result of an erase or a row write.
 */
typedef enum {
    PIC24_ICSP_OK = 0,      ///< WR cleared without WRERR
    PIC24_ICSP_ERR_TIMEOUT, ///< WR did not clear in time
    PIC24_ICSP_ERR_WRERR,   ///< The PIC ended the operation with WRERR set
} pic24_icsp_status_t;

/**
 * @brief Select the pins and the clock speed. Drives nothing.
 * @param pgc       BIO pin wired to PGC
 * @param pgd       BIO pin wired to PGD
 * @param mclr      BIO pin wired to MCLR
 * @param phase_ns  Duration of each PGC phase (high and low), DS39768 asks for 40 ns at least
 */
void pic24_icsp_init(uint8_t pgc, uint8_t pgd, uint8_t mclr, uint32_t phase_ns);

/**
 * @brief Change the PGC phase duration, as for pic24_icsp_init().
 */
void pic24_icsp_set_phase_ns(uint32_t phase_ns);

/**
 * @brief Enter ICSP: MCLR pulse, the 32-bit key, MCLR high, then NOP, GOTO 0x200, NOP.
 * @details Drives MCLR, PGC and PGD. The caller must know that the target is powered
 *          (MCLR pulled up), or it may be powered through these pins.
 */
void pic24_icsp_enter(void);

/**
 * @brief Leave ICSP: PGC and PGD low, then MCLR low. The PIC stays in reset and the
 *        three pins stay driven: release them with bio_input() when appropriate.
 */
void pic24_icsp_exit(void);

/**
 * @brief SIX: run one 24-bit instruction on the PIC.
 */
void pic24_icsp_six(uint32_t instruction);

/**
 * @brief REGOUT: read the 16-bit VISI register.
 */
uint16_t pic24_icsp_regout(void);

/**
 * @brief Write a pattern to VISI and read it back. A PIC in ICSP mode echoes it.
 */
uint16_t pic24_icsp_visi_echo(uint16_t pattern);

/**
 * @brief Read instruction words with table reads (two words per three REGOUT).
 * @param address  Even PC address of the first word (TBLPAG is address bits 23:16)
 * @param words    Receives count 24-bit words
 * @param count    Number of words, even (an odd last word is not read)
 */
void pic24_icsp_read(uint32_t address, uint32_t* words, uint32_t count);

/**
 * @brief Chip erase of user memory only (NVMCON 0x404F with TBLPAG 0).
 * @param timeout_ms  How long to wait for WR to clear
 */
pic24_icsp_status_t pic24_icsp_erase_user(uint32_t timeout_ms);

/**
 * @brief Program one erased row of 64 instructions.
 * @param address     PC address of the row (a multiple of 0x80)
 * @param words       64 instruction words; 0xFFFFFF leaves a word erased
 * @param timeout_ms  How long to wait for WR to clear
 */
pic24_icsp_status_t pic24_icsp_write_row(uint32_t address, const uint32_t* words, uint32_t timeout_ms);

#endif
