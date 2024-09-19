#include <stdio.h>
#include "pico/stdlib.h"
#include "pirate.h"
#include "pirate/bio.h"
#include "hardware/uart.h"
#include "hardware/irq.h"
#include "system_config.h"
#include "debug.h"
#include "usb_rx.h"

struct _debug_uart debug_uart[]=
{
    {BP_DEBUG_UART_0,BP_DEBUG_UART_0_RX,BP_DEBUG_UART_0_TX, UART0_IRQ },
    {BP_DEBUG_UART_1,BP_DEBUG_UART_1_RX,BP_DEBUG_UART_1_TX, UART1_IRQ }    
};

static const char debug_pin_labels[][5]={
    "DBTX",
    "DBRX",
    "DTTX",
    "DTRX"
};

// easy to remember debug function for developers...
void debug_tx(char c)
{
    uart_putc(debug_uart[system_config.debug_uart_number].uart, c);
}
//TODO: currently blocking function, make nonblocking
bool debug_rx(char *c)
{
    if(uart_is_readable(debug_uart[system_config.debug_uart_number].uart))
    {
        *c=uart_getc(debug_uart[system_config.debug_uart_number].uart);
        return true;
    }
    return false;
}

void debug_uart_init(int uart_number, bool dbrx, bool dbtx, bool terminal_label) // BUGBUG -- no error return path!
{
    // BUGBUG -- rewrite for improved safety
    // First, claim the pins (because this could theoretically fail)
    // If only one pin was claimed, then release the claimed pin.
    //     Alternatives:
    //     * hard-lock the BP5 (not terrible, since only called at start of core1_entry()...)
    //     * make this function return a value so can return error code
    // Else both pins were claimed, so do the actual initialization, etc.

    // Initialise debug UART
    uart_init(debug_uart[uart_number].uart, 115200);
    uart_set_fifo_enabled(debug_uart[uart_number].uart, true); //might set to false    

    if(dbtx)
    {
        // manually setup the buffer direction control pin to output 
        // becuase debug pins are ignored in debug mode
        bio_buf_pin_init(debug_uart[uart_number].tx_pin);
        // set the buffer direction
        bio_buf_output(debug_uart[uart_number].tx_pin); //output TX
        // Set the GPIO pin mux to the UART
        bio_set_function(debug_uart[uart_number].tx_pin, GPIO_FUNC_UART);
        //claim and label pin // BUGBUG -- Should claim and label pin BEFORE modifying the pin direction, etc.
        system_bio_claim(true, debug_uart[uart_number].tx_pin, BP_PIN_DEBUG, debug_pin_labels[(terminal_label *2) + 0]); // BUGBUG -- unchecked error return value
    }

    if(dbrx)
    {
        // manually setup the buffer direction control pin to output 
        // becuase debug pins are ignored in debug mode
        bio_buf_pin_init(debug_uart[uart_number].rx_pin);
        // set the buffer direction
        bio_buf_input(debug_uart[uart_number].rx_pin); //input RX
        // Set the GPIO pin mux to the UART
        bio_set_function(debug_uart[uart_number].rx_pin, GPIO_FUNC_UART);
        //claim and label pin // BUGBUG -- Should claim and label pin BEFORE modifying the pin direction, etc.
        system_bio_claim(true, debug_uart[uart_number].rx_pin, BP_PIN_DEBUG, debug_pin_labels[(terminal_label *2) + 1]); // BUGBUG -- unchecked error return value
    }
}

// BUGBUG -- remove unused function, rx_uart_disable()
void rx_uart_disable(void) // BUGBUG -- does not actually perform any cleanup!
{
    // BUGBUG -- should call uart_set_fifo_enabled(uart, false)
    // BUGBUG -- should call uart_deinit(uart)
    // BUGBUG -- should set pins to HiZ state ???
    // BUGBUG -- should set pin functions to ... ??? GPIO_FUNC_SIO ???
    // BUGBUG -- should call system_bio_claim(false, rx_pin, BP_PIN_IO, NULL);
    // BUGBUG -- should call system_bio_claim(false, tx_pin, BP_PIN_IO, NULL);
    //cleanup uart hardware and pin
    
}