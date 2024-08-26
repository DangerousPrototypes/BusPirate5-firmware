/**
 * Copyright (c) 2021 mjcross
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include "pico/stdlib.h"
#include "hardware/pio.h"

// public API

int nec_rx_init(uint pin);
void nec_rx_deinit(void);
bool nec_decode_frame(uint32_t frame, uint8_t *p_address, uint8_t *p_data);
