#include <stdio.h>
#include "pico/stdlib.h"
#include <stdint.h>
#include "hardware/uart.h"
#include "pirate.h"
#include "system_config.h"
#include "opt_args.h"
#include "bytecode.h"
#include "mode/hwuart.h"
#include "pirate/bio.h"
#include "ui/ui_prompt.h"
#include "ui/ui_term.h"
#include "command_attributes.h"
#include "ui/ui_format.h"
#include "pirate/storage.h"
#include "usb_rx.h"
#include "usb_tx.h"
#include "ui/ui_help.h"
#include "commands/uart/nmea.h"
#include "commands/uart/bridge.h"
#include "commands/uart/simcard.h"
#include "pirate/hwuart_pio.h"
#include "commands/hdplxuart/bridge.h"

static struct _uart_mode_config mode_config;
static struct command_attributes periodic_attributes;

// command configuration
const struct _command_struct hwhduart_commands[]={   //Function Help
// note: for now the allow_hiz flag controls if the mode provides it's own help
    //{"gps",false,&nmea_decode_handler,T_HELP_UART_NMEA}, // the help is shown in the -h *and* the list of mode apps
    //{"bridge",false,&uart_bridge_handler,T_HELP_UART_BRIDGE}, // the help is shown in the -h *and* the list of mode apps
	//{"sim", false, &simcard_handler, 0x00}
	{"bridge", false, &hduart_bridge_handler, 0x00}
};
const uint32_t hwhduart_commands_count=count_of(hwhduart_commands);

static const char pin_labels[][5]={
	"RXTX",
    "RST", //TODO: support reset like 2 wire mode 
	"CTS",
    "RTS",
};

uint32_t hwhduart_setup(void){
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

	static const struct prompt_item uart_speed_menu[]={{T_UART_SPEED_MENU_1}};
	static const struct prompt_item uart_blocking_menu[]={{T_UART_BLOCKING_MENU_1},{T_UART_BLOCKING_MENU_2}};		
	static const struct prompt_item uart_parity_menu[]={{T_UART_PARITY_MENU_1},{T_UART_PARITY_MENU_2},{T_UART_PARITY_MENU_3}};		
	static const struct prompt_item uart_data_bits_menu[]={{T_UART_DATA_BITS_MENU_1}};		
	static const struct prompt_item uart_stop_bits_menu[]={{T_UART_STOP_BITS_MENU_1},{T_UART_STOP_BITS_MENU_2}};		
	//static const struct prompt_item uart_blocking_menu[]={{T_UART_BLOCKING_MENU_1},{T_UART_BLOCKING_MENU_2}};		

	static const struct ui_prompt uart_menu[]={
		{T_UART_SPEED_MENU,uart_speed_menu,count_of(uart_speed_menu),T_UART_SPEED_PROMPT, 1, 1000000, 115200, 	0,&prompt_int_cfg},
		{T_UART_PARITY_MENU,uart_parity_menu,count_of(uart_parity_menu),T_UART_PARITY_PROMPT,0,0,1,		0,&prompt_list_cfg},
		{T_UART_DATA_BITS_MENU,uart_data_bits_menu,count_of(uart_data_bits_menu),T_UART_DATA_BITS_PROMPT, 5, 8, 8,		0,&prompt_int_cfg},
		{T_UART_STOP_BITS_MENU,uart_stop_bits_menu,count_of(uart_stop_bits_menu),T_UART_STOP_BITS_PROMPT, 0, 0, 1,		0,&prompt_list_cfg},
		{T_UART_BLOCKING_MENU,uart_blocking_menu,count_of(uart_blocking_menu),T_UART_BLOCKING_PROMPT, 0, 0, 1,		0,&prompt_list_cfg}					
	};
	prompt_result result;

	const char config_file[]="bphwuart.bp";

	struct _mode_config_t config_t[]={
		{"$.baudrate", &mode_config.baudrate},
		{"$.data_bits", &mode_config.data_bits},
		{"$.stop_bits", &mode_config.stop_bits},
		{"$.parity", &mode_config.parity}
	};

	if(storage_load_mode(config_file, config_t, count_of(config_t))){
		printf("\r\n\r\n%s%s%s\r\n", ui_term_color_info(), t[T_USE_PREVIOUS_SETTINGS], ui_term_color_reset());
		printf(" %s: %d %s\r\n", t[T_UART_SPEED_MENU], mode_config.baudrate, t[T_UART_BAUD]);			
		printf(" %s: %d\r\n", t[T_UART_DATA_BITS_MENU], mode_config.data_bits);
		printf(" %s: %s\r\n", t[T_UART_PARITY_MENU], t[uart_parity_menu[mode_config.parity].description]);
		printf(" %s: %d\r\n", t[T_UART_STOP_BITS_MENU], mode_config.stop_bits);
		bool user_value;
		if(!ui_prompt_bool(&result, true, true, true, &user_value)) return 0;		
		if(user_value) return 1; //user said yes, use the saved settings
	}

	ui_prompt_uint32(&result, &uart_menu[0], &mode_config.baudrate);
	if(result.exit) return 0;

	ui_prompt_uint32(&result, &uart_menu[2], &temp);
	if(result.exit) return 0;
	mode_config.data_bits=(uint8_t)temp;

	ui_prompt_uint32(&result, &uart_menu[1], &temp); //could also just subtract one...
	if(result.exit) return 0;
	mode_config.parity=(uint8_t)temp;
	//uart_parity_t { UART_PARITY_NONE, UART_PARITY_EVEN, UART_PARITY_ODD }
	//subtract 1 for actual parity setting
	mode_config.parity--;

	ui_prompt_uint32(&result, &uart_menu[3], &temp);
	if(result.exit) return 0;
	mode_config.stop_bits=(uint8_t)temp;
	//block=(ui_prompt_int(UARTBLOCKINGMENU, 1, 2, 2)-1);

	storage_save_mode(config_file, config_t, count_of(config_t));
	
	mode_config.async_print=false;
	
	return 1;
}

uint32_t hwhduart_setup_exc(void){
	//setup peripheral
	//half duplex
	hwuart_pio_init(mode_config.data_bits, mode_config.parity, mode_config.stop_bits, mode_config.baudrate);
	system_bio_claim(true, M_UART_RXTX, BP_PIN_MODE, pin_labels[0]);

    //printf("\r\nHalf Duplex UART is a work in progress.\r\nPlease reserve bug reports for later.\r\n");
	return 1;
}

void hwhduart_periodic(void){
	uint32_t raw;
	uint8_t cooked;
	if(hwuart_pio_read(&raw, &cooked)){
		//printf("PIO: 0x%04X ", raw);
		printf("0x%02x ", cooked);
		//ui_format_print_number_2(&periodic_attributes,  &cooked);
	}
}

void hwhduart_open(struct _bytecode *result, struct _bytecode *next){	
	//clear FIFO and enable UART
	//while(uart_is_readable(M_UART_PORT)){
	//	uart_getc(M_UART_PORT);
	//}

    mode_config.async_print=true;
    result->data_message=t[T_UART_OPEN_WITH_READ];
}

void hwhduart_open_read(struct _bytecode *result, struct _bytecode *next){    // start with read
    mode_config.async_print=true;
    result->data_message=t[T_UART_OPEN_WITH_READ];
}

void hwhduart_close(struct _bytecode *result, struct _bytecode *next){
	mode_config.async_print=false;
	result->data_message=t[T_UART_CLOSE];
}

void hwhduart_start_alt(struct _bytecode *result, struct _bytecode *next){
	result->data_message=t[T_HW2WIRE_RST_HIGH];
	bio_input(M_2WIRE_RST);
}

void hwhduart_stop_alt(struct _bytecode *result, struct _bytecode *next){
	result->data_message=t[T_HW2WIRE_RST_LOW];
	bio_output(M_2WIRE_RST);
}

void hwhduart_write(struct _bytecode *result, struct _bytecode *next){
	if(mode_config.blocking){
		//uart_putc_raw(M_UART_PORT, result->out_data);
	}else{
		//uart_putc_raw(M_UART_PORT, result->out_data);
	}
	hwuart_pio_write(result->out_data);
}

void hwhduart_read(struct _bytecode *result, struct _bytecode *next){
	uint32_t timeout=0xfff;
	uint32_t raw;
	uint8_t cooked;

	while(!hwuart_pio_read(&raw, &cooked)){
		timeout--;
		if(!timeout){
			result->error=SRES_ERROR;
			result->error_message=t[T_UART_NO_DATA_READ];	
			return;			
		}
	}
	result->in_data=cooked;
}

void hwhduart_macro(uint32_t macro){
    struct command_result result;
	switch(macro){
		case 0:		printf("1. Transparent UART bridge\r\n");
				break;
		case 1:
            //uart_bridge_handler(&result);
			break;
		default:	printf("%s\r\n", t[T_MODE_ERROR_MACRO_NOT_DEFINED]);
				system_config.error=1;
	}
}

void hwhduart_cleanup(void){
	//disable peripheral
	//half duplex
	hwuart_pio_deinit();
	system_bio_claim(false, M_UART_RXTX, BP_PIN_MODE,0);

	// reset all pins to safe mode (done before mode change, but we do it here to be safe)
	bio_init();
	// update modeConfig pins
	system_config.misoport=0;
	system_config.mosiport=0;
	system_config.misopin=0;
	system_config.mosipin=0;
}

void hwhduart_settings(void){
	uint32_t par=0;

	switch(mode_config.parity){
		case 	UART_PARITY_NONE: par=1;
			break;
		case 	UART_PARITY_EVEN: par=2;
			break;
		case 	UART_PARITY_ODD: par=3;
			break;
	}
	//printf("HWUSART (br parity numbits stopbits block)=(%d %d %d %d %d)", br, par, nbits, ((sbits>>12)+1), (block+1));
}

void hwhduart_printerror(void){
	uint32_t error;
}

void hwhduart_help(void){
	printf("Peer to peer HALF DUPLEX asynchronous protocol with open drain bus.\r\n");
	printf("Requires pull-up resistors\r\n\r\n");

	if(mode_config.parity==UART_PARITY_NONE){
		printf("BPCMD\t     |                      DATA(8 bits)               |\r\n");
		printf("\tIDLE |STRT| D7 | D6 | D5 | D4 | D3 | D2 | D1 | D0 |STOP|IDLE\r\n");
		printf("TXD\t\"\"\"\"\"|____|{##}|{##}|{##}|{##}|{##}|{##}|{##}|{##}|\"\"\"\"|\"\"\"\"\"\r\n");
		printf("RXD\t\"\"\"\"\"|____|{##}|{##}|{##}|{##}|{##}|{##}|{##}|{##}|\"\"\"\"|\"\"\"\"\"\r\n");
	}else{
		printf("BPCMD\t     |                      DATA(8/9 bits)                  |\r\n");
		printf("\tIDLE |STRT| D7 | D6 | D5 | D4 | D3 | D2 | D1 | D0 |PRTY|STOP|IDLE\r\n");
		printf("TXD\t\"\"\"\"\"|____|{##}|{##}|{##}|{##}|{##}|{##}|{##}|{##}|{##}|\"\"\"\"|\"\"\"\"\"\r\n");
		printf("RXD\t\"\"\"\"\"|____|{##}|{##}|{##}|{##}|{##}|{##}|{##}|{##}|{##}|\"\"\"\"|\"\"\"\"\"\r\n");
	}

	printf("\t              ^sample moment\r\n");
	printf("\r\n");
	printf("Connections:\r\n");
	printf("\tRXTX\t------------------ RXTX\r\n");
	printf("\tGND\t------------------ GND\r\n\r\n");

    printf("%s{\tuse { to print data as it arrives\r\n}/]\t use } or ] to stop printing data\r\n", ui_term_color_info());

	ui_help_mode_commands(hwhduart_commands, hwhduart_commands_count);
}







	
