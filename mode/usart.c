#include <stdio.h>
#include "pico/stdlib.h"
#include <stdint.h>
#include "hardware/uart.h"
#include "pirate.h"
#include "system_config.h"
#include "opt_args.h"
#include "bytecode.h"
#include "mode/usart.h"
#include "bio.h"
#include "ui/ui_prompt.h"
#include "ui/ui_term.h"
#include "command_attributes.h"
#include "ui/ui_format.h"
#include "storage.h"
#include "lib/minmea/minmea.h"

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
uint32_t hwusart_periodic(void)
{
	if(uart_is_readable(M_UART_PORT) && mode_config.async_print)
	{
		//printf("ASYNC: %d\r\n", uart_getc(M_UART_PORT));
		uint32_t temp = uart_getc(M_UART_PORT);

		ui_format_print_number_2(&periodic_attributes,  &temp);
	}
}

void hwusart_open(struct _bytecode *result, struct _bytecode *next)			// start
{	
	//clear FIFO and enable UART
	while(uart_is_readable(M_UART_PORT))
	{
		uart_getc(M_UART_PORT);
	}

	if(system_config.write_with_read)
	{
		mode_config.async_print=true;
		result->data_message=t[T_UART_OPEN_WITH_READ];
	}
	else
	{
		mode_config.async_print=false;
		result->data_message=t[T_UART_OPEN];
	}	
}

//I think these will no longer be needed
void hwusart_open_read(struct _bytecode *result, struct _bytecode *next)	// start with read
{
	//mode_config.async_print=true;
	//result->data_message=t[T_UART_OPEN_WITH_READ];
}

void hwusart_close(struct _bytecode *result, struct _bytecode *next)		// stop
{
	mode_config.async_print=false;
	result->data_message=t[T_UART_CLOSE];
}

void hwusart_write(struct _bytecode *result, struct _bytecode *next)
{
	if(mode_config.blocking)
	{
		uart_putc_raw(M_UART_PORT, result->out_data);
	}
	else
	{
		uart_putc_raw(M_UART_PORT, result->out_data);
	}
}

void hwusart_read(struct _bytecode *result, struct _bytecode *next)
{
	uint32_t timeout=0xff;

	//if blocking wait with timeout
	//if(mode_config.blocking)
	//{
		while(!uart_is_readable(M_UART_PORT))
		{
			timeout--;
			if(!timeout)
			{
				result->error=SRES_ERROR;
				result->error_message=t[T_UART_NO_DATA_READ];	
				return;			
			}
		}
	//}

	if(uart_is_readable(M_UART_PORT))
	{
		result->in_data=uart_getc(M_UART_PORT);
	}
	else
	{
		result->error=SRES_ERROR;
		result->error_message=t[T_UART_NO_DATA_READ];
	}

}

void hwusart_macro(uint32_t macro)
{
	switch(macro)
	{
		case 0:		printf("%s\r\n", t[T_MODE_ERROR_NO_MACROS_AVAILABLE]);
				break;
		case 1:
			while (true) {
				char line[MINMEA_MAX_SENTENCE_LENGTH];
				uint32_t nmea_cnt=0;
				while(true)
				{
					if(uart_is_readable(M_UART_PORT))
					{
						uint32_t temp = uart_getc(M_UART_PORT);
						if(nmea_cnt>0 || temp=='$')
						{
							line[nmea_cnt]=temp;
							nmea_cnt++;
							if(temp==0x0a)
							{
								line[nmea_cnt]=0x00;
								break;
							} 
						}
					}
				}
				printf(line);
				switch (minmea_sentence_id(line, false)) {
					case MINMEA_SENTENCE_RMC: {
						struct minmea_sentence_rmc frame;
						if (minmea_parse_rmc(&frame, line)) {
							printf( "$xxRMC: raw coordinates and speed: (%d/%d,%d/%d) %d/%d\r\n",
									frame.latitude.value, frame.latitude.scale,
									frame.longitude.value, frame.longitude.scale,
									frame.speed.value, frame.speed.scale);
							printf( "$xxRMC fixed-point coordinates and speed scaled to three decimal places: (%d,%d) %d\r\n",
									minmea_rescale(&frame.latitude, 1000),
									minmea_rescale(&frame.longitude, 1000),
									minmea_rescale(&frame.speed, 1000));
							printf( "$xxRMC floating point degree coordinates and speed: (%f,%f) %f\r\n",
									minmea_tocoord(&frame.latitude),
									minmea_tocoord(&frame.longitude),
									minmea_tofloat(&frame.speed));
						}
						else {
							printf( "$xxRMC sentence is not parsed\r\n");
						}
					} break;

					case MINMEA_SENTENCE_GGA: {
						struct minmea_sentence_gga frame;
						if (minmea_parse_gga(&frame, line)) {
							printf( "$xxGGA: fix quality: %d\r\n", frame.fix_quality);
						}
						else {
							printf( "$xxGGA sentence is not parsed\r\n");
						}
					} break;

					case MINMEA_SENTENCE_GST: {
						struct minmea_sentence_gst frame;
						if (minmea_parse_gst(&frame, line)) {
							printf( "$xxGST: raw latitude,longitude and altitude error deviation: (%d/%d,%d/%d,%d/%d)\r\n",
									frame.latitude_error_deviation.value, frame.latitude_error_deviation.scale,
									frame.longitude_error_deviation.value, frame.longitude_error_deviation.scale,
									frame.altitude_error_deviation.value, frame.altitude_error_deviation.scale);
							printf( "$xxGST fixed point latitude,longitude and altitude error deviation"
								" scaled to one decimal place: (%d,%d,%d)\r\n",
									minmea_rescale(&frame.latitude_error_deviation, 10),
									minmea_rescale(&frame.longitude_error_deviation, 10),
									minmea_rescale(&frame.altitude_error_deviation, 10));
							printf( "$xxGST floating point degree latitude, longitude and altitude error deviation: (%f,%f,%f)",
									minmea_tofloat(&frame.latitude_error_deviation),
									minmea_tofloat(&frame.longitude_error_deviation),
									minmea_tofloat(&frame.altitude_error_deviation));
						}
						else {
							printf( "$xxGST sentence is not parsed\r\n");
						}
					} break;

					case MINMEA_SENTENCE_GSV: {
						struct minmea_sentence_gsv frame;
						if (minmea_parse_gsv(&frame, line)) {
							printf( "$xxGSV: message %d of %d\r\n", frame.msg_nr, frame.total_msgs);
							printf( "$xxGSV: satellites in view: %d\r\n", frame.total_sats);
							for (int i = 0; i < 4; i++)
								printf( "$xxGSV: sat nr %d, elevation: %d, azimuth: %d, snr: %d dbm\r\n",
									frame.sats[i].nr,
									frame.sats[i].elevation,
									frame.sats[i].azimuth,
									frame.sats[i].snr);
						}
						else {
							printf( "$xxGSV sentence is not parsed\r\n");
						}
					} break;

					case MINMEA_SENTENCE_VTG: {
					struct minmea_sentence_vtg frame;
					if (minmea_parse_vtg(&frame, line)) {
							printf( "$xxVTG: true track degrees = %f\r\n",
								minmea_tofloat(&frame.true_track_degrees));
							printf( "        magnetic track degrees = %f\r\n",
								minmea_tofloat(&frame.magnetic_track_degrees));
							printf( "        speed knots = %f\r\n",
									minmea_tofloat(&frame.speed_knots));
							printf( "        speed kph = %f\r\n",
									minmea_tofloat(&frame.speed_kph));
					}
					else {
							printf( "$xxVTG sentence is not parsed\r\n");
					}
					} break;

					case MINMEA_SENTENCE_ZDA: {
						struct minmea_sentence_zda frame;
						if (minmea_parse_zda(&frame, line)) {
							printf( "$xxZDA: %d:%d:%d %02d.%02d.%d UTC%+03d:%02d\r\n",
								frame.time.hours,
								frame.time.minutes,
								frame.time.seconds,
								frame.date.day,
								frame.date.month,
								frame.date.year,
								frame.hour_offset,
								frame.minute_offset);
						}
						else {
							printf( "$xxZDA sentence is not parsed\r\n");
						}
					} break;

					case MINMEA_INVALID: {
						printf( "$xxxxx sentence is not valid\r\n");
					} break;

					default: {
						printf( "$xxxxx sentence is not parsed\r\n");
					} break;
				}
			}
			break;
		default:	printf("%s\r\n", t[T_MODE_ERROR_MACRO_NOT_DEFINED]);
				system_config.error=1;
	}
}

uint32_t hwusart_setup(void)
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



uint32_t hwusart_setup_exc(void)
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



void hwusart_cleanup(void)
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
/*void hwusart_pins(void)
{
	printf("-\t-\tRXD\tTXD");
}*/
void hwusart_settings(void)
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

void hwusart_printerror(void)
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

void hwusart_help(void)
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







	
