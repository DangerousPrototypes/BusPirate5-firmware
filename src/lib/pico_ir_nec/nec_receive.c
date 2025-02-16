/**
 * Copyright (c) 2021 mjcross
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

// SDK types and declarations
#include "pico/stdlib.h"
#include "pirate.h"
#include "hardware/pio.h"
#include "hardware/clocks.h"    // for clock_get_hz()
#include "mode/infrared-struct.h"
#include "nec_receive.h"

// import the assembled PIO state machine program
#include "nec_receive.pio.h"

#include "pio_config.h"

static struct _pio_config pio_config;

// Claim an unused state machine on the specified PIO and configure it
// to receive NEC IR frames on the given GPIO pin.
//
// Returns: the state machine number on success, otherwise -1
int nec_rx_init(uint pin_num) {

    // disable pull-up and pull-down on gpio pin
    gpio_disable_pulls(pin_num);

    //bool success = pio_claim_free_sm_and_add_program_for_gpio_range(&nec_receive_program, &pio_config.pio, &pio_config.sm, &pio_config.offset, pin_num, 1, true);
    //hard_assert(success);
    pio_config.pio = PIO_MODE_PIO;
    pio_config.sm = 2;
    pio_config.program = &nec_receive_program;
    pio_config.offset = pio_add_program(pio_config.pio, pio_config.program);
    /*if(!success) {
        return -1;
    }*/
    
    #ifdef BP_PIO_SHOW_ASSIGNMENT
    printf("PIO: pio=%d, sm=%d, offset=%d\r\n", PIO_NUM(pio_config.pio), pio_config.sm, pio_config.offset);
    #endif
 
    // configure and enable the state machine
    nec_receive_program_init(pio_config.pio, pio_config.sm, pio_config.offset, pin_num);

    return pio_config.sm;
}

void nec_rx_deinit(uint pin_num) {
    //pio_remove_program_and_unclaim_sm(&nec_receive_program, pio_config.pio, pio_config.sm, pio_config.offset);
    pio_remove_program(pio_config.pio, pio_config.program, pio_config.offset);
    pio_sm_clear_fifos(pio_config.pio, pio_config.sm);
    pio_sm_restart(pio_config.pio, pio_config.sm);
}

ir_rx_status_t nec_get_frame(uint32_t *rx_frame) {
    // display any frames in the receive FIFO
    if(pio_sm_is_rx_fifo_empty(pio_config.pio, pio_config.sm)) {
        return IR_RX_NO_FRAME;
    }

    (*rx_frame) = pio_sm_get(pio_config.pio, pio_config.sm);

    uint8_t rx_address, rx_data;
    if (nec_decode_frame(rx_frame, &rx_address, &rx_data)) {
        printf("\r\n(0x%08x) Address: %d (0x%02x) Command: %d (0x%02x)", (*rx_frame), rx_address, rx_address, rx_data, rx_data);
        return IR_RX_FRAME_OK;
    }else{
        //printf("\r\n(0x%08x) invalid frame", (*rx_frame));
        return IR_RX_FRAME_ERROR;
    }



}

void nec_rx_drain_fifo(void) {
    while(!pio_sm_is_rx_fifo_empty(pio_config.pio, pio_config.sm)) {
        pio_sm_get(pio_config.pio, pio_config.sm);
    }
}

// Validate a 32-bit frame and store the address and data at the locations
// provided.
//
// Returns: `true` if the frame was valid, otherwise `false`
bool nec_decode_frame(uint32_t *frame, uint8_t *p_address, uint8_t *p_data) {

    // access the frame data as four 8-bit fields
    //
    union {
        uint32_t raw;
        struct {
            uint8_t address;
            uint8_t inverted_address;
            uint8_t data;
            uint8_t inverted_data;
        };
    } f;

    f.raw = (*frame);

    // a valid (non-extended) 'NEC' frame should contain 8 bit
    // address, inverted address, data and inverted data
    if (f.address != (f.inverted_address ^ 0xff) ||
        f.data != (f.inverted_data ^ 0xff)) {
        return false;
    }

    // store the validated address and data
    *p_address = f.address;
    *p_data = f.data;

    return true;
}

