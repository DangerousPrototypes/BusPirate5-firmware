#include <stdio.h>
#include "pico/stdlib.h"
#include <stdint.h>
#include "hardware/uart.h"
#include "pirate.h"
#include "system_config.h"
#include "opt_args.h"
#include "ui/ui_term.h"
#include "ui/ui_help.h"
#include "bytecode.h"
#include "mode/hwuart.h"
#include "pirate/button.h"
#include "usb_rx.h"
#include "usb_tx.h"
#include "hwuart.pio.h"
#include "pirate/bio.h"

static PIO pio = M_UART_PIO;
static uint sm = M_UART_PIO_SM;
static uint pio_loaded_offset;

void hwuart_pio_init(uint32_t baud){
    pio_loaded_offset = pio_add_program(pio, &uart_rx_program);
    uart_rx_program_init(pio, sm, pio_loaded_offset, bio2bufiopin[M_UART_RXTX], baud);

    pio_remove_program(pio, &uart_rx_program, pio_loaded_offset);
}

void hwuart_pio_deinit(void){
    pio_remove_program(pio, &uart_rx_program, pio_loaded_offset);
}

bool hwuart_pio_read(uint32_t *raw, uint8_t *cooked){
    if(pio_sm_is_rx_fifo_empty(pio, sm)){
        return false;
    }
    // 8-bit read from the uppermost byte of the FIFO, as data is left-justified
    (*raw) = pio->rxf[sm];
    (*cooked) = (uint8_t)((*raw) >> 23); //MSB is the parity bit...
    return true;
}