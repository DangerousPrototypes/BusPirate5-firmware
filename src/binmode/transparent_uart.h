/**
 * @file transparent_uart.h
 * @brief Host-configured USB CDC to hardware UART binary mode.
 *
 * Copyright (c) 2026 DereIBims. MIT License.
 */

#ifndef TRANSPARENT_UART_H
#define TRANSPARENT_UART_H

#include <stdbool.h>
#include <stdint.h>

extern const char transparent_uart_name[];

void transparent_uart_setup(void);
void transparent_uart_service(void);
void transparent_uart_cleanup(void);

/** TinyUSB CDC event entry points. Non-binary CDC interfaces are ignored. */
void transparent_uart_line_coding_changed(
    uint8_t itf, uint32_t bit_rate, uint8_t stop_bits, uint8_t parity, uint8_t data_bits);
void transparent_uart_line_state_changed(uint8_t itf, bool dtr, bool rts);

#endif // TRANSPARENT_UART_H
