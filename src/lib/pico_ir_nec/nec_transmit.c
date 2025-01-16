/**
 * Copyright (c) 2021 mjcross
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */


// SDK types and declarations
//
#include "pico/stdlib.h"
#include "hardware/pio.h"
#include "hardware/clocks.h"    // for clock_get_hz()
#include "nec_transmit.h"
#include "pirate.h"

// import the assembled PIO state machine programs
#include "nec_carrier_burst.pio.h"
#include "nec_carrier_control.pio.h"

#include "pio_config.h"

static struct _pio_config pio_config_burst;
static struct _pio_config pio_config_control;

// Claim an unused state machine on the specified PIO and configure it
// to transmit NEC IR frames on the specificied GPIO pin.
//
// Returns: on success, the number of the carrier_control state machine
// otherwise -1
int nec_tx_init(uint pin_num, uint32_t mod_freq) {

    // install the carrier_burst program in the PIO shared instruction space
    //bool success = pio_claim_free_sm_and_add_program_for_gpio_range(&nec_carrier_burst_program, &pio_config_burst.pio, &pio_config_burst.sm, &pio_config_burst.offset, pin_num, 1, true);
    //hard_assert(success);
    /*if(!success) {
        printf("Failed to claim free state machine for carrier_burst program\r\n");
        return -1;
    }*/
    pio_config_burst.pio = PIO_MODE_PIO;
    pio_config_burst.sm = 0;
    pio_config_burst.program = &nec_carrier_burst_program;
    pio_config_burst.offset = pio_add_program(pio_config_burst.pio, pio_config_burst.program);
    #ifdef BP_PIO_SHOW_ASSIGNMENT
    printf("PIO: pio=%d, sm=%d, offset=%d\r\n", PIO_NUM(pio_config_burst.pio), pio_config_burst.sm, pio_config_burst.offset);
    #endif
    // configure and enable the state machine
    nec_carrier_burst_program_init(pio_config_burst.pio,
                                   pio_config_burst.sm,
                                   pio_config_burst.offset,
                                   pin_num,
                                   38.222e3);                   // 38.222 kHz carrier

    // install the carrier_control program in the PIO shared instruction space
    /*success = pio_claim_free_sm_and_add_program_for_gpio_range(&nec_carrier_control_program, &pio_config_control.pio, &pio_config_control.sm, &pio_config_control.offset, pin_num, 1, true);
    hard_assert(success);
    if(!success) {
        return -1;
    } */   
    pio_config_control.pio = PIO_MODE_PIO;
    pio_config_control.sm = 1;
    pio_config_control.program = &nec_carrier_control_program;
    pio_config_control.offset = pio_add_program(pio_config_control.pio, pio_config_control.program);
    #ifdef BP_PIO_SHOW_ASSIGNMENT    
    printf("PIO: pio=%d, sm=%d, offset=%d\r\n", PIO_NUM(pio_config_control.pio), pio_config_control.sm, pio_config_control.offset);    
    #endif
    // configure and enable the state machine
    nec_carrier_control_program_init(pio_config_control.pio,
                                     pio_config_control.sm,
                                     pio_config_control.offset,
                                     2 * (1 / 562.5e-6f),        // 2 ticks per 562.5us carrier burst
                                     32);                       // 32 bits per frame

    return pio_config_control.sm;
}

void nec_tx_deinit(uint pin_num) {
    //pio_remove_program_and_unclaim_sm(&nec_carrier_burst_program, pio_config_burst.pio, pio_config_burst.sm, pio_config_burst.offset);
    //pio_remove_program_and_unclaim_sm(&nec_carrier_control_program, pio_config_control.pio, pio_config_control.sm, pio_config_control.offset);
    pio_remove_program(pio_config_burst.pio, pio_config_burst.program, pio_config_burst.offset);
    pio_remove_program(pio_config_control.pio, pio_config_control.program, pio_config_control.offset);
    pio_sm_clear_fifos(pio_config_burst.pio, pio_config_burst.sm);
    pio_sm_clear_fifos(pio_config_control.pio, pio_config_control.sm);
    pio_sm_restart(pio_config_burst.pio, pio_config_burst.sm);
    pio_sm_restart(pio_config_control.pio, pio_config_control.sm);
}


// Create a frame in `NEC` format from the provided 8-bit address and data
//
// Returns: a 32-bit encoded frame
uint32_t nec_encode_frame(uint8_t address, uint8_t data) {
    // a normal 32-bit frame is encoded as address, inverted address, data, inverse data,
    return address | (address ^ 0xff) << 8 | data << 16 | (data ^ 0xff) << 24;
}

void nec_send_frame(uint32_t tx_frame) {
    // send the frame
    pio_sm_put(pio_config_control.pio, pio_config_control.sm, tx_frame);
}

void nec_write(uint32_t *data) {
    // transmit and receive frames
    uint8_t tx_address = (*data) >> 8;
    uint8_t tx_data = (*data) & 0xff;
    // create a 32-bit frame and add it to the transmit FIFO
    nec_send_frame(nec_encode_frame((uint8_t)tx_address, (uint8_t)tx_data));
}

bool nec_tx_wait_idle(void){
    return pio_sm_wait_idle(pio_config_control.pio, pio_config_control.sm, 0xfffff);
}
