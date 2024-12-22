/**
 * Copyright (c) 2021 mjcross
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include "pico/stdlib.h"
#include "hardware/pio.h"

// public API
int nec_rx_init(uint pin);
void nec_rx_deinit(uint pin_num);
bool nec_decode_frame(uint32_t *frame, uint8_t *p_address, uint8_t *p_data);
ir_rx_status_t nec_get_frame(uint32_t *rx_frame) ;
void nec_rx_drain_fifo(void) ;
