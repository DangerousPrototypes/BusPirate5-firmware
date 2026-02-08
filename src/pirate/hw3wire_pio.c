/**
 * @file hw3wire_pio.c
 * @brief 3-Wire (SPI) protocol PIO implementation.
 * @details PIO-based SPI master with configurable clock frequency and phase.
 *          Provides full-duplex SPI communication with manual clock control
 *          for debugging and custom protocols.
 *          
 *          Features:
 *          - Configurable frequency
 *          - Full-duplex transfer
 *          - Manual clock ticking
 *          - Direct pin manipulation
 * @copyright Copyright (c) 2021 Raspberry Pi (Trading) Ltd. (BSD-3-Clause)
 */

/**
 * Copyright (c) 2021 Raspberry Pi (Trading) Ltd.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */
#include <stdint.h>
#include "pico/stdlib.h"
#include "hw3wire_pio.h"
#include "hw3wire.pio.h"
#include "hardware/structs/iobank0.h"
#include "hardware/regs/io_bank0.h"
#include "pirate.h"
#include "pio_config.h"
#include "pirate/bio.h"

#define PIO_PIN_0 1u << 0
#define PIO_SIDE_0 1u << 11

const int PIO_HW3WIRE_ICOUNT_LSB = 10;

static struct _pio_config pio_config;

void pio_hw3wire_init(uint mosi, uint sclk, uint miso, uint32_t freq) {
    // bool success = pio_claim_free_sm_and_add_program_for_gpio_range(&hw3wire_program, &pio_config.pio,
    // &pio_config.sm, &pio_config.offset, RGB_CDO, 1, true); hard_assert(success);
    pio_config.pio = PIO_MODE_PIO;
    pio_config.sm = 0;
    pio_config.program = &hw3wire_program;
    pio_config.offset = pio_add_program(pio_config.pio, pio_config.program);
#ifdef BP_PIO_SHOW_ASSIGNMENT
    printf("PIO: pio=%d, sm=%d, offset=%d\r\n", PIO_NUM(pio_config.pio), pio_config.sm, pio_config.offset);
#endif
    hw3wire_program_init(pio_config.pio, pio_config.sm, pio_config.offset, mosi, sclk, miso, freq);
}

void pio_hw3wire_cleanup(void) {
    // pio_remove_program_and_unclaim_sm(&hw3wire_program, pio_config.pio, pio_config.sm, pio_config.offset);
    pio_remove_program(pio_config.pio, pio_config.program, pio_config.offset);
}

static inline void pio_hw3wire_wait_idle(PIO pio, uint sm) {
    pio->fdebug = 1u << (PIO_FDEBUG_TXSTALL_LSB + sm);
    while (!(pio->fdebug & 1u << (PIO_FDEBUG_TXSTALL_LSB + sm)))
        ;
}

uint8_t pio_hw3wire_get(PIO pio, uint sm) {
    return (uint8_t)pio_sm_get(pio, sm);
}

void pio_hw3wire_put(PIO pio, uint sm, uint16_t data) {
    while (pio_sm_is_tx_fifo_full(pio, sm))
        ;
        // some versions of GCC dislike this
#ifdef __GNUC__
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wstrict-aliasing"
#endif
    *(io_rw_16*)&pio->txf[sm] = data;
#ifdef __GNUC__
#pragma GCC diagnostic pop
#endif
    pio_hw3wire_wait_idle(pio, sm); // blocking function
}

// put data and block until idle
void pio_hw3wire_put16(uint16_t data) {
    pio_hw3wire_put(pio_config.pio, pio_config.sm, data & 0xff);
    pio_hw3wire_wait_idle(pio_config.pio, pio_config.sm); // blocking function
}

// put 0xff, wait for read to complete
void pio_hw3wire_get16(uint8_t* data) {
    while (!pio_sm_is_rx_fifo_empty(pio_config.pio, pio_config.sm)) {
        (void)pio_hw3wire_get(pio_config.pio, pio_config.sm);
    }
    uint16_t temp = (*data);
    pio_hw3wire_put(pio_config.pio, pio_config.sm, temp);
    while (pio_sm_is_rx_fifo_empty(pio_config.pio, pio_config.sm));
    (*data) = pio_hw3wire_get(pio_config.pio, pio_config.sm);
}

static inline void pio_hw3wire_put_instructions(const uint16_t* inst, uint8_t length) {
    for (uint8_t i = 0; i < length; i++) {
        pio_hw3wire_put(pio_config.pio, pio_config.sm, inst[i]);
    }
}

void pio_hw3wire_set_mask(uint32_t pin_mask, uint32_t pin_value) {
    // build instruction on the fly...
    // get the current PIO output state from OUTTOPAD register bit
    // set the same state in instruction
    // set the bit pattern for the pin we're changing
    // execute without changing the existing state of other pins
    // pin 0 is bit 11, side 0 is bit 0
    /*static const uint16_t set_scl_sda_program_instructions[] = {
            //     .wrap_target
    0xf700, //  0: set    pins, 0         side 0 [7] 0b1111011100000000
    0xf701, //  1: set    pins, 1         side 0 [7] 0b1111011100000001
    0xff00, //  2: set    pins, 0         side 1 [7] 0b1111111100000000
    0xff01, //  3: set    pins, 1         side 1 [7] 0b1111111100000001
    */
    uint32_t instruction = 0xf700;
    if (pin_mask & 1u << 0) {
        if (pin_value & 1u << 0) {
            instruction |= PIO_PIN_0;
        }
    } else {
        // retain the current state of the pin
        if ((iobank0_hw->io[15].status & (1u << IO_BANK0_GPIO15_STATUS_OUTTOPAD_MSB))) {
            instruction |= PIO_PIN_0;
        }
    }

    if (pin_mask & 1u << 1) {
        if (pin_value & 1u << 1) {
            instruction |= PIO_SIDE_0;
        }
    } else {
        // retain the current state of the pin
        if ((iobank0_hw->io[14].status & (1u << IO_BANK0_GPIO14_STATUS_OUTTOPAD_MSB))) {
            instruction |= PIO_SIDE_0;
        }
    }
    // this is a bit of a hack: minimum instructions is 2, so we need to pad the instruction with a no-op
    uint16_t set[] = { 1u << PIO_HW3WIRE_ICOUNT_LSB, // Escape code for 3 instruction sequence
                       0xa042,                       // NOP padding
                       instruction };
    pio_hw3wire_put_instructions(set, count_of(set));
}

void pio_hw3wire_clock_tick(void) {
    uint32_t instruction = 0xf700;
    if ((iobank0_hw->io[15].status & (1u << IO_BANK0_GPIO15_STATUS_OUTTOPAD_MSB))) {
        instruction |= PIO_PIN_0;
    }
    uint16_t tick_clock[] = {
        2u << PIO_HW3WIRE_ICOUNT_LSB, // Escape code for 3 instruction sequence
        instruction,                  // base state, clock low
        instruction | PIO_SIDE_0,     // Release clock
        instruction                   // lower clock
    };
    pio_hw3wire_put_instructions(tick_clock, count_of(tick_clock));
}
