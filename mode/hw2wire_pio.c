/**
 * Copyright (c) 2021 Raspberry Pi (Trading) Ltd.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include "mode/hw2wire_pio.h"

const int PIO_HW2WIRE_ICOUNT_LSB = 10;
const int PIO_HW2WIRE_FINAL_LSB  = 9;
const int PIO_HW2WIRE_DATA_LSB   = 1;
const int PIO_HW2WIRE_NAK_LSB    = 0;

bool pio_hw2wire_check_error(PIO pio, uint sm) {
    return pio_interrupt_get(pio, sm);
}

void pio_hw2wire_rx_enable(PIO pio, uint sm, bool en) {
    if (en) hw_set_bits(&pio->sm[sm].shiftctrl, PIO_SM0_SHIFTCTRL_AUTOPUSH_BITS);
    else hw_clear_bits(&pio->sm[sm].shiftctrl, PIO_SM0_SHIFTCTRL_AUTOPUSH_BITS);
}
 
static inline void pio_hw2wire_wait_idle(PIO pio, uint sm) {
    pio->fdebug = 1u << (PIO_FDEBUG_TXSTALL_LSB + sm);
    while(!(pio->fdebug & 1u << (PIO_FDEBUG_TXSTALL_LSB + sm)));  
}

uint8_t pio_hw2wire_get(PIO pio, uint sm) {
    return (uint8_t)pio_sm_get(pio, sm);
}

// put data and block until idle
void pio_hw2wire_put16(PIO pio, uint sm, uint16_t data) {
    while (pio_sm_is_tx_fifo_full(pio, sm));
    // some versions of GCC dislike this
#ifdef __GNUC__
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wstrict-aliasing"
#endif
    *(io_rw_16 *)&pio->txf[sm] = data;
#ifdef __GNUC__
#pragma GCC diagnostic pop
#endif
    pio_hw2wire_wait_idle(pio, sm); //blocking function
}

// put 0xff, wait for read to complete
void pio_hw2wire_get16(PIO pio, uint sm, uint32_t *data) {
	while(!pio_sm_is_rx_fifo_empty(pio, sm)) {
        (void)pio_hw2wire_get(pio, sm);
    }
    pio_hw2wire_put16(pio, sm, (0xffu << 1) | (0?0:(1u << 9) | (1u << 0)));
    while(pio_sm_is_rx_fifo_empty(pio, sm));
	(*data) = pio_hw2wire_get(pio, sm);
}

static inline void pio_hw2wire_put_instructions(PIO pio, uint sm, const uint16_t *inst, uint8_t length) {
    for(uint8_t i=0; i<length; i++){
        pio_hw2wire_put16(pio, sm, inst[i]);
    }
}

void pio_hw2wire_reset(PIO pio, uint sm) {
    const uint16_t start[]= {
        1u << PIO_HW2WIRE_ICOUNT_LSB, // Escape code for 2 instruction sequence
        set_scl_sda_program_instructions[I2C_SC1_SD1],  // reset to high to release
        set_scl_sda_program_instructions[I2C_SC0_SD0]   // pull both low so clock goes high on first tick
    };
    pio_hw2wire_put_instructions(pio, sm, start, count_of(start));    
}

void pio_hw2wire_clock_tick(PIO pio, uint sm) {
    const uint16_t stop[]= {
        2u << PIO_HW2WIRE_ICOUNT_LSB,
        set_scl_sda_program_instructions[I2C_SC0_SD0],  // need to tailor this....
        set_scl_sda_program_instructions[I2C_SC1_SD0], // Release clock
        set_scl_sda_program_instructions[I2C_SC0_SD0] // lower clock - need to manage data here...
    };
    pio_hw2wire_put_instructions(pio, sm, stop, count_of(stop));    
} 

void pio_hw2wire_start(PIO pio, uint sm) {
    const uint16_t start[]= {
        1u << PIO_HW2WIRE_ICOUNT_LSB, // Escape code for 2 instruction sequence
        set_scl_sda_program_instructions[I2C_SC1_SD0],  // We are already in idle state, just pull SDA low
        set_scl_sda_program_instructions[I2C_SC0_SD0] // Also pull clock low so we can present data
    };
    pio_hw2wire_put_instructions(pio, sm, start, count_of(start));    
}

void pio_hw2wire_stop(PIO pio, uint sm) {
    const uint16_t stop[]= {
        2u << PIO_HW2WIRE_ICOUNT_LSB,
        set_scl_sda_program_instructions[I2C_SC0_SD0],  // SDA is unknown; pull it down
        set_scl_sda_program_instructions[I2C_SC1_SD0], // Release clock
        set_scl_sda_program_instructions[I2C_SC1_SD1] // Release SDA to return to idle state
    };
    pio_hw2wire_put_instructions(pio, sm, stop, count_of(stop));    
} 

void pio_hw2wire_restart(PIO pio, uint sm)  {
    const uint16_t restart[]= {
        3u << PIO_HW2WIRE_ICOUNT_LSB,
        set_scl_sda_program_instructions[I2C_SC0_SD1],
        set_scl_sda_program_instructions[I2C_SC1_SD1],
        set_scl_sda_program_instructions[I2C_SC1_SD0],
        set_scl_sda_program_instructions[I2C_SC0_SD0]
    };
    pio_hw2wire_put_instructions(pio, sm, restart, count_of(restart));    
}