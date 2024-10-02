#include <stdio.h>
#include "pico/stdlib.h"
#include <stdint.h>
#include "hardware/uart.h"
#include "pirate.h"
#include "system_config.h"
#include "opt_args.h"
#include "bytecode.h"
#include "mode/binloopback.h"
#include "pirate/bio.h"
#include "ui/ui_prompt.h"
#include "ui/ui_term.h"
#include "command_attributes.h"
#include "ui/ui_format.h"
#include "pirate/storage.h"
#include "usb_rx.h"
#include "usb_tx.h"
#include "ui/ui_help.h"

static struct _binloopback_mode_config mode_config;
static struct command_attributes periodic_attributes;

// command configuration
const struct _command_struct binloopback_commands[]={   //Function Help
};
const uint32_t binloopback_commands_count=count_of(binloopback_commands);

uint32_t binloopback_setup(void){
	uint32_t temp;
	periodic_attributes.has_value=true;
	periodic_attributes.has_dot=false;
	periodic_attributes.has_colon=false;
	periodic_attributes.has_string=false;
	periodic_attributes.command=0;    //the actual command called
	periodic_attributes.number_format=4; //DEC/HEX/BIN
	periodic_attributes.value=0;     // integer value parsed from command line
	periodic_attributes.dot=0;       // value after .
	periodic_attributes.colon=0;     // value after :
	
	mode_config.async_print=true;
	
	return 1;
}

uint32_t binloopback_setup_exc(void){
	return 1;
}

void binloopback_periodic(void){
    char c;
	if(mode_config.async_print && bin_tx_fifo_try_get(&c)){
        uint32_t temp=c;
		ui_format_print_number_2(&periodic_attributes, &temp);
	}
}

void binloopback_open(struct _bytecode *result, struct _bytecode *next){	
    mode_config.async_print=false;
    result->data_message = GET_T(T_UART_OPEN);
}

void binloopback_open_read(struct _bytecode *result, struct _bytecode *next){    // start with read
    mode_config.async_print=true;
    result->data_message = GET_T(T_UART_OPEN_WITH_READ);
}

void binloopback_close(struct _bytecode *result, struct _bytecode *next){
	mode_config.async_print=false;
	result->data_message = GET_T(T_UART_CLOSE);
}

void binloopback_write(struct _bytecode *result, struct _bytecode *next){
    char c=result->out_data;
    bin_rx_fifo_add(&c);
}

void binloopback_read(struct _bytecode *result, struct _bytecode *next){
	uint32_t timeout=0xfff;
    char c;
	while(!bin_tx_fifo_try_get(&c)){
		timeout--;
		if(!timeout){
			result->error=SRES_ERROR;
			result->error_message = GET_T(T_UART_NO_DATA_READ);	
			return;			
		}
	}
	result->in_data=c;
}

void binloopback_cleanup(void){
	bio_init();
}







	
