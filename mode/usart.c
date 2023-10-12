#include <stdio.h>
#include "pico/stdlib.h"
#include <stdint.h>
#include "hardware/uart.h"
#include "pirate.h"
#include "system_config.h"
#include "mode/usart.h"
#include "bio.h"
#include "ui/ui_prompt.h"
#include "ui/ui_term.h"
#include "ui/ui_format.h"
#include "storage.h"

#define M_UART_PORT uart0
#define M_UART_TX BIO4
#define M_UART_RX BIO5
#define M_UART_RTS
#define M_UART_CTS

static const char pin_labels[][5]={
	"TX->", 
	"RX<-",
	"RTS",
	"CTS"
};

static struct _uart_mode_config mode_config;
static struct command_attributes periodic_attributes;
uint32_t HWUSART_periodic(void)
{
	if(uart_is_readable(M_UART_PORT) && mode_config.async_print)
	{
		//printf("ASYNC: %d\r\n", uart_getc(M_UART_PORT));
		uint32_t temp = uart_getc(M_UART_PORT);

		ui_format_print_number_2(&periodic_attributes,  &temp);
	}
}


void HWUSART_open(void)			// start
{
	//mode_config.async_print=true;

}
void HWUSART_open_read(void)	// start with read
{
	mode_config.async_print=true;
}
void HWUSART_close(void)		// stop
{
	mode_config.async_print=false;
}

uint32_t HWUSART_send(uint32_t d)
{
	if(mode_config.blocking)
		uart_putc_raw(M_UART_PORT, d);
	else
		uart_putc_raw(M_UART_PORT, d);

	//HWUSART_printerror();

	return 0;
}
uint32_t HWUSART_read(void)
{
	uint32_t received;

	if(uart_is_readable(M_UART_PORT))
		received=uart_getc(M_UART_PORT);
	else
		received=0xffff;

	//HWUSART_printerror();

	return received;
}

void HWUSART_macro(uint32_t macro)
{
	switch(macro)
	{
		case 0:		printf("No macros available");
				break;
		default:	printf("Macro not defined");
				system_config.error=1;
	}
}

uint32_t HWUSART_setup(void)
{
	uint32_t temp;
	// did the user leave us arguments?
	
	/*
	// baudrate
	if(cmdtail!=cmdhead) cmdtail=(cmdtail+1)&(CMDBUFFSIZE-1);
	consumewhitechars();
	br=getint();
	if(br==0) 			// any value is ok for us, but for the other side? :)
		system_config.error=1;

	// parity
	if(cmdtail!=cmdhead) cmdtail=(cmdtail+1)&(CMDBUFFSIZE-1);
	consumewhitechars();
	parity=getint()-1;
	if(parity<=2)
	{
		switch (parity)
		{
			case 0:	parity=USART_PARITY_NONE;
				break;
			case 1:	parity=USART_PARITY_EVEN;
				break;
			case 2:	parity=USART_PARITY_ODD;
				break;
		}
	}
		else system_config.error=1;

	// numbits
	if(cmdtail!=cmdhead) cmdtail=(cmdtail+1)&(CMDBUFFSIZE-1);
	consumewhitechars();
	nbits=getint();
	if(!((nbits==8)||(nbits==9)))
		system_config.error=1;

	//stopbits
	if(cmdtail!=cmdhead) cmdtail=(cmdtail+1)&(CMDBUFFSIZE-1);
	consumewhitechars();
	sbits=getint()-1;
	if(sbits<=4)
		sbits<<=12;
	else
		system_config.error=1;

	// block
	if(cmdtail!=cmdhead) cmdtail=(cmdtail+1)&(CMDBUFFSIZE-1);
	consumewhitechars();
	block=getint();
	if(block>=1)
		block-=1;
	else
		system_config.error=1;
*/
	// did the user did it right?

/*#define UARTSPEEDMENU		"\r\nUART Speed\r\n 1... baudrate\r\nbr> "
#define UARTPARITYMENU 		"\r\nParity\r\n 1. none*\r\n 2. even\r\n 3. odd\r\nparity> "
#define UARTNUMBITSMENU 	"\r\nNumber of bits\r\n 8. 8*\r\n 9. 9\r\nbits> "
#define UARTSTOPBITSMENU	"\r\nNumber of stop bits\r\n 1. 1*\r\n 2. 0.5\r\n 3. 2\r\n 4. 1.5\r\nbits> "
#define UARTBLOCKINGMENU	"\r\nUse blocking functions?\r\n 1. no\r\n 2. yes\r\nblock> "


//UART0 on gp0 gp1/BIO0/BIO1
	if(system_config.error)			// go interactive 
	{
*/

		
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
		static const struct prompt_item uart_parity_menu[]={{T_UART_PARITY_MENU_1},{T_UART_PARITY_MENU_2},{T_UART_PARITY_MENU_3}};		
		static const struct prompt_item uart_data_bits_menu[]={{T_UART_DATA_BITS_MENU_1}};		
		static const struct prompt_item uart_stop_bits_menu[]={{T_UART_STOP_BITS_MENU_1},{T_UART_STOP_BITS_MENU_2}};		
		static const struct prompt_item uart_blocking_menu[]={{T_UART_BLOCKING_MENU_1},{T_UART_BLOCKING_MENU_2}};		

		static const struct ui_prompt uart_menu[]={
			{T_UART_SPEED_MENU,uart_speed_menu,count_of(uart_speed_menu),T_UART_SPEED_PROMPT, 1, 1000000, 115200, 	0,&prompt_int_cfg},
			{T_UART_PARITY_MENU,uart_parity_menu,count_of(uart_parity_menu),T_UART_PARITY_PROMPT,0,0,1,		0,&prompt_list_cfg},
			{T_UART_DATA_BITS_MENU,uart_data_bits_menu,count_of(uart_data_bits_menu),T_UART_DATA_BITS_PROMPT, 5, 8, 8,		0,&prompt_int_cfg},
			{T_UART_STOP_BITS_MENU,uart_stop_bits_menu,count_of(uart_stop_bits_menu),T_UART_STOP_BITS_PROMPT, 0, 0, 1,		0,&prompt_list_cfg},
			{T_UART_BLOCKING_MENU,uart_blocking_menu,count_of(uart_blocking_menu),T_UART_BLOCKING_PROMPT, 0, 0, 1,		0,&prompt_list_cfg}					
		};
		prompt_result result;

		const char config_file[]="bpuart.bp";

		struct _mode_config_t config_t[]={
			{"$.baudrate", &mode_config.baudrate},
			{"$.data_bits", &mode_config.data_bits},
			{"$.stop_bits", &mode_config.stop_bits},
			{"$.parity", &mode_config.parity}
		};

		if(storage_load_mode(config_file, config_t, count_of(config_t)))
		{
			printf("\r\n\r\n%s%s%s\r\n", ui_term_color_info(), t[T_USE_PREVIOUS_SETTINGS], ui_term_color_reset());
			printf(" %s: %d %s\r\n", t[T_UART_SPEED_MENU], mode_config.baudrate, t[T_UART_BAUD]);			
			printf(" %s: %d\r\n", t[T_UART_DATA_BITS_MENU], mode_config.data_bits);
			printf(" %s: %s\r\n", t[T_UART_PARITY_MENU], t[uart_parity_menu[mode_config.parity].description]);
			printf(" %s: %d\r\ny/n>", t[T_UART_STOP_BITS_MENU], mode_config.stop_bits);
			do{
				temp=ui_prompt_yes_no();
			}while(temp>1);

			printf("\r\n");

			if(temp==1) return 1;
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
	//}

#if 1
	storage_save_mode(config_file, config_t, count_of(config_t));
#endif

	return 1;

}



uint32_t HWUSART_setup_exc(void)
{
	//setup peripheral
	mode_config.baudrate_actual=uart_init(M_UART_PORT, mode_config.baudrate);
	printf("\r\n%s%s: %u %s%s", 
		ui_term_color_notice(),
		t[T_UART_ACTUAL_SPEED_BAUD],
		mode_config.baudrate_actual,
		t[T_UART_BAUD],
		ui_term_color_reset()
	);
	uart_set_format(M_UART_PORT,mode_config.data_bits, mode_config.stop_bits, mode_config.parity);
	
	//set buffers to correct position
	bio_buf_output(M_UART_TX); //tx
	bio_buf_input(M_UART_RX); //rx

	//assign peripheral to io pins
	bio_set_function(M_UART_TX, GPIO_FUNC_UART); //tx
	bio_set_function(M_UART_RX, GPIO_FUNC_UART); //rx

	system_bio_claim(true, M_UART_TX, BP_PIN_MODE, pin_labels[0]);
	system_bio_claim(true, M_UART_RX, BP_PIN_MODE, pin_labels[1]);
	return 1;
}



void HWUSART_cleanup(void)
{
	//disable peripheral
	uart_deinit(M_UART_PORT);

	system_bio_claim(false, M_UART_TX, BP_PIN_MODE,0);
	system_bio_claim(false, M_UART_RX, BP_PIN_MODE,0);

	// reset all pins to safe mode (done before mode change, but we do it here to be safe)
	bio_init();

	// update modeConfig pins
	system_config.misoport=0;
	system_config.mosiport=0;
	system_config.misopin=0;
	system_config.mosipin=0;

}
/*void HWUSART_pins(void)
{
	printf("-\t-\tRXD\tTXD");
}*/
void HWUSART_settings(void)
{
	uint32_t par=0;

	switch(mode_config.parity)
	{
		case 	UART_PARITY_NONE: par=1;
			break;
		case 	UART_PARITY_EVEN: par=2;
			break;
		case 	UART_PARITY_ODD: par=3;
			break;
	}
	//printf("HWUSART (br parity numbits stopbits block)=(%d %d %d %d %d)", br, par, nbits, ((sbits>>12)+1), (block+1));
}

void HWUSART_printerror(void)
{
	uint32_t error;

	/*error=USART_SR(BP_USART)&USARTERRORS;	// not all are errors

	if(error)
	{
		printf("flags: ");
		if(error&USART_SR_PE)		// parity error
			printf("PE ");
		if(error&USART_SR_FE)		// framing error
			printf("FE ");
		if(error&USART_SR_NE)		// noise error
			printf("NE ");
		if(error&USART_SR_ORE)		// overrun
			printf("ORE ");
		if(error&USART_SR_IDLE)		// idle 
			printf("IDLE ");
		if(error&USART_SR_RXNE)		// RX register not empty
			printf("RXNE ");
		if(error&USART_SR_TC)		// transmission complete
			printf("TC ");
		if(error&USART_SR_TXE)		// TX buff empty
			printf("TXE ");
		if(error&USART_SR_LBD)		// LIN break 
			printf("LBD ");
		if(error&USART_SR_CTS)		// CTS set
			printf("CTS ");
		USART_SR(BP_USART)=error;	// clear error(s)

	}*/
}

void HWUSART_help(void)
{
	printf("Peer to peer asynchronous protocol.\r\n");
	printf("\r\n");

	if(mode_config.parity==UART_PARITY_NONE)
	{
		printf("BPCMD\t     |                      DATA(8 bits)               |\r\n");
		printf("\tIDLE |STRT| D7 | D6 | D5 | D4 | D3 | D2 | D1 | D0 |STOP|IDLE\r\n");
		printf("TXD\t\"\"\"\"\"|____|{##}|{##}|{##}|{##}|{##}|{##}|{##}|{##}|\"\"\"\"|\"\"\"\"\"\r\n");
		printf("RXD\t\"\"\"\"\"|____|{##}|{##}|{##}|{##}|{##}|{##}|{##}|{##}|\"\"\"\"|\"\"\"\"\"\r\n");
	}
	else
	{
		printf("BPCMD\t     |                      DATA(8/9 bits)                  |\r\n");
		printf("\tIDLE |STRT| D7 | D6 | D5 | D4 | D3 | D2 | D1 | D0 |PRTY|STOP|IDLE\r\n");
		printf("TXD\t\"\"\"\"\"|____|{##}|{##}|{##}|{##}|{##}|{##}|{##}|{##}|{##}|\"\"\"\"|\"\"\"\"\"\r\n");
		printf("RXD\t\"\"\"\"\"|____|{##}|{##}|{##}|{##}|{##}|{##}|{##}|{##}|{##}|\"\"\"\"|\"\"\"\"\"\r\n");
	}

	printf("\t              ^sample moment\r\n");
	printf("\r\n");
	printf("Connections:\r\n");
	printf("\tTXD\t------------------ RXD\r\n");
	printf("{BP}\tRXD\t------------------ TXD\t{DUT}\r\n");
	printf("\tGND\t------------------ GND\r\n");
}







	
