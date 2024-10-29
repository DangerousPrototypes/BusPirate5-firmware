/**
 * Copyright (c) 2021 Raspberry Pi (Trading) Ltd.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 *
 * **This is a heavily modified version of the PIO I2C driver from the Raspberry Pi Pico SDK.**
 * Modified by: Bus Pirate project 2022, 2023, 2024 (Ian Lesnet)
 */
#include <stdio.h>
#include "pico/stdlib.h"
#include "pirate.h"
#include "hardware/pio.h"
#include "pio_config.h"
#include "hwi2c_pio.h"

static struct _pio_config pio_config;

#define PIO_I2C_ICOUNT_LSB 10
#define PIO_I2C_FINAL_LSB 9
#define PIO_I2C_DATA_LSB 1
#define PIO_I2C_NAK_LSB 0

void pio_i2c_init(uint sda, uint scl, uint dir_sda, uint dir_scl, uint baudrate, bool clock_stretch) {
    // bool success = pio_claim_free_sm_and_add_program_for_gpio_range(&i2c_program, &pio_config.pio, &pio_config.sm,
    // &pio_config.offset, dir_sda, 10, true); hard_assert(success);
    pio_config.pio = PIO_MODE_PIO;
    pio_config.sm = 0;
    if(clock_stretch) {
        pio_config.program = &i2c_clock_stretch_program;
        i2c_clock_stretch_program_init(pio_config.pio, pio_config.sm, pio_config.offset, sda, scl, dir_sda, dir_scl, baudrate);
    } else {
        pio_config.program = &i2c_program;
        i2c_program_init(pio_config.pio, pio_config.sm, pio_config.offset, sda, scl, dir_sda, dir_scl, baudrate);
    }
    pio_config.offset = pio_add_program(pio_config.pio, pio_config.program);
    #ifdef BP_PIO_SHOW_ASSIGNMENT
        printf("PIO: pio=%d, sm=%d, offset=%d\r\n", PIO_NUM(pio_config.pio), pio_config.sm, pio_config.offset);
    #endif
}

void pio_i2c_cleanup(void) {
    // pio_remove_program_and_unclaim_sm(&i2c_program, pio_config.pio, pio_config.sm, pio_config.offset);
    pio_remove_program(pio_config.pio, pio_config.program, pio_config.offset);
}


static inline uint8_t pio_i2c_get(void) {
    return (uint8_t)pio_sm_get(pio_config.pio, pio_config.sm);
}

void pio_i2c_resume_after_error() {
    pio_sm_drain_tx_fifo(pio_config.pio, pio_config.sm);
    //TODO: perhaps drain the RX as well?
    while (!pio_sm_is_rx_fifo_empty(pio_config.pio, pio_config.sm)) {
        (void)pio_i2c_get();
    }
    pio_sm_exec(pio_config.pio,
                pio_config.sm,
                (pio_config.pio->sm[pio_config.sm].execctrl & PIO_SM0_EXECCTRL_WRAP_BOTTOM_BITS) >>
                    PIO_SM0_EXECCTRL_WRAP_BOTTOM_LSB);

    /*pio_interrupt_clear(pio_config.pio, pio_config.sm);
    pio_i2c_rx_enable(false);
    pio_i2c_stop_timeout(0xff);
    pio_i2c_stop_timeout(0xff);
    pio_i2c_stop_timeout(0xff);
    pio_sm_drain_tx_fifo(pio_config.pio, pio_config.sm);
    pio_sm_exec(pio_config.pio,
                pio_config.sm,
                (pio_config.pio->sm[pio_config.sm].execctrl & PIO_SM0_EXECCTRL_WRAP_BOTTOM_BITS) >>
                    PIO_SM0_EXECCTRL_WRAP_BOTTOM_LSB);
    pio_interrupt_clear(pio_config.pio, pio_config.sm);*/
}


/* This should no longer be needed
void pio_i2c_rx_enable(bool en) {
    //if (en) {
        hw_set_bits(&pio_config.pio->sm[pio_config.sm].shiftctrl, PIO_SM0_SHIFTCTRL_AUTOPUSH_BITS);
    //} else {
    //    hw_clear_bits(&pio_config.pio->sm[pio_config.sm].shiftctrl, PIO_SM0_SHIFTCTRL_AUTOPUSH_BITS);
    //}
}
*/
// wait for state machine to be idle, return false if timeout
static inline hwi2c_status_t pio_i2c_wait_idle_timeout(uint32_t timeout) {
    pio_config.pio->fdebug = 1u << (PIO_FDEBUG_TXSTALL_LSB + pio_config.sm);
    while (!(pio_config.pio->fdebug & 1u << (PIO_FDEBUG_TXSTALL_LSB + pio_config.sm))) {
        timeout--;
        if (!timeout) {
            return HWI2C_TIMEOUT;
        }
    }
    return HWI2C_OK;
}

static inline hwi2c_status_t pio_i2c_put_timeout(uint16_t data, uint32_t timeout) {
    //this is a precaution, we should never leave with stuff in the fifo
    while (pio_sm_is_tx_fifo_full(pio_config.pio, pio_config.sm)) {
        timeout--;
        if (!timeout) return HWI2C_TIMEOUT;
    }

    // some versions of GCC dislike this
    #ifdef __GNUC__
    #pragma GCC diagnostic push
    #pragma GCC diagnostic ignored "-Wstrict-aliasing"
    #endif
        *(io_rw_16*)&pio_config.pio->txf[pio_config.sm] = data;
    #ifdef __GNUC__
    #pragma GCC diagnostic pop
    #endif
    return HWI2C_OK;
}

// put to fifo, wait for slot, wait for empty
// returns false on fail, true on success
static inline hwi2c_status_t pio_i2c_put_blocking_timeout(uint16_t data, uint32_t timeout) {
    if(!pio_i2c_wait_idle_timeout(timeout)) return HWI2C_TIMEOUT;

    // some versions of GCC dislike this
    #ifdef __GNUC__
    #pragma GCC diagnostic push
    #pragma GCC diagnostic ignored "-Wstrict-aliasing"
    #endif
        *(io_rw_16*)&pio_config.pio->txf[pio_config.sm] = data;
    #ifdef __GNUC__
    #pragma GCC diagnostic pop
    #endif

    if(!pio_i2c_wait_idle_timeout(timeout)) return HWI2C_TIMEOUT;
    return HWI2C_OK;
}

/*
* Functions for I2C START, STOP, RESTART
*
*/

// put a sequence of instructions to the PIO, return false on fail, true on success
static inline hwi2c_status_t pio_i2c_put_instructions_timeout(const uint16_t* inst, uint8_t length, uint32_t timeout) {
    for (uint8_t i = 0; i < length; i++) {
        if(pio_i2c_put_blocking_timeout(inst[i], timeout)) return HWI2C_TIMEOUT;
    }
    return HWI2C_OK;
}

hwi2c_status_t pio_i2c_start_timeout(uint32_t timeout) {
    const uint16_t start[] = {
        1u << PIO_I2C_ICOUNT_LSB,                      // Escape code for 2 instruction sequence
        set_scl_sda_program_instructions[I2C_SC1_SD0], // We are already in idle state, just pull SDA low
        set_scl_sda_program_instructions[I2C_SC0_SD0]  // Also pull clock low so we can present data
    };
    return pio_i2c_put_instructions_timeout(start, count_of(start), timeout);
}

hwi2c_status_t pio_i2c_stop_timeout(uint32_t timeout) {
    const uint16_t stop[] = {
        2u << PIO_I2C_ICOUNT_LSB,
        set_scl_sda_program_instructions[I2C_SC0_SD0], // SDA is unknown; pull it down
        set_scl_sda_program_instructions[I2C_SC1_SD0], // Release clock
        set_scl_sda_program_instructions[I2C_SC1_SD1]  // Release SDA to return to idle state
    };
    return pio_i2c_put_instructions_timeout(stop, count_of(stop), timeout);
};

hwi2c_status_t pio_i2c_restart_timeout(uint32_t timeout) {
    const uint16_t restart[] = { 3u << PIO_I2C_ICOUNT_LSB,
                                 set_scl_sda_program_instructions[I2C_SC0_SD1],
                                 set_scl_sda_program_instructions[I2C_SC1_SD1],
                                 set_scl_sda_program_instructions[I2C_SC1_SD0],
                                 set_scl_sda_program_instructions[I2C_SC0_SD0] };
    return pio_i2c_put_instructions_timeout(restart, count_of(restart), timeout);
}

/*
* Functions for single byte I2C transactions
*
*/
// write an I2C byte, return false on fail, true on success
hwi2c_status_t pio_i2c_transaction_timeout(uint32_t out_data, uint32_t* in_data, uint32_t timeout) {

    if(!pio_i2c_wait_idle_timeout(timeout)) return HWI2C_TIMEOUT;

    //remove any data from the RX FIFO
    while (!pio_sm_is_rx_fifo_empty(pio_config.pio, pio_config.sm)) {
        (void)pio_i2c_get();
    }

    if(!pio_i2c_put_blocking_timeout(out_data, timeout)) return HWI2C_TIMEOUT;

    uint32_t to = timeout;
    while (pio_sm_is_rx_fifo_empty(pio_config.pio, pio_config.sm)) {
        to--;
        if (!to) return HWI2C_TIMEOUT;
    }
    (*in_data) = pio_i2c_get();

    if(!pio_i2c_wait_idle_timeout(timeout)) return HWI2C_TIMEOUT;
    
    return HWI2C_OK;
}

hwi2c_status_t pio_i2c_write_timeout(uint32_t out_data, uint32_t timeout) {
    uint32_t in_data;
    hwi2c_status_t i2c_result = pio_i2c_transaction_timeout((out_data << 1) | (1u), &in_data, timeout);
    if(i2c_result != HWI2C_OK) return i2c_result;
    return (in_data & 0b1) ? HWI2C_NACK : HWI2C_OK;
}

hwi2c_status_t pio_i2c_read_timeout(uint32_t* in_data, bool ack, uint32_t timeout) {
    return pio_i2c_transaction_timeout((0xff << 1) | (ack ? 0 : (1u << 0)), in_data, timeout);
}

/*
* functions for bulk I2C transactions
* TODO: handle ACK NACK in a new way!
*/
// write an array to I2C with start and stop, return false on fail, true on success
hwi2c_status_t pio_i2c_write_array_timeout(uint8_t addr, uint8_t* txbuf, uint len, uint32_t timeout) {
    if(pio_i2c_start_timeout(timeout)) return HWI2C_TIMEOUT;
    hwi2c_status_t i2c_result = pio_i2c_write_timeout(addr, timeout);
    if(i2c_result != HWI2C_OK) return i2c_result;

    uint32_t temp = timeout;
    while (len) {
        if (!pio_sm_is_tx_fifo_full(pio_config.pio, pio_config.sm)) {
            --len;
            //TODO: fix final bit
            //if (pio_i2c_put_timeout((*txbuf++ << PIO_I2C_DATA_LSB) | ((len == 0) | 1u), timeout)) return HWI2C_TIMEOUT;
            i2c_result = pio_i2c_write_timeout(*txbuf++, timeout);
            if(i2c_result != HWI2C_OK) return i2c_result;
        }
        temp--;
        if (!temp) return HWI2C_TIMEOUT;
    }
    if (pio_i2c_stop_timeout(timeout)) return HWI2C_TIMEOUT;
    if (pio_i2c_wait_idle_timeout(timeout)) return HWI2C_TIMEOUT;
    return HWI2C_OK;
}

// read an array from I2C with start and stop, return false on fail, true on success
hwi2c_status_t pio_i2c_read_array_timeout(uint8_t addr, uint8_t* rxbuf, uint len, uint32_t timeout) {
    while (!pio_sm_is_rx_fifo_empty(pio_config.pio, pio_config.sm)) {
        (void)pio_i2c_get();
    }
    if (pio_i2c_start_timeout(timeout)) return HWI2C_TIMEOUT;
    hwi2c_status_t i2c_result = pio_i2c_write_timeout(addr | 1u, timeout);
    if(i2c_result != HWI2C_OK) return i2c_result;

    uint32_t tx_remain = len; // Need to stuff 0xff bytes in to get clocks

    bool first = true;
    uint32_t temp = timeout;
    while ((tx_remain || len)) {
        if (tx_remain && !pio_sm_is_tx_fifo_full(pio_config.pio, pio_config.sm)) {
            --tx_remain;
            // NACK the final byte of the read
            if (pio_i2c_put_timeout((0xff << PIO_I2C_DATA_LSB) | (tx_remain ? 0 : 1u), timeout)) return HWI2C_TIMEOUT;
            temp = timeout; // reset the counter
        }
        if (!pio_sm_is_rx_fifo_empty(pio_config.pio, pio_config.sm)) {
            if (first) {
                // Ignore returned address byte
                (void)pio_i2c_get();
                first = false;
            } else {
                --len;
                *rxbuf++ = pio_i2c_get();
            }
        }

        temp--;
        if (!temp) return HWI2C_TIMEOUT;
    }
    if (pio_i2c_stop_timeout(timeout)) return HWI2C_TIMEOUT;
    if (pio_i2c_wait_idle_timeout(timeout)) return HWI2C_TIMEOUT;
    return HWI2C_OK;
}

// one full I2C transaction with error and timeout
hwi2c_status_t pio_i2c_transaction_array_timeout(uint8_t addr, uint8_t* txbuf, uint txlen, uint8_t* rxbuf, uint rxlen, uint32_t timeout) {
    
    hwi2c_status_t i2c_result = pio_i2c_write_array_timeout(addr, txbuf, txlen, timeout);
    if(i2c_result != HWI2C_OK) return i2c_result;

    if (rxlen > 0) {
        i2c_result = pio_i2c_read_array_timeout(addr, rxbuf, rxlen, timeout);
        if(i2c_result != HWI2C_OK) return i2c_result;
    }
    
    return HWI2C_OK;
}



