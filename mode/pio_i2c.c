/**
 * Copyright (c) 2021 Raspberry Pi (Trading) Ltd.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include "mode/pio_i2c.h"

const int PIO_I2C_ICOUNT_LSB = 10;
const int PIO_I2C_FINAL_LSB  = 9;
const int PIO_I2C_DATA_LSB   = 1;
const int PIO_I2C_NAK_LSB    = 0;


bool pio_i2c_check_error(PIO pio, uint sm) {
    return pio_interrupt_get(pio, sm);
}

void pio_i2c_rx_enable(PIO pio, uint sm, bool en) 
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


static inline uint32_t pio_i2c_wait_idle_timeout(PIO pio, uint sm, uint32_t timeout)
{
    pio->fdebug = 1u << (PIO_FDEBUG_TXSTALL_LSB + sm);
    while(!(pio->fdebug & 1u << (PIO_FDEBUG_TXSTALL_LSB + sm)))
    {
        if(pio_i2c_check_error(pio, sm))
        {
            return 1; //TODO: MAKE MODE ERROR CODES
        }

        timeout--;
        if(!timeout) return 2;
    }

    if(pio_i2c_check_error(pio, sm))
    {
        return 1;
    }

    return 0;
}


static inline uint32_t pio_i2c_put16_or_timeout(PIO pio, uint sm, uint16_t data, uint32_t timeout) 
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
static inline uint32_t pio_i2c_put_or_timeout(PIO pio, uint sm, uint16_t data, uint32_t timeout) 
{   
    uint32_t error;

    error=pio_i2c_wait_idle_timeout(pio, sm, timeout);
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

    error=pio_i2c_wait_idle_timeout(pio, sm, timeout);

    return error;
}

uint8_t pio_i2c_get(PIO pio, uint sm) {
    return (uint8_t)pio_sm_get(pio, sm);
}

static inline uint32_t pio_i2c_put_instructions(PIO pio, uint sm, const uint16_t *inst, uint8_t length, uint32_t timeout)    
{
    uint32_t error;
    for(uint8_t i=0; i<length; i++)
    {
        error=pio_i2c_put_or_timeout(pio, sm, inst[i], timeout);
        if(error) return error;
    }
    return 0;
}

uint32_t pio_i2c_start_timeout(PIO pio, uint sm, uint32_t timeout) 
{
    const uint16_t start[]=
    {
        1u << PIO_I2C_ICOUNT_LSB, // Escape code for 2 instruction sequence
        set_scl_sda_program_instructions[I2C_SC1_SD0],  // We are already in idle state, just pull SDA low
        set_scl_sda_program_instructions[I2C_SC0_SD0] // Also pull clock low so we can present data
    };
    return pio_i2c_put_instructions(pio, sm, start, count_of(start), timeout);    
}


uint32_t pio_i2c_stop_timeout(PIO pio, uint sm, uint32_t timeout) 
{
    const uint16_t stop[]=
    {
        2u << PIO_I2C_ICOUNT_LSB,
        set_scl_sda_program_instructions[I2C_SC0_SD0],  // SDA is unknown; pull it down
        set_scl_sda_program_instructions[I2C_SC1_SD0], // Release clock
        set_scl_sda_program_instructions[I2C_SC1_SD1] // Release SDA to return to idle state 
    };
    return pio_i2c_put_instructions(pio, sm, stop, count_of(stop), timeout);    
};

uint32_t pio_i2c_restart_timeout(PIO pio, uint sm, uint32_t timeout) 
{
    const uint16_t restart[]=
    {
        3u << PIO_I2C_ICOUNT_LSB,
        set_scl_sda_program_instructions[I2C_SC0_SD1],
        set_scl_sda_program_instructions[I2C_SC1_SD1],
        set_scl_sda_program_instructions[I2C_SC1_SD0],
        set_scl_sda_program_instructions[I2C_SC0_SD0]
    };

    return pio_i2c_put_instructions(pio, sm, restart, count_of(restart), timeout);    
}


void pio_i2c_resume_after_error(PIO pio, uint sm) {
    pio_sm_drain_tx_fifo(pio, sm);
    pio_sm_exec(pio, sm, (pio->sm[sm].execctrl & PIO_SM0_EXECCTRL_WRAP_BOTTOM_BITS) >> PIO_SM0_EXECCTRL_WRAP_BOTTOM_LSB);
    pio_interrupt_clear(pio, sm);
    pio_i2c_rx_enable(pio, sm, false); 
    pio_i2c_stop_timeout(pio, sm, 0xff);
    pio_i2c_stop_timeout(pio, sm, 0xff);
    pio_i2c_stop_timeout(pio, sm, 0xff);
    pio_sm_drain_tx_fifo(pio, sm);
    pio_sm_exec(pio, sm, (pio->sm[sm].execctrl & PIO_SM0_EXECCTRL_WRAP_BOTTOM_BITS) >> PIO_SM0_EXECCTRL_WRAP_BOTTOM_LSB);
    pio_interrupt_clear(pio, sm);
}

uint32_t pio_i2c_write_timeout(PIO pio, uint sm, uint32_t data, uint32_t timeout)
{
    uint32_t error;

    error=pio_i2c_wait_idle_timeout(pio, sm, timeout);
    if(error) return error;

    error=pio_i2c_put_or_timeout(pio, sm, (data << 1)|(1u), timeout);
    if(error) return error;

    error=pio_i2c_wait_idle_timeout(pio, sm, timeout);

    return error;
}

uint32_t pio_i2c_read_timeout(PIO pio, uint sm, uint32_t *data, bool ack, uint32_t timeout)
{
    uint32_t error;

    error=pio_i2c_wait_idle_timeout(pio, sm, timeout);
    if(error) return error;
    
	while(!pio_sm_is_rx_fifo_empty(pio, sm))
    {
        (void)pio_i2c_get(pio, sm);
    }

    error=pio_i2c_put16_or_timeout(pio, sm, (0xffu << 1) | (ack?0:(1u << 9) | (1u << 0)), timeout);
    if(error) return error;

    uint32_t to=timeout;
    while(pio_sm_is_rx_fifo_empty(pio, sm))
    {
        to--;
        if(!to) return 2;
    }

	(*data) = pio_i2c_get(pio, sm);
	
    error=pio_i2c_wait_idle_timeout(pio, sm, timeout);
    
    return error;
}


uint32_t pio_i2c_write_blocking_timeout(PIO pio, uint sm, uint8_t addr, uint8_t *txbuf, uint len, uint32_t timeout) 
{
    int err = 0;
    if(pio_i2c_start_timeout(pio, sm, timeout)) return 1;
    pio_i2c_rx_enable(pio, sm, false);
    if(pio_i2c_put16_or_timeout(pio, sm, (addr << 1) | 1u, timeout)) return 1;
    uint32_t temp=timeout;
    while(len && !pio_i2c_check_error(pio, sm))
    {
        if(!pio_sm_is_tx_fifo_full(pio, sm)) 
        {
            --len;
            if(pio_i2c_put_or_timeout(pio, sm, (*txbuf++ << PIO_I2C_DATA_LSB) | ((len == 0) << PIO_I2C_FINAL_LSB) | 1u, timeout)) return 1;
        }
        temp--;
        if(!temp) return 1;
    }
    if(pio_i2c_stop_timeout(pio, sm, timeout)) return 1;
    if(pio_i2c_wait_idle_timeout(pio, sm, timeout)) return 1;
    if(pio_i2c_check_error(pio, sm)) 
    {
        //err = -1; 
        pio_i2c_resume_after_error(pio, sm);
        //pio_i2c_stop_timeout(pio, sm, timeout);
        return 1;
    }
    return 0;
}

uint32_t pio_i2c_read_blocking_timeout(PIO pio, uint sm, uint8_t addr, uint8_t *rxbuf, uint len, uint32_t timeout) 
{
    int err = 0;
    if(pio_i2c_start_timeout(pio, sm, timeout)) return 1;
    pio_i2c_rx_enable(pio, sm, true); 
    while(!pio_sm_is_rx_fifo_empty(pio, sm))
    {
        (void)pio_i2c_get(pio, sm); 
    }
    if(pio_i2c_put16_or_timeout(pio, sm, (addr << 1) | 3u, timeout)) return 1;
    uint32_t tx_remain = len; // Need to stuff 0xff bytes in to get clocks

    bool first = true;
    uint32_t temp=timeout;
    while((tx_remain || len) && !pio_i2c_check_error(pio, sm))
    {
        if(tx_remain && !pio_sm_is_tx_fifo_full(pio, sm)) 
        {
            --tx_remain;
            if(pio_i2c_put16_or_timeout(pio, sm, (0xffu << 1) | (tx_remain ? 0 : (1u << PIO_I2C_FINAL_LSB) | (1u << PIO_I2C_NAK_LSB)), timeout)) return 1;
            temp=timeout; //reset the counter
        }
        if(!pio_sm_is_rx_fifo_empty(pio, sm)) 
        {
            if(first) 
            {
                // Ignore returned address byte
                (void)pio_i2c_get(pio, sm);
                first = false;
            }
            else 
            {
                --len;
                *rxbuf++ = pio_i2c_get(pio, sm);
            }
        }

        temp--;
        if(!temp) return 1;
    }
    if(pio_i2c_stop_timeout(pio, sm, timeout)) return 1;
    if(pio_i2c_wait_idle_timeout(pio, sm, timeout)) return 1;
    if(pio_i2c_check_error(pio, sm)) 
    {
        pio_i2c_resume_after_error(pio, sm);
        //pio_i2c_stop_timeout(pio, sm, timeout);
        return 1;
    }
    return 0;
}

//one full I2C transaction with error and timeout
uint32_t pio_i2c_transaction_blocking_timeout(PIO pio, uint sm, uint8_t addr, uint8_t *txbuf, uint txlen, uint8_t *rxbuf, uint rxlen, uint32_t timeout) 
{
    if(pio_i2c_write_blocking_timeout(pio, sm, addr, txbuf, txlen, timeout)) return 1;
    if(rxlen>0)
    {
        if(pio_i2c_read_blocking_timeout(pio, sm, addr,rxbuf, rxlen, timeout)) return 1;
    }
    return 0;
}

//////////////////////////////////////
// Full I2C packet functions (no timeout)

int pio_i2c_write_blocking(PIO pio, uint sm, uint8_t addr, uint8_t *txbuf, uint len) {
    int err = 0;
    pio_i2c_start(pio, sm);
    pio_i2c_rx_enable(pio, sm, false);
    pio_i2c_put16(pio, sm, (addr << 2) | 1u);
    while (len && !pio_i2c_check_error(pio, sm)) {
        if (!pio_sm_is_tx_fifo_full(pio, sm)) {
            --len;
            pio_i2c_put_or_err(pio, sm, (*txbuf++ << PIO_I2C_DATA_LSB) | ((len == 0) << PIO_I2C_FINAL_LSB) | 1u);
        }
    }
    pio_i2c_stop(pio, sm);
    pio_i2c_wait_idle(pio, sm);
    if (pio_i2c_check_error(pio, sm)) {
        err = -1; 
        pio_i2c_resume_after_error(pio, sm);
        //pio_i2c_stop(pio, sm);
    }
    return err;
}

int pio_i2c_read_blocking(PIO pio, uint sm, uint8_t addr, uint8_t *rxbuf, uint len) {
    int err = 0;
    pio_i2c_start(pio, sm);
    pio_i2c_rx_enable(pio, sm, true); 
    while (!pio_sm_is_rx_fifo_empty(pio, sm))
        (void)pio_i2c_get(pio, sm);
    pio_i2c_put16(pio, sm, (addr << 2) | 3u);
    uint32_t tx_remain = len; // Need to stuff 0xff bytes in to get clocks

    bool first = true;

    while ((tx_remain || len) && !pio_i2c_check_error(pio, sm)) {
        if (tx_remain && !pio_sm_is_tx_fifo_full(pio, sm)) {
            --tx_remain;
            pio_i2c_put16(pio, sm, (0xffu << 1) | (tx_remain ? 0 : (1u << PIO_I2C_FINAL_LSB) | (1u << PIO_I2C_NAK_LSB)));
        }
        if (!pio_sm_is_rx_fifo_empty(pio, sm)) {
            if (first) {
                // Ignore returned address byte
                (void)pio_i2c_get(pio, sm);
                first = false;
            }
            else {
                --len;
                *rxbuf++ = pio_i2c_get(pio, sm);
            }
        }
    }
    pio_i2c_stop(pio, sm);
    pio_i2c_wait_idle(pio, sm);
    if (pio_i2c_check_error(pio, sm)) {
        err = -1;
        pio_i2c_resume_after_error(pio, sm);
        //pio_i2c_stop(pio, sm);
    }
    return err;
}

/////////////////////////////
// Old functions


// If I2C is ok, block and push data. Otherwise fall straight through.
void pio_i2c_put_or_err(PIO pio, uint sm, uint16_t data) {
    while (pio_sm_is_tx_fifo_full(pio, sm))
        if (pio_i2c_check_error(pio, sm))
            return;
    if (pio_i2c_check_error(pio, sm))
        return;
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


void pio_i2c_put16(PIO pio, uint sm, uint16_t data) {
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


void pio_i2c_start(PIO pio, uint sm) {
    pio_i2c_put_or_err(pio, sm, 1u << PIO_I2C_ICOUNT_LSB); // Escape code for 2 instruction sequence
    pio_i2c_put_or_err(pio, sm, set_scl_sda_program_instructions[I2C_SC1_SD0]);    // We are already in idle state, just pull SDA low
    pio_i2c_put_or_err(pio, sm, set_scl_sda_program_instructions[I2C_SC0_SD0]);    // Also pull clock low so we can present data
}

void pio_i2c_stop(PIO pio, uint sm) {
    pio_i2c_put_or_err(pio, sm, 2u << PIO_I2C_ICOUNT_LSB);
    pio_i2c_put_or_err(pio, sm, set_scl_sda_program_instructions[I2C_SC0_SD0]);    // SDA is unknown; pull it down
    pio_i2c_put_or_err(pio, sm, set_scl_sda_program_instructions[I2C_SC1_SD0]);    // Release clock
    pio_i2c_put_or_err(pio, sm, set_scl_sda_program_instructions[I2C_SC1_SD1]);    // Release SDA to return to idle state
};


void pio_i2c_repstart(PIO pio, uint sm) {
    pio_i2c_put_or_err(pio, sm, 3u << PIO_I2C_ICOUNT_LSB);
    pio_i2c_put_or_err(pio, sm, set_scl_sda_program_instructions[I2C_SC0_SD1]);
    pio_i2c_put_or_err(pio, sm, set_scl_sda_program_instructions[I2C_SC1_SD1]);
    pio_i2c_put_or_err(pio, sm, set_scl_sda_program_instructions[I2C_SC1_SD0]);
    pio_i2c_put_or_err(pio, sm, set_scl_sda_program_instructions[I2C_SC0_SD0]);
}


void pio_i2c_wait_idle(PIO pio, uint sm) {
    uint32_t timeout=10000;
    // Finished when TX runs dry or SM hits an IRQ
    pio->fdebug = 1u << (PIO_FDEBUG_TXSTALL_LSB + sm);
    while (!(pio->fdebug & 1u << (PIO_FDEBUG_TXSTALL_LSB + sm) || pio_i2c_check_error(pio, sm)) && timeout)
    {
        tight_loop_contents();
        timeout--;
    }
}
