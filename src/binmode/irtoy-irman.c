#include <pico/stdlib.h>
#include <string.h>
#include "hardware/clocks.h"
#include "pirate.h"
#include "queue.h"
#include "usb_rx.h"
#include "usb_tx.h"
#include "system_config.h"
// #include "bytecode.h" //needed because modes.h has some functions that use it TODO: move all the opt args and
// bytecode stuff to a single helper file #include "command_struct.h" //needed for same reason as bytecode and needs same fix
// #include "modes.h"
#include "binmode/binmodes.h"
#include "tusb.h"
#include "ui/ui_term.h"
#include "mode/infrared-struct.h"
#include "pirate/rc5_pio.h"
#include "lib/pico_ir_nec/nec_receive.h"
#include "pirate/bio.h"
#include "binmode/binio_helpers.h"

#define MAX_UART_PKT 64
#define CDC_INTF 1

// binmode name to display
const char irtoy_irman_name[] = "IRMAN IR decoder (LIRC, etc)";

static const char pin_labels[][5]={
    "38k",
};

// binmode setup on mode start
void irtoy_irman_setup(void) {
    system_bio_update_purpose_and_label(true, BIO5, BP_PIN_IO, pin_labels[0]);
    system_config.binmode_usb_rx_queue_enable = true;
    system_config.binmode_usb_tx_queue_enable = true;
    rc5_rx_init(bio2bufiopin[BIO5]);
}

// binmode cleanup on exit
void irtoy_irman_cleanup(void) {
    system_bio_update_purpose_and_label(false, BIO5, BP_PIN_IO, pin_labels[0]);
    rc5_rx_deinit(bio2bufiopin[BIO5]);
    system_config.binmode_usb_rx_queue_enable = true;
    system_config.binmode_usb_tx_queue_enable = true;
}

#define HARDWARE_VERSION 3
#define FIRMWARE_VERSION_H '2'
#define FIRMWARE_VERSION_L '0'


/*
// Send IRMAN formatted receive packet
//final USB packet is:
//byte 1 bits 7-5 (don't care)
//byte 1 bits 4-0 (5 address bits)
//byte 2 bit 7 (don't care)
//byte 2 bit 6 (RC5x/start bit 2, not inversed)
//byte 2 bits 5-0 (RC5 6 bit command)
//byte 3-6 (unused)
//
//first byte of USB data is the RC5 address (lower 5 bits of first byte)
//loop through DecoderBuffer[] and shift the 5bit address into the USB output buffer
//5 address bits, 5-0, MSB first
*/
void irtoy_irman_rx(void) {
    static char i;
    char temp;
    //todo: check if PIO has input from IR and decode it
    uint32_t rx_frame;
    if (rc5_receive(&rx_frame)== IR_RX_FRAME_OK) {
        //sb2 (rc5_frame >> 12) & 1
        //toggle (rc5_frame >> 11) & 1
        //address (rc5_frame >> 6) & 0x1f
        //command rc5_frame & 0x3f

        char irman_frame[6];
        memset(irman_frame, 0x00, sizeof(irman_frame));
        //byte 1 bits 7-5 (don't care)
        //byte 1 bits 4-0 (5 address bits)
        irman_frame[0] = (rx_frame >> 6) & 0x1f;
        //byte 2 bit 7 (don't care)
        //byte 2 bit 6 (RC5x/start bit 2, not inversed)
        //byte 2 bits 5-0 (RC5 6 bit command)
        irman_frame[1] = ( ((rx_frame >> 12) & 1)<<6) | (rx_frame & 0x3f);
        //byte 3-6 (unused)

        script_send(irman_frame, 6);
    }
}

void irtoy_irman_service(void){
    static const char ok[2]="OK";
    static const char version[4] = {'V', (HARDWARE_VERSION + 0x30), FIRMWARE_VERSION_H, FIRMWARE_VERSION_L};
    if (!tud_cdc_n_connected(CDC_INTF)) {
        rc5_drain_fifo();
        return;
    }

    //check PIO for incomming IR data
    //manchester decode, validate, send to USB 
    irtoy_irman_rx();  

    char c;
    if (!bin_rx_fifo_try_get(&c)){
        return;
    }

    switch (c) {
        case 'r': //IRMAN decoder mode
        case 'R':
            script_send(ok, 2);
            break;
        case 'V':
        case 'v':// Acquire Version
            script_send(version, 4);
            break;            
        default:
            break;
    }

}
