#include <stdio.h>
#include "pico/stdlib.h"
#include <stdint.h>
#include <string.h>
#include "pirate.h"
#include "system_config.h"
#include "opt_args.h"
#include "bytecode.h"
#include "mode/hw1wire.h"
#include "bio.h"
#include "ui/ui_prompt.h"
#include "storage.h"
#include "ui/ui_term.h"
#include "hardware/pio.h"
#include "pico/binary_info.h"
#include "hardware/clocks.h"
#include "mode/onewire.h"

#define M_OW_PIO pio1
#define M_OW_OWD BIO3

static const char pin_labels[][5]=
{
	"OWD"
};

struct owobj owobj;

uint32_t hw1wire_setup(void)
{
	uint32_t temp;	
	return 1;
}

uint32_t hw1wire_setup_exc(void)
{
	system_bio_claim(true, M_OW_OWD, BP_PIN_MODE, pin_labels[0]);

    owobj.pio = M_OW_PIO;
    owobj.sm = 3;
    owobj.pin = bio2bufiopin[BIO3];
    owobj.dir = bio2bufdirpin[BIO3];

    onewire_init(&owobj);
    onewire_set_fifo_thresh(&owobj, 8);
    pio_sm_set_enabled(owobj.pio, owobj.sm, true);
	
    return 1;
}

void hw1wire_start(struct _bytecode *result, struct _bytecode *next)
{
	result->data_message=t[T_HW1WIRE_RESET];

	if(bio_get(M_OW_OWD)==0)
	{
		result->error_message=t[T_HWI2C_NO_PULLUP_DETECTED];
		result->error=SRES_WARN; 
	}
	
	uint8_t device_detect=onewire_reset(&owobj);

	if(device_detect)
    {
        //result->error_message=t[T_HW1WIRE_PRESENCE_DETECT];
    }
    else
	{
        result->error_message=t[T_HW1WIRE_NO_DEVICE];
		result->error=SRES_ERROR;
	}
        
}

void hw1wire_write(struct _bytecode *result, struct _bytecode *next)
{
    onewire_tx_byte(&owobj, result->out_data);
    onewire_wait_for_idle(&owobj);
}

void hw1wire_read(struct _bytecode *result, struct _bytecode *next)
{
    result->in_data=onewire_rx_byte(&owobj);
}

void hw1wire_cleanup(void)
{
	onewire_cleanup(&owobj);
	bio_init();
	system_bio_claim(false, M_OW_OWD, BP_PIN_MODE,0);
}

// MACROS
void hw1wire_macro(uint32_t macro)
{
	uint32_t result=0;
	switch(macro)
	{
		case 0:		printf(" 1. 1-Wire ROM search\r\n");
				break;
		case 1:		onewire_test_romsearch(&owobj);	break;
		default:	printf("%s\r\n", t[T_MODE_ERROR_MACRO_NOT_DEFINED]);
				system_config.error=1;
	}

	if(result)
	{
		printf("Device not found\r\n");
	}
}

/* Simple test with single DS18B20
   Configure, start conversion and read temperature.
   Assume phantom power. */
void  onewire_ds18b20_conversion(struct owobj *owobj){
    int  i;
    unsigned char buf[9];
    PIO pio = owobj->pio;
    uint sm = owobj->sm;
    uint offset = owobj->offset;
    uint pin =  owobj->pin;
    uint dir =  owobj->dir;
    int32_t temp;
    
    //onewire_program_init(pio, sm, offset, pin, dir);
    //pio_sm_set_enabled(pio, sm, true);

    if( ! onewire_reset(owobj) ){
        printf("No Device\r\n");
        return;
    }

    onewire_tx_byte(owobj, 0xcc); // Skip ROM command
    onewire_tx_byte(owobj, 0x4e); // Write Scatchpad
    onewire_tx_byte(owobj, 0x00); // TH
    onewire_tx_byte(owobj, 0x00); // TL
    onewire_tx_byte(owobj, 0x7f); // CONF (12bit)

    onewire_wait_for_idle(owobj);

    if( ! onewire_reset(owobj) ){
        printf("No Device\r\n");
        return;
    }

    onewire_tx_byte(owobj, 0xcc); // Skip ROM command
    onewire_tx_byte(owobj, 0x44); // Convert T
    sleep_ms(800);  /* 12bit: 750 ms */
    //onewire_end_spu(owobj);

    if( ! onewire_reset(owobj) ){
        printf("No Device\r\n");
        return;
    }
    
    onewire_tx_byte(owobj, 0xcc); // Skip ROM command
    onewire_tx_byte(owobj, 0xbe); // Read Scatchpad
    for(i=0; i<9; i++){
        buf[i] = onewire_rx_byte(owobj);
    }
    
    if( calc_crc8_buf(buf, 8) != buf[8] ){
        printf("CRC Fail\r\n");
        return;
    }

    temp = buf[0]|(buf[1]<<8);
    if( temp & 0x8000 ){
        temp |= 0xffff0000;
    }
    printf("Temperature: %.3f\r\n", (float)temp/16);

    printf("Rx bytes:");
    for(i=0; i<9; i++){
        printf(" %.2x", buf[i]);
    }
    printf("\r\n");
}