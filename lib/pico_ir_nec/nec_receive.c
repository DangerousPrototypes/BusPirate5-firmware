/**
 * Copyright (c) 2021 mjcross
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

// SDK types and declarations
#include "pico/stdlib.h"
#include "hardware/pio.h"
#include "hardware/clocks.h"    // for clock_get_hz()

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

    bool success = pio_claim_free_sm_and_add_program_for_gpio_range(&nec_receive_program, &pio_config.pio, &pio_config.sm, &pio_config.offset, pin_num, 1, true);
    hard_assert(success);
    if(!success) {
        return -1;
    }
    printf("PIO: pio=%d, sm=%d, offset=%d\r\n", PIO_NUM(pio_config.pio), pio_config.sm, pio_config.offset);
 
    // configure and enable the state machine
    nec_receive_program_init(pio_config.pio, pio_config.sm, pio_config.offset, pin_num);

    return sm;
}

void nec_rx_deinit(void) {
    pio_remove_program_and_unclaim_sm(&nec_receive_program, pio_config.pio, pio_config.sm, pio_config.offset);
}


// Validate a 32-bit frame and store the address and data at the locations
// provided.
//
// Returns: `true` if the frame was valid, otherwise `false`
bool nec_decode_frame(uint32_t frame, uint8_t *p_address, uint8_t *p_data) {

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

    f.raw = frame;

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
