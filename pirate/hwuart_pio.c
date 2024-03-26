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
static uint pio_loaded_offset_rx;
static uint pio_loaded_offset_tx;

void hwuart_pio_init(uint8_t data_bits, uint8_t parity, uint8_t stop_bits, uint32_t baud){
    
    uint8_t bits = data_bits;
    if(parity != 'n'){
        bits++;
    }
    if(stop_bits == 2){
        bits++;
    }
    bits--;
    
    pio_loaded_offset_rx = pio_add_program(pio, &uart_rx_program);
    uart_rx_program_init(pio, sm, pio_loaded_offset_rx, bio2bufiopin[M_UART_RXTX], bits, baud);
    pio_loaded_offset_tx = pio_add_program(pio, &uart_tx_program);
    uart_tx_program_init(pio, sm-1, pio_loaded_offset_tx, bio2bufdirpin[M_UART_RXTX], bits, baud);

}

void hwuart_pio_deinit(void){
    pio_remove_program(pio, &uart_rx_program, pio_loaded_offset_rx);
     pio_remove_program(pio, &uart_tx_program, pio_loaded_offset_tx);
}

bool hwuart_pio_read(uint32_t *raw, uint8_t *cooked){
    if(pio_sm_is_rx_fifo_empty(pio, sm)){
        return false;
    }
    // 8-bit read from the uppermost byte of the FIFO, as data is left-justified
    (*raw) = pio->rxf[sm];
    //TODO: change this based on UART settings
    //Detect parity error?
    (*cooked) = (uint8_t)((*raw) >> 22); //MSB is the parity bit...
    return true;
}

bool getParity(unsigned int n){
    bool parity = 0;
    while (n){
        parity = !parity;
        n     = n & (n - 1);
    }     
    return parity;
}

void hwuart_pio_write(uint32_t data){
    //8 data bits, 1 parity bit, 2 stop bits = 11 bits
    // one stop bit is handled in the state machine
    data = data << 1 | getParity(data);
    data = data << 1 | 0b1; //add a stop bit in the case of 2 stop bits 
    pio_sm_put_blocking(pio, sm-1, data);
}