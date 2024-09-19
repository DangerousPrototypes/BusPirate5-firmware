#pragma once

// BUGBUG - rename these functions to have `bp_usb_cdc_` prefix
void rx_fifo_init(void);
void rx_uart_init_irq(void);
void rx_usb_init(void);
void rx_fifo_service(void);

void rx_fifo_add(char *c);
bool rx_fifo_try_get(char *c);
void rx_fifo_get_blocking(char *c);
bool rx_fifo_try_peek(char *c);
void rx_fifo_peek_blocking(char *c);

void bin_rx_fifo_add(char *c);
void bin_rx_fifo_get_blocking(char *c);
void bin_rx_fifo_available_bytes(uint16_t *cnt);
bool bin_rx_fifo_try_get(char *c);


