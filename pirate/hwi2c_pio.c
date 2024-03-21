/**
 * Copyright (c) 2021 Raspberry Pi (Trading) Ltd.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include "hwi2c_pio.h"

const int PIO_I2C_ICOUNT_LSB = 10;
const int PIO_I2C_FINAL_LSB  = 9;
const int PIO_I2C_DATA_LSB   = 1;
const int PIO_I2C_NAK_LSB    = 0;

static PIO hwi2c_pio;
static uint hwi2c_pio_state_machine;
static uint hwi2c_pio_loaded_offset;

void pio_i2c_init(PIO pio, uint sm, uint sda, uint scl, uint dir_sda, uint dir_scl, uint baudrate){
    hwi2c_pio=pio;
    hwi2c_pio_state_machine=sm;
    hwi2c_pio_loaded_offset = pio_add_program(hwi2c_pio, &i2c_program);
    i2c_program_init(pio, hwi2c_pio_state_machine, hwi2c_pio_loaded_offset, sda, scl, dir_sda, dir_scl, baudrate);
}

void pio_i2c_cleanup(void){
    pio_remove_program(hwi2c_pio, &i2c_program, hwi2c_pio_loaded_offset);
}

bool pio_i2c_check_error(void) {
    return pio_interrupt_get(hwi2c_pio, hwi2c_pio_state_machine);
}

void pio_i2c_rx_enable(bool en) {
    if (en) hw_set_bits(&hwi2c_pio->sm[hwi2c_pio_state_machine].shiftctrl, PIO_SM0_SHIFTCTRL_AUTOPUSH_BITS);
    else hw_clear_bits(&hwi2c_pio->sm[hwi2c_pio_state_machine].shiftctrl, PIO_SM0_SHIFTCTRL_AUTOPUSH_BITS);
}

static inline uint32_t pio_i2c_wait_idle_timeout(uint32_t timeout){
    hwi2c_pio->fdebug = 1u << (PIO_FDEBUG_TXSTALL_LSB + hwi2c_pio_state_machine);
    while(!(hwi2c_pio->fdebug & 1u << (PIO_FDEBUG_TXSTALL_LSB + hwi2c_pio_state_machine))){
        if(pio_i2c_check_error()){
            return 1; //TODO: MAKE MODE ERROR CODES
        }
        timeout--;
        if(!timeout) return 2;
    }

    if(pio_i2c_check_error()){
        return 1;
    }

    return 0;
}


static inline uint32_t pio_i2c_put16_or_timeout(uint16_t data, uint32_t timeout) {
    while(pio_sm_is_tx_fifo_full(hwi2c_pio, hwi2c_pio_state_machine)){
        timeout--;
        if(!timeout) return 2;
    }
    
    // some versions of GCC dislike this
    #ifdef __GNUC__
    #pragma GCC diagnostic push
    #pragma GCC diagnostic ignored "-Wstrict-aliasing"
    #endif
        *(io_rw_16 *)&hwi2c_pio->txf[hwi2c_pio_state_machine] = data;
    #ifdef __GNUC__
    #pragma GCC diagnostic pop
    #endif
    return 0;
}

//put to fifo, wait for slot, wait for empyt
static inline uint32_t pio_i2c_put_or_timeout(uint16_t data, uint32_t timeout){   
    uint32_t error;

    error=pio_i2c_wait_idle_timeout(timeout);
    if(error) return error;
    // some versions of GCC dislike this
    #ifdef __GNUC__
    #pragma GCC diagnostic push
    #pragma GCC diagnostic ignored "-Wstrict-aliasing"
    #endif
        *(io_rw_16 *)&hwi2c_pio->txf[hwi2c_pio_state_machine] = data;
    #ifdef __GNUC__
    #pragma GCC diagnostic pop
    #endif
    error=pio_i2c_wait_idle_timeout(timeout);
    return error;
}

uint8_t pio_i2c_get(void) {
    return (uint8_t)pio_sm_get(hwi2c_pio, hwi2c_pio_state_machine);
}

static inline uint32_t pio_i2c_put_instructions(const uint16_t *inst, uint8_t length, uint32_t timeout)    {
    uint32_t error;
    for(uint8_t i=0; i<length; i++){
        error=pio_i2c_put_or_timeout(inst[i], timeout);
        if(error) return error;
    }
    return 0;
}

uint32_t pio_i2c_start_timeout(uint32_t timeout) {
    const uint16_t start[]={
        1u << PIO_I2C_ICOUNT_LSB, // Escape code for 2 instruction sequence
        set_scl_sda_program_instructions[I2C_SC1_SD0],  // We are already in idle state, just pull SDA low
        set_scl_sda_program_instructions[I2C_SC0_SD0] // Also pull clock low so we can present data
    };
    return pio_i2c_put_instructions(start, count_of(start), timeout);    
}


uint32_t pio_i2c_stop_timeout(uint32_t timeout) {
    const uint16_t stop[]={
        2u << PIO_I2C_ICOUNT_LSB,
        set_scl_sda_program_instructions[I2C_SC0_SD0],  // SDA is unknown; pull it down
        set_scl_sda_program_instructions[I2C_SC1_SD0], // Release clock
        set_scl_sda_program_instructions[I2C_SC1_SD1] // Release SDA to return to idle state 
    };
    return pio_i2c_put_instructions(stop, count_of(stop), timeout);    
};

uint32_t pio_i2c_restart_timeout(uint32_t timeout) {
    const uint16_t restart[]={
        3u << PIO_I2C_ICOUNT_LSB,
        set_scl_sda_program_instructions[I2C_SC0_SD1],
        set_scl_sda_program_instructions[I2C_SC1_SD1],
        set_scl_sda_program_instructions[I2C_SC1_SD0],
        set_scl_sda_program_instructions[I2C_SC0_SD0]
    };
    return pio_i2c_put_instructions(restart, count_of(restart), timeout);    
}


void pio_i2c_resume_after_error() {
    pio_sm_drain_tx_fifo(hwi2c_pio, hwi2c_pio_state_machine);
    pio_sm_exec(hwi2c_pio, hwi2c_pio_state_machine, (hwi2c_pio->sm[hwi2c_pio_state_machine].execctrl & PIO_SM0_EXECCTRL_WRAP_BOTTOM_BITS) >> PIO_SM0_EXECCTRL_WRAP_BOTTOM_LSB);
    pio_interrupt_clear(hwi2c_pio, hwi2c_pio_state_machine);
    pio_i2c_rx_enable(false); 
    pio_i2c_stop_timeout(0xff);
    pio_i2c_stop_timeout(0xff);
    pio_i2c_stop_timeout(0xff);
    pio_sm_drain_tx_fifo(hwi2c_pio, hwi2c_pio_state_machine);
    pio_sm_exec(hwi2c_pio, hwi2c_pio_state_machine, (hwi2c_pio->sm[hwi2c_pio_state_machine].execctrl & PIO_SM0_EXECCTRL_WRAP_BOTTOM_BITS) >> PIO_SM0_EXECCTRL_WRAP_BOTTOM_LSB);
    pio_interrupt_clear(hwi2c_pio, hwi2c_pio_state_machine);
}

uint32_t pio_i2c_write_timeout(uint32_t data, uint32_t timeout){
    uint32_t error;

    error=pio_i2c_wait_idle_timeout(timeout);
    if(error) return error;
    error=pio_i2c_put_or_timeout( (data << 1)|(1u), timeout);
    if(error) return error;
    error=pio_i2c_wait_idle_timeout(timeout);
    return error;
}

uint32_t pio_i2c_read_timeout(uint32_t *data, bool ack, uint32_t timeout){
    uint32_t error;

    error=pio_i2c_wait_idle_timeout(timeout);
    if(error) return error;   
	while(!pio_sm_is_rx_fifo_empty(hwi2c_pio, hwi2c_pio_state_machine)){
        (void)pio_i2c_get();
    }
    error=pio_i2c_put16_or_timeout((0xffu << 1) | (ack?0:(1u << 9) | (1u << 0)), timeout);
    if(error) return error;
    uint32_t to=timeout;
    while(pio_sm_is_rx_fifo_empty(hwi2c_pio, hwi2c_pio_state_machine)){
        to--;
        if(!to) return 2;
    }
	(*data) = pio_i2c_get();
    error=pio_i2c_wait_idle_timeout(timeout); 
    return error;
}


uint32_t pio_i2c_write_blocking_timeout(uint8_t addr, uint8_t *txbuf, uint len, uint32_t timeout) 
{
    int err = 0;
    if(pio_i2c_start_timeout(timeout)) return 1;
    pio_i2c_rx_enable(false);
    if(pio_i2c_put16_or_timeout((addr << 1) | 1u, timeout)) return 1;
    uint32_t temp=timeout;
    while(len && !pio_i2c_check_error()){
        if(!pio_sm_is_tx_fifo_full(hwi2c_pio, hwi2c_pio_state_machine)) {
            --len;
            if(pio_i2c_put_or_timeout((*txbuf++ << PIO_I2C_DATA_LSB) | ((len == 0) << PIO_I2C_FINAL_LSB) | 1u, timeout)) return 1;
        }
        temp--;
        if(!temp) return 1;
    }
    if(pio_i2c_stop_timeout(timeout)) return 1;
    if(pio_i2c_wait_idle_timeout(timeout)) return 1;
    if(pio_i2c_check_error()) {
        //err = -1; 
        pio_i2c_resume_after_error();
        //pio_i2c_stop_timeout(pio, sm, timeout);
        return 1;
    }
    return 0;
}

uint32_t pio_i2c_read_blocking_timeout(uint8_t addr, uint8_t *rxbuf, uint len, uint32_t timeout) {
    int err = 0;
    if(pio_i2c_start_timeout(timeout)) return 1;
    pio_i2c_rx_enable(true); 
    while(!pio_sm_is_rx_fifo_empty(hwi2c_pio, hwi2c_pio_state_machine)){
        (void)pio_i2c_get(); 
    }
    if(pio_i2c_put16_or_timeout((addr << 1) | 3u, timeout)) return 1;
    uint32_t tx_remain = len; // Need to stuff 0xff bytes in to get clocks

    bool first = true;
    uint32_t temp=timeout;
    while((tx_remain || len) && !pio_i2c_check_error()){
        if(tx_remain && !pio_sm_is_tx_fifo_full(hwi2c_pio, hwi2c_pio_state_machine)) {
            --tx_remain;
            if(pio_i2c_put16_or_timeout( (0xffu << 1) | (tx_remain ? 0 : (1u << PIO_I2C_FINAL_LSB) | (1u << PIO_I2C_NAK_LSB)), timeout)) return 1;
            temp=timeout; //reset the counter
        }
        if(!pio_sm_is_rx_fifo_empty(hwi2c_pio, hwi2c_pio_state_machine)) {
            if(first) {
                // Ignore returned address byte
                (void)pio_i2c_get();
                first = false;
            }else {
                --len;
                *rxbuf++ = pio_i2c_get();
            }
        }

        temp--;
        if(!temp) return 1;
    }
    if(pio_i2c_stop_timeout(timeout)) return 1;
    if(pio_i2c_wait_idle_timeout(timeout)) return 1;
    if(pio_i2c_check_error()) {
        pio_i2c_resume_after_error();
        //pio_i2c_stop_timeout(pio, sm, timeout);
        return 1;
    }
    return 0;
}

//one full I2C transaction with error and timeout
uint32_t pio_i2c_transaction_blocking_timeout(uint8_t addr, uint8_t *txbuf, uint txlen, uint8_t *rxbuf, uint rxlen, uint32_t timeout) {
    if(pio_i2c_write_blocking_timeout(addr, txbuf, txlen, timeout)) return 1;
    if(rxlen>0){
        if(pio_i2c_read_blocking_timeout(addr,rxbuf, rxlen, timeout)) return 1;
    }
    return 0;
}

//////////////////////////////////////
// Full I2C packet functions (no timeout)

int pio_i2c_write_blocking(uint8_t addr, uint8_t *txbuf, uint len) {
    int err = 0;
    pio_i2c_start();
    pio_i2c_rx_enable( false);
    pio_i2c_put16((addr << 2) | 1u);
    while (len && !pio_i2c_check_error()) {
        if (!pio_sm_is_tx_fifo_full(hwi2c_pio, hwi2c_pio_state_machine)) {
            --len;
            pio_i2c_put_or_err( (*txbuf++ << PIO_I2C_DATA_LSB) | ((len == 0) << PIO_I2C_FINAL_LSB) | 1u);
        }
    }
    pio_i2c_stop();
    pio_i2c_wait_idle();
    if (pio_i2c_check_error()) {
        err = -1; 
        pio_i2c_resume_after_error();
        //pio_i2c_stop(pio, sm);
    }
    return err;
}

int pio_i2c_read_blocking(uint8_t addr, uint8_t *rxbuf, uint len) {
    int err = 0;
    pio_i2c_start();
    pio_i2c_rx_enable( true); 
    while (!pio_sm_is_rx_fifo_empty(hwi2c_pio, hwi2c_pio_state_machine))
        (void)pio_i2c_get();
    pio_i2c_put16((addr << 2) | 3u);
    uint32_t tx_remain = len; // Need to stuff 0xff bytes in to get clocks

    bool first = true;

    while ((tx_remain || len) && !pio_i2c_check_error()) {
        if (tx_remain && !pio_sm_is_tx_fifo_full(hwi2c_pio, hwi2c_pio_state_machine)) {
            --tx_remain;
            pio_i2c_put16( (0xffu << 1) | (tx_remain ? 0 : (1u << PIO_I2C_FINAL_LSB) | (1u << PIO_I2C_NAK_LSB)));
        }
        if (!pio_sm_is_rx_fifo_empty(hwi2c_pio, hwi2c_pio_state_machine)) {
            if (first) {
                // Ignore returned address byte
                (void)pio_i2c_get();
                first = false;
            }
            else {
                --len;
                *rxbuf++ = pio_i2c_get();
            }
        }
    }
    pio_i2c_stop();
    pio_i2c_wait_idle();
    if (pio_i2c_check_error()) {
        err = -1;
        pio_i2c_resume_after_error();
        //pio_i2c_stop(pio, sm);
    }
    return err;
}

/////////////////////////////
// Old functions


// If I2C is ok, block and push data. Otherwise fall straight through.
void pio_i2c_put_or_err(uint16_t data) {
    while (pio_sm_is_tx_fifo_full(hwi2c_pio, hwi2c_pio_state_machine))
        if (pio_i2c_check_error())
            return;
    if (pio_i2c_check_error())
        return;
    // some versions of GCC dislike this
#ifdef __GNUC__
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wstrict-aliasing"
#endif
    *(io_rw_16 *)&hwi2c_pio->txf[hwi2c_pio_state_machine] = data;
#ifdef __GNUC__
#pragma GCC diagnostic pop
#endif
}


void pio_i2c_put16(uint16_t data) {
    while (pio_sm_is_tx_fifo_full(hwi2c_pio, hwi2c_pio_state_machine));
    // some versions of GCC dislike this
#ifdef __GNUC__
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wstrict-aliasing"
#endif
    *(io_rw_16 *)&hwi2c_pio->txf[hwi2c_pio_state_machine] = data;
#ifdef __GNUC__
#pragma GCC diagnostic pop
#endif
}


void pio_i2c_start() {
    pio_i2c_put_or_err( 1u << PIO_I2C_ICOUNT_LSB); // Escape code for 2 instruction sequence
    pio_i2c_put_or_err(set_scl_sda_program_instructions[I2C_SC1_SD0]);    // We are already in idle state, just pull SDA low
    pio_i2c_put_or_err(set_scl_sda_program_instructions[I2C_SC0_SD0]);    // Also pull clock low so we can present data
}

void pio_i2c_stop() {
    pio_i2c_put_or_err(2u << PIO_I2C_ICOUNT_LSB);
    pio_i2c_put_or_err(set_scl_sda_program_instructions[I2C_SC0_SD0]);    // SDA is unknown; pull it down
    pio_i2c_put_or_err(set_scl_sda_program_instructions[I2C_SC1_SD0]);    // Release clock
    pio_i2c_put_or_err(set_scl_sda_program_instructions[I2C_SC1_SD1]);    // Release SDA to return to idle state
};


void pio_i2c_repstart() {
    pio_i2c_put_or_err(3u << PIO_I2C_ICOUNT_LSB);
    pio_i2c_put_or_err(set_scl_sda_program_instructions[I2C_SC0_SD1]);
    pio_i2c_put_or_err(set_scl_sda_program_instructions[I2C_SC1_SD1]);
    pio_i2c_put_or_err(set_scl_sda_program_instructions[I2C_SC1_SD0]);
    pio_i2c_put_or_err(set_scl_sda_program_instructions[I2C_SC0_SD0]);
}


void pio_i2c_wait_idle() {
    uint32_t timeout=10000;
    // Finished when TX runs dry or SM hits an IRQ
    hwi2c_pio->fdebug = 1u << (PIO_FDEBUG_TXSTALL_LSB + hwi2c_pio_state_machine);
    while (!(hwi2c_pio->fdebug & 1u << (PIO_FDEBUG_TXSTALL_LSB + hwi2c_pio_state_machine) || pio_i2c_check_error()) && timeout){
        tight_loop_contents();
        timeout--;
    }
}
