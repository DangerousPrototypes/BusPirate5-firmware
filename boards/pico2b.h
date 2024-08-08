/*
 * Copyright (c) 2020 Raspberry Pi (Trading) Ltd.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

// -----------------------------------------------------
// NOTE: THIS HEADER IS ALSO INCLUDED BY ASSEMBLER SO
//       SHOULD ONLY CONSIST OF PREPROCESSOR DIRECTIVES
// -----------------------------------------------------

// This header may be included by other board headers as "boards/pico.h"

// pico_cmake_set PICO_PLATFORM=rp2350

#ifndef _BOARDS_BP6_2350B_H
#define _BOARDS_BP6_2350B_H

// For board detection
#define BUSPIRATE6_2350B_REV1

// --- RP2350 VARIANT ---
#define PICO_RP2350B 1

#ifndef PICO_RP2040_B0_SUPPORTED
#define PICO_RP2040_B0_SUPPORTED 0
#endif

#ifndef PICO_RP2350_A2_SUPPORTED
#define PICO_RP2350_A2_SUPPORTED 1
#endif

// --- FLASH ---

#define PICO_BOOT_STAGE2_CHOOSE_W25Q128 1

#ifndef PICO_FLASH_SPI_CLKDIV
#define PICO_FLASH_SPI_CLKDIV 2
#endif

#ifndef PICO_FLASH_SIZE_BYTES
#define PICO_FLASH_SIZE_BYTES (128 * 1024 * 1024)
#endif

#endif
