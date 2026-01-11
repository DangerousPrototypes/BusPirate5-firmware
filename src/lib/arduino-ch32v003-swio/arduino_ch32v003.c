#include <stdio.h>
#include "pico/stdlib.h"
#include <stdint.h>
#include "pirate.h"
#include "swio.h"
#include "pirate/bio.h"
#include "swio.h"
#include "pirate/psu.h"
#include "system_config.h"
#include "queue.h"
#include "usb_rx.h"
#include "usb_tx.h"
#include "tusb.h"
#include "lib/picorvd/picoswio.h"

void target_power(int x){
    if (x)
        psu_enable(5.0f,0.0f,true, 100);
    else
        psu_disable();
}

#define PROTOCOL_START     '!'
#define PROTOCOL_ACK       '+'
#define PROTOCOL_TEST      '?'
#define PROTOCOL_POWER_ON  'p'
#define PROTOCOL_POWER_OFF 'P'
#define PROTOCOL_WRITE_REG 'w'
#define PROTOCOL_READ_REG  'r'

const char arduino_ch32v003_name[]="Arduino CH32V003 SWIO";

void arduino_ch32v003_cleanup(void){
    ch32vswio_cleanup();
}

void arduino_ch32v003(void) {
    uint8_t reg;
    uint32_t val;

    if(!tud_cdc_n_connected(1)) return;  

    /*if(!system_config.rts){
        //RTS is high, simulate arduino reset
        //uart_init();
        //swio_init();
        
        bin_tx_fifo_put(PROTOCOL_START);
        system_config.rts=1;
    }*/

    char c;

    if(bin_rx_fifo_try_get(&c)) { //co-op multitask
        switch (c) {
            case PROTOCOL_TEST:
                ch32vswio_reset(bio2bufiopin[BIO0], bio2bufdirpin[BIO0]);
                bin_tx_fifo_put(PROTOCOL_ACK);
                break;
            case PROTOCOL_POWER_ON:
                target_power(1);
                bin_tx_fifo_put(PROTOCOL_ACK);
                break;
            case PROTOCOL_POWER_OFF:
                target_power(0);
                bin_tx_fifo_put(PROTOCOL_ACK);
                break;
            case PROTOCOL_WRITE_REG:
                //fread(&reg, sizeof(uint8_t), 1, uart);
                bin_rx_fifo_get_blocking(&reg);
                //fread(&val, sizeof(uint32_t), 1, uart);
                for(uint8_t i=0;i<4;i++){
                    val=(val>>8);
                    bin_rx_fifo_get_blocking(&c);
                    val|=(c<<24);
                }
                //swio_write_reg(reg, val);
                ch32vswio_put(reg, val);
                bin_tx_fifo_put(PROTOCOL_ACK);
                break;
            case PROTOCOL_READ_REG:
                //fread(&reg, sizeof(uint8_t), 1, uart);
                bin_rx_fifo_get_blocking(&reg);
                //val = swio_read_reg(reg);
                val = ch32vswio_get(reg);
                //fwrite(&val, sizeof(uint32_t), 1, uart);
                for(uint8_t i=0;i<4;i++){
                    c=val&0xff;
                    bin_tx_fifo_put(c);
                    val=val>>8;
                }
                break;
        }
    }
}
