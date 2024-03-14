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


void pio_hw2wire_rx_enable(PIO pio, uint sm, bool en) 
{
    if (en)
    {
        hw_set_bits(&pio->sm[sm].shiftctrl, PIO_SM0_SHIFTCTRL_AUTOPUSH_BITS);
    }
    else
    {
        hw_clear_bits(&pio->sm[sm].shiftctrl, PIO_SM0_SHIFTCTRL_AUTOPUSH_BITS);
    }
}


static inline uint32_t pio_hw2wire_wait_idle_timeout(PIO pio, uint sm, uint32_t timeout)
{
    pio->fdebug = 1u << (PIO_FDEBUG_TXSTALL_LSB + sm);
    while(!(pio->fdebug & 1u << (PIO_FDEBUG_TXSTALL_LSB + sm)))
    {
        if(pio_hw2wire_check_error(pio, sm))
        {
            return 1; //TODO: MAKE MODE ERROR CODES
        }

        timeout--;
        if(!timeout) return 2;
    }

    if(pio_hw2wire_check_error(pio, sm))
    {
        return 1;
    }

    return 0;
}


static inline uint32_t pio_hw2wire_put16_or_timeout(PIO pio, uint sm, uint16_t data, uint32_t timeout) 
{
    while(pio_sm_is_tx_fifo_full(pio, sm))
    {
        timeout--;
        if(!timeout) return 2;
    }
    
    // some versions of GCC dislike this
    #ifdef __GNUC__
    #pragma GCC diagnostic push
    #pragma GCC diagnostic ignored "-Wstrict-aliasing"
    #endif
        *(io_rw_16 *)&pio->txf[sm] = data;
    #ifdef __GNUC__
    #pragma GCC diagnostic pop
    #endif
    return 0;
}

//put to fifo, wait for slot, wait for empyt
static inline uint32_t pio_hw2wire_put_or_timeout(PIO pio, uint sm, uint16_t data, uint32_t timeout) 
{   
    uint32_t error;

    error=pio_hw2wire_wait_idle_timeout(pio, sm, timeout);
    if(error) return error;

    // some versions of GCC dislike this
    #ifdef __GNUC__
    #pragma GCC diagnostic push
    #pragma GCC diagnostic ignored "-Wstrict-aliasing"
    #endif
        *(io_rw_16 *)&pio->txf[sm] = data;
    #ifdef __GNUC__
    #pragma GCC diagnostic pop
    #endif

    error=pio_hw2wire_wait_idle_timeout(pio, sm, timeout);

    return error;
}

uint8_t pio_hw2wire_get(PIO pio, uint sm) {
    return (uint8_t)pio_sm_get(pio, sm);
}

static inline uint32_t pio_hw2wire_put_instructions(PIO pio, uint sm, const uint16_t *inst, uint8_t length, uint32_t timeout)    
{
    uint32_t error;
    for(uint8_t i=0; i<length; i++)
    {
        error=pio_hw2wire_put_or_timeout(pio, sm, inst[i], timeout);
        if(error) return error;
    }
    return 0;
}

uint32_t pio_hw2wire_start_timeout(PIO pio, uint sm, uint32_t timeout) 
{
    const uint16_t start[]=
    {
        1u << PIO_HW2WIRE_ICOUNT_LSB, // Escape code for 2 instruction sequence
        set_scl_sda_program_instructions[I2C_SC1_SD0],  // We are already in idle state, just pull SDA low
        set_scl_sda_program_instructions[I2C_SC0_SD0] // Also pull clock low so we can present data
    };
    return pio_hw2wire_put_instructions(pio, sm, start, count_of(start), timeout);    
}


uint32_t pio_hw2wire_stop_timeout(PIO pio, uint sm, uint32_t timeout) 
{
    const uint16_t stop[]=
    {
        2u << PIO_HW2WIRE_ICOUNT_LSB,
        set_scl_sda_program_instructions[I2C_SC0_SD0],  // SDA is unknown; pull it down
        set_scl_sda_program_instructions[I2C_SC1_SD0], // Release clock
        set_scl_sda_program_instructions[I2C_SC1_SD1] // Release SDA to return to idle state 
    };
    return pio_hw2wire_put_instructions(pio, sm, stop, count_of(stop), timeout);    
};

uint32_t pio_hw2wire_restart_timeout(PIO pio, uint sm, uint32_t timeout) 
{
    const uint16_t restart[]=
    {
        3u << PIO_HW2WIRE_ICOUNT_LSB,
        set_scl_sda_program_instructions[I2C_SC0_SD1],
        set_scl_sda_program_instructions[I2C_SC1_SD1],
        set_scl_sda_program_instructions[I2C_SC1_SD0],
        set_scl_sda_program_instructions[I2C_SC0_SD0]
    };

    return pio_hw2wire_put_instructions(pio, sm, restart, count_of(restart), timeout);    
}

#if 0
void pio_hw2wire_resume_after_error(PIO pio, uint sm) {
    pio_sm_drain_tx_fifo(pio, sm);
    pio_sm_exec(pio, sm, (pio->sm[sm].execctrl & PIO_SM0_EXECCTRL_WRAP_BOTTOM_BITS) >> PIO_SM0_EXECCTRL_WRAP_BOTTOM_LSB);
    pio_interrupt_clear(pio, sm);
    pio_hw2wire_rx_enable(pio, sm, false); 
    pio_hw2wire_stop_timeout(pio, sm, 0xff);
    pio_hw2wire_stop_timeout(pio, sm, 0xff);
    pio_hw2wire_stop_timeout(pio, sm, 0xff);
    pio_sm_drain_tx_fifo(pio, sm);
    pio_sm_exec(pio, sm, (pio->sm[sm].execctrl & PIO_SM0_EXECCTRL_WRAP_BOTTOM_BITS) >> PIO_SM0_EXECCTRL_WRAP_BOTTOM_LSB);
    pio_interrupt_clear(pio, sm);
}
#endif
uint32_t pio_hw2wire_write_timeout(PIO pio, uint sm, uint32_t data, uint32_t timeout)
{
    uint32_t error;

    error=pio_hw2wire_wait_idle_timeout(pio, sm, timeout);
    if(error) return error;

    error=pio_hw2wire_put_or_timeout(pio, sm, (data << 1)|(1u), timeout);
    if(error) return error;

    error=pio_hw2wire_wait_idle_timeout(pio, sm, timeout);

    return error;
}

uint32_t pio_hw2wire_read_timeout(PIO pio, uint sm, uint32_t *data, bool ack, uint32_t timeout)
{
    uint32_t error;

    error=pio_hw2wire_wait_idle_timeout(pio, sm, timeout);
    if(error) return error;
    
	while(!pio_sm_is_rx_fifo_empty(pio, sm))
    {
        (void)pio_hw2wire_get(pio, sm);
    }

    error=pio_hw2wire_put16_or_timeout(pio, sm, (0xffu << 1) | (ack?0:(1u << 9) | (1u << 0)), timeout);
    if(error) return error;

    uint32_t to=timeout;
    while(pio_sm_is_rx_fifo_empty(pio, sm))
    {
        to--;
        if(!to) return 2;
    }

	(*data) = pio_hw2wire_get(pio, sm);
	
    error=pio_hw2wire_wait_idle_timeout(pio, sm, timeout);
    
    return error;
}

/////////////////////////////
// Old functions


// If I2C is ok, block and push data. Otherwise fall straight through.
void pio_hw2wire_put_or_err(PIO pio, uint sm, uint16_t data) {
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
}


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
}

void pio_hw2wire_get16(PIO pio, uint sm, uint32_t *data) {
	while(!pio_sm_is_rx_fifo_empty(pio, sm))
    {
        (void)pio_hw2wire_get(pio, sm);
    }

    pio_hw2wire_put16(pio, sm, (0xffu << 1) | (0?0:(1u << 9) | (1u << 0)));

    while(pio_sm_is_rx_fifo_empty(pio, sm));

	(*data) = pio_hw2wire_get(pio, sm);
}

void pio_hw2wire_reset(PIO pio, uint sm) {
    pio_hw2wire_put_or_err(pio, sm, 1u << PIO_HW2WIRE_ICOUNT_LSB); // Escape code for 2 instruction sequence
    pio_hw2wire_put_or_err(pio, sm, set_scl_sda_program_instructions[I2C_SC1_SD1]);    // reset to high to release
    pio_hw2wire_put_or_err(pio, sm, set_scl_sda_program_instructions[I2C_SC0_SD0]);    // pull both low so clock goes high
}

void pio_hw2wire_clock_tick(PIO pio, uint sm) {
    pio_hw2wire_put_or_err(pio, sm, 2u << PIO_HW2WIRE_ICOUNT_LSB);
    pio_hw2wire_put_or_err(pio, sm, set_scl_sda_program_instructions[I2C_SC0_SD0]);    // SDA is unknown; pull it down
    pio_hw2wire_put_or_err(pio, sm, set_scl_sda_program_instructions[I2C_SC1_SD0]);    // Release clock
    pio_hw2wire_put_or_err(pio, sm, set_scl_sda_program_instructions[I2C_SC0_SD0]);    // Release SDA to return to idle state
};

void pio_hw2wire_start(PIO pio, uint sm) {
    pio_hw2wire_put_or_err(pio, sm, 1u << PIO_HW2WIRE_ICOUNT_LSB); // Escape code for 2 instruction sequence
    pio_hw2wire_put_or_err(pio, sm, set_scl_sda_program_instructions[I2C_SC1_SD0]);    // We are already in idle state, just pull SDA low
    pio_hw2wire_put_or_err(pio, sm, set_scl_sda_program_instructions[I2C_SC0_SD0]);    // Also pull clock low so we can present data
}

void pio_hw2wire_stop(PIO pio, uint sm) {
    pio_hw2wire_put_or_err(pio, sm, 2u << PIO_HW2WIRE_ICOUNT_LSB);
    pio_hw2wire_put_or_err(pio, sm, set_scl_sda_program_instructions[I2C_SC0_SD0]);    // SDA is unknown; pull it down
    pio_hw2wire_put_or_err(pio, sm, set_scl_sda_program_instructions[I2C_SC1_SD0]);    // Release clock
    pio_hw2wire_put_or_err(pio, sm, set_scl_sda_program_instructions[I2C_SC1_SD1]);    // Release SDA to return to idle state
};


void pio_hw2wire_repstart(PIO pio, uint sm) {
    pio_hw2wire_put_or_err(pio, sm, 3u << PIO_HW2WIRE_ICOUNT_LSB);
    pio_hw2wire_put_or_err(pio, sm, set_scl_sda_program_instructions[I2C_SC0_SD1]);
    pio_hw2wire_put_or_err(pio, sm, set_scl_sda_program_instructions[I2C_SC1_SD1]);
    pio_hw2wire_put_or_err(pio, sm, set_scl_sda_program_instructions[I2C_SC1_SD0]);
    pio_hw2wire_put_or_err(pio, sm, set_scl_sda_program_instructions[I2C_SC0_SD0]);
}


void pio_hw2wire_wait_idle(PIO pio, uint sm) {
    uint32_t timeout=10000;
    // Finished when TX runs dry or SM hits an IRQ
    pio->fdebug = 1u << (PIO_FDEBUG_TXSTALL_LSB + sm);
    while (!(pio->fdebug & 1u << (PIO_FDEBUG_TXSTALL_LSB + sm)) && timeout)
    {
        tight_loop_contents();
        timeout--;
    }
}
