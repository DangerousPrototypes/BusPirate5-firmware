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
#include "lib/pico_ir_nec/nec_receive.h"
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

int rc5_tx_init(uint pin_num) {
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
}

void rc5_tx_deinit(uint pin_num) {
    //pio_remove_program_and_unclaim_sm(pio_config_tx.program, pio_config_tx.pio, pio_config_tx.sm, pio_config_tx.offset);
    pio_remove_program(pio_config_tx.pio, pio_config_tx.program, pio_config_tx.offset);
}

void rc5_send(uint32_t address, uint32_t data) {
    pio_sm_put_blocking(pio_config_tx.pio, pio_config_tx.sm, data);
}

nec_rx_status_t rc5_receive(uint32_t *rx_frame, uint8_t *rx_address, uint8_t *rx_data) {
    if (pio_sm_is_rx_fifo_empty(pio_config_rx.pio, pio_config_rx.sm)) {
        return NEC_RX_NO_FRAME;
    }
    (*rx_frame) = pio_sm_get_blocking(pio_config_rx.pio, pio_config_rx.sm);
    return NEC_RX_FRAME_OK;
}

void rc5_test(void) {
    pio_sm_set_enabled(pio_config_tx.pio, pio_config_tx.sm, false);
    pio_sm_put_blocking(pio_config_tx.pio, pio_config_tx.sm, 0);
    pio_sm_put_blocking(pio_config_tx.pio, pio_config_tx.sm, 0x0ff0a55a);
    pio_sm_put_blocking(pio_config_tx.pio, pio_config_tx.sm, 0x12345678);
    pio_sm_set_enabled(pio_config_tx.pio, pio_config_tx.sm, true);
    uint32_t timeout=0xffffff;
    for (int i = 0; i < 3; ++i){
        while(timeout){
            if(!pio_sm_is_rx_fifo_empty(pio_config_rx.pio, pio_config_rx.sm)){
                printf("%08x\r\n", pio_sm_get_blocking(pio_config_rx.pio, pio_config_rx.sm));
                timeout=0xffffff;
                break;
            }
            timeout--;
        }
    }
}

bool rc5_tx_wait_idle(void){
    return pio_sm_wait_idle(pio_config_tx.pio, pio_config_tx.sm, 0xfffff);
}