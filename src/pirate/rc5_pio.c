/**
 * Copyright (c) 2020 Raspberry Pi (Trading) Ltd.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <stdio.h>
#include "pirate.h"
#include "pico/stdlib.h"
#include "hardware/pio.h"
#include "hardware/clocks.h"    // for clock_get_hz()
#include "mode/infrared-struct.h"
#include "rc5.pio.h"
#include "pirate/rc5_pio.h"

// Manchester serial transmit/receive example. This transmits and receives at
// 10 Mbps if sysclk is 120 MHz.

#include "pio_config.h"

static struct _pio_config pio_config_rx;
static struct _pio_config pio_config_tx;
static struct _pio_config pio_config_rc5_carrier;

int rc5_rx_init(uint pin_num) {
    // disable pull-up and pull-down on gpio pin
    gpio_disable_pulls(pin_num);

    pio_config_rx.pio = PIO_MODE_PIO;
    pio_config_rx.sm = 2;
    pio_config_rx.program = &manchester_rx_program;
    pio_config_rx.offset = pio_add_program(pio_config_rx.pio, pio_config_rx.program);

    #ifdef BP_PIO_SHOW_ASSIGNMENT
        printf("PIO: pio=%d, sm=%d, offset=%d\r\n", PIO_NUM(pio_config_rx.pio), pio_config_rx.sm, pio_config_rx.offset);
    #endif

    manchester_rx_program_init(pio_config_rx.pio, pio_config_rx.sm, pio_config_rx.offset, pin_num, 36000.0f/64.0f);
    return 0;
}

int rc5_tx_init(uint pin_num, uint32_t mod_freq) {
    // start rc5 carrier program
    pio_config_rc5_carrier.pio = PIO_MODE_PIO;
    pio_config_rc5_carrier.sm = 0;
    pio_config_rc5_carrier.program = &rc5_carrier_program;
    pio_config_rc5_carrier.offset = pio_add_program(pio_config_rc5_carrier.pio, pio_config_rc5_carrier.program);

    #ifdef BP_PIO_SHOW_ASSIGNMENT
        printf("PIO: pio=%d, sm=%d, offset=%d\r\n", PIO_NUM(pio_config_rc5_carrier.pio), pio_config_rc5_carrier.sm, pio_config_rc5_carrier.offset);
    #endif

    rc5_carrier_program_init(pio_config_rc5_carrier.pio, pio_config_rc5_carrier.sm, pio_config_rc5_carrier.offset, pin_num, 36000.0f);
    //start rc5 control program
    pio_config_tx.pio = PIO_MODE_PIO;
    pio_config_tx.sm = 1;
    pio_config_tx.program = &manchester_tx_program;
    pio_config_tx.offset = pio_add_program(pio_config_tx.pio, pio_config_tx.program);

    #ifdef BP_PIO_SHOW_ASSIGNMENT
        printf("PIO: pio=%d, sm=%d, offset=%d\r\n", PIO_NUM(pio_config_tx.pio), pio_config_tx.sm, pio_config_tx.offset);
    #endif

    manchester_tx_program_init(pio_config_tx.pio, pio_config_tx.sm, pio_config_tx.offset, 36000.0f/64.0f);
    return 0;
}

void rc5_rx_deinit(uint pin_num) {
    //pio_remove_program_and_unclaim_sm(pio_config_tx.program, pio_config_rx.pio, pio_config_rx.sm, pio_config_rx.offset);
    pio_remove_program(pio_config_rx.pio, pio_config_rx.program, pio_config_rx.offset);
    gpio_set_inover(1u << pin_num, GPIO_OVERRIDE_NORMAL);
    pio_sm_clear_fifos(pio_config_rx.pio, pio_config_rx.sm);
    pio_sm_restart(pio_config_rx.pio, pio_config_rx.sm);
}

void rc5_tx_deinit(uint pin_num) {
    //pio_remove_program_and_unclaim_sm(pio_config_tx.program, pio_config_tx.pio, pio_config_tx.sm, pio_config_tx.offset);
    pio_remove_program(pio_config_tx.pio, pio_config_tx.program, pio_config_tx.offset);
    pio_remove_program(pio_config_rc5_carrier.pio, pio_config_rc5_carrier.program, pio_config_rc5_carrier.offset);
    pio_sm_clear_fifos(pio_config_tx.pio, pio_config_tx.sm);
    pio_sm_clear_fifos(pio_config_rc5_carrier.pio, pio_config_rc5_carrier.sm);
    pio_sm_restart(pio_config_tx.pio, pio_config_tx.sm);
    pio_sm_restart(pio_config_rc5_carrier.pio, pio_config_rc5_carrier.sm);
}

void rc5_send(uint32_t *data) {
    //RC5 frame format: S1 S2 T A4 A3 A2 A1 A0 C5 C4 C3 C2 C1 C0
    // 14bits total. S1=1, S2=1, T=toggle bit, A=address (5), C=command (6)
    // for Extended RC5 S2 is command bit 7. S1 has to be inverted to be backwards compatible
    (*data) =((*data) | ((1u<<13)|(1u<<12))); //set S1 and S2
    //need to align the 14 bits to the left of the 32 bit word
    uint32_t rc5_frame = (*data) << 18;
    pio_sm_put_blocking(pio_config_tx.pio, pio_config_tx.sm, rc5_frame);
}

ir_rx_status_t rc5_receive(uint32_t *rx_frame) {
    if (pio_sm_is_rx_fifo_empty(pio_config_rx.pio, pio_config_rx.sm)) {
        return IR_RX_NO_FRAME;
    }
    // we get a 32 bit word with the raw pin samples
    uint32_t manchester_frame = pio_sm_get_blocking(pio_config_rx.pio, pio_config_rx.sm);
    // Manchester decoding
    // we need to decode the manchester frame to get the raw RC5 frame
    #define RC5_0 0b01 //these are inverted because the sensor pulls low for active IR signal
    #define RC5_1 0b10
    uint16_t rc5_frame = 0;
    for (int i = 0; i < 14; i++) {
        rc5_frame <<= 1;
        uint8_t mc_bit = (manchester_frame >> (26 - (2 * i))) & 0b11; // Extract 2 bits
        if (mc_bit == RC5_1) {
            rc5_frame |= 1; // Logical '1'
        } else if (mc_bit != RC5_0) {
            // Invalid Manchester encoding
            //printf("\r\nInvalid Manchester encoding (0x%08x)", manchester_frame);
            return IR_RX_FRAME_ERROR; // Indicate an error
        }
    }
    (*rx_frame) = rc5_frame;
    //TODO: seperate these out to we can use the function without the printf
    printf("\r\n(0x%04x) SB1:%d SB2:%d Toggle:%d Address: %d (0x%02x) Command: %d (0x%02x)", 
    rc5_frame, (rc5_frame >> 13) & 1, (rc5_frame >> 12) & 1, (rc5_frame >> 11) & 1, 
    (rc5_frame >> 6) & 0x1f, (rc5_frame >> 6) & 0x1f, rc5_frame & 0x3f, rc5_frame & 0x3f);
    return IR_RX_FRAME_OK;
}

void rc5_drain_fifo(void) {
    while (!pio_sm_is_rx_fifo_empty(pio_config_rx.pio, pio_config_rx.sm)) {
        pio_sm_get_blocking(pio_config_rx.pio, pio_config_rx.sm);
    }
}

bool rc5_tx_wait_idle(void){
    return pio_sm_wait_idle(pio_config_tx.pio, pio_config_tx.sm, 0xfffff);
}