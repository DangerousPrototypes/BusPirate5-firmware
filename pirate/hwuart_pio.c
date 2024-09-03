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
#include "pio_config.h"

static struct _pio_config pio_config_rx;
static struct _pio_config pio_config_tx;

uint8_t hwuart_data_bits;
uint8_t hwuart_parity;
uint8_t hwuart_stop_bits;


void hwuart_pio_init(uint8_t data_bits, uint8_t parity, uint8_t stop_bits, uint32_t baud){
    hwuart_data_bits = data_bits;
    hwuart_parity = parity;
    hwuart_stop_bits = stop_bits;

    //calculate the number of bits to send, minus the final stop bit which is handled in hardware
    uint8_t bits = data_bits;
    if(hwuart_parity != UART_PARITY_NONE){
        bits++;
    }
    if(hwuart_stop_bits == 2){
        bits++;
    }
    bits--;
    
    //bool success = pio_claim_free_sm_and_add_program_for_gpio_range(&uart_rx_program, &pio_config_rx.pio, &pio_config_rx.sm, &pio_config_rx.offset, bio2bufiopin[M_UART_RXTX], 1, true);
    //hard_assert(success);
    pio_config_rx.pio = PIO_MODE_PIO;
    pio_config_rx.sm = 0;
    pio_config_rx.program = &uart_rx_program;
    pio_config_rx.offset = pio_add_program(pio_config_rx.pio, pio_config_rx.program);
    #ifdef BP_PIO_SHOW_ASSIGNMENT
    printf("PIO: pio=%d, sm=%d, offset=%d\r\n", PIO_NUM(pio_config_rx.pio), pio_config_rx.sm, pio_config_rx.offset);
    #endif    
    uart_rx_program_init(pio_config_rx.pio, pio_config_rx.sm, pio_config_rx.offset, bio2bufiopin[M_UART_RXTX], bits, baud);

    //success = pio_claim_free_sm_and_add_program_for_gpio_range(&uart_tx_program, &pio_config_tx.pio, &pio_config_tx.sm, &pio_config_tx.offset, bio2bufiopin[M_UART_RXTX], 1, true);
    //hard_assert(success);
    pio_config_tx.pio = PIO_MODE_PIO;
    pio_config_tx.sm = 1;
    pio_config_tx.program = &uart_tx_program;
    pio_config_tx.offset = pio_add_program(pio_config_tx.pio, pio_config_tx.program);
    #ifdef BP_PIO_SHOW_ASSIGNMENT
    printf("PIO: pio=%d, sm=%d, offset=%d\r\n", PIO_NUM(pio_config_tx.pio), pio_config_tx.sm, pio_config_tx.offset);
    #endif    
    uart_tx_program_init(pio_config_tx.pio, pio_config_tx.sm, pio_config_tx.offset, bio2bufdirpin[M_UART_RXTX], bits, baud);

}

void hwuart_pio_deinit(void){
    //pio_remove_program_and_unclaim_sm(&uart_rx_program, pio_config_rx.pio, pio_config_rx.sm, pio_config_rx.offset);
    //pio_remove_program_and_unclaim_sm(&uart_tx_program, pio_config_tx.pio, pio_config_tx.sm, pio_config_tx.offset);
    pio_remove_program(pio_config_rx.pio, pio_config_rx.program, pio_config_rx.offset);
    pio_remove_program(pio_config_tx.pio, pio_config_tx.program, pio_config_tx.offset);
}

bool hwuart_pio_read(uint32_t *raw, uint8_t *cooked){
    if(pio_sm_is_rx_fifo_empty(pio_config_rx.pio, pio_config_rx.sm)){
        return false;
    }
    // 8-bit read from the uppermost byte of the FIFO, as data is left-justified
    (*raw) = pio_config_rx.pio->rxf[pio_config_rx.sm];
    //TODO: change this based on UART settings
    //Detect parity error?
    (*cooked) = (uint8_t)((*raw) >> 22); //MSB is the parity bit...
    return true;
}

static inline void pio_hwuart_wait_idle(PIO pio, uint sm) {
    pio->fdebug = 1u << (PIO_FDEBUG_TXSTALL_LSB + sm);
    while(!(pio->fdebug & 1u << (PIO_FDEBUG_TXSTALL_LSB + sm)));  
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
    // PIO shifts data to right (LSB of value first)
    // in the FIFO the data should be stopbit2|parity bit|data bits
    // one stop bit is handled in the state machine
    uint8_t bits=hwuart_data_bits;
    if(hwuart_parity==UART_PARITY_ODD){ //add a parity bit
        if(!getParity(data)) data|=(0b1<<bits);
        bits++;
        //printf("even ");
    }else if(hwuart_parity==UART_PARITY_EVEN){
        if(getParity(data)) data|=(0b1<<bits);
        bits++;
        //printf("odd ");
    }
    if(hwuart_stop_bits==2){
        data|= (0b1<<bits); //add a stop bit in the case of 2 stop bits 
        //printf("2 stop ");
    }
    //printf("stop bit: %d parity bit: %d data bits: %d\n", hwuart_stop_bits, hwuart_parity, hwuart_data_bits);
    pio_sm_set_enabled(pio_config_rx.pio, pio_config_rx.sm, false); //pause the RX state machine? maybe just discard the byte?
    pio_sm_put_blocking(pio_config_tx.pio, pio_config_tx.sm, data);
    pio_hwuart_wait_idle(pio_config_tx.pio, pio_config_tx.sm);//wait for the TX state machine to finish
    pio_sm_set_enabled(pio_config_rx.pio, pio_config_rx.sm, true); //enable the RX state machine again
}