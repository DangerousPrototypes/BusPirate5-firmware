/**
 * Copyright (c) 2021 mjcross
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include "pico/stdlib.h"
#include "hardware/pio.h"

// public API

int nec_tx_init(uint pin);
void nec_tx_deinit(void);
uint32_t nec_encode_frame(uint8_t address, uint8_t data);
void nec_send_frame(uint32_t tx_frame);
bool nec_tx_wait_idle(void);
