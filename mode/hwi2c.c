#include <stdio.h>
#include "pico/stdlib.h"
#include <stdint.h>
#include "pirate.h"
#include "system_config.h"
#include "opt_args.h"
#include "bytecode.h"
#include "mode/hwi2c.h"
#include "pirate/bio.h"
#include "ui/ui_prompt.h"
#include "hwi2c.pio.h"
#include "pirate/hwi2c_pio.h"
#include "pirate/storage.h"
#include "commands/i2c/scan.h"
#include "commands/i2c/demos.h"
#include "ui/ui_term.h"
#include "ui/ui_help.h"

#define M_I2C_PIO pio0
#define M_I2C_SDA BIO0
#define M_I2C_SCL BIO1

static const char pin_labels[][5]={
	"SDA",
	"SCL",
};

static struct _i2c_mode_config mode_config;

static PIO pio = M_I2C_PIO;
static uint pio_state_machine = M_I2C_PIO_SM;

// command configuration
const struct _command_struct hwi2c_commands[]={   //Function Help
// note: for now the allow_hiz flag controls if the mode provides it's own help
    {"scan",0x00,&i2c_search_addr,T_HELP_I2C_SCAN}, // the help is shown in the -h *and* the list of mode apps
	{"si7021",0x00,&demo_si7021,T_HELP_I2C_SI7021},
	{"ms5611",0x00,&demo_ms5611,T_HELP_I2C_MS5611},
	{"tsl2561",0x00,&demo_tsl2561,T_HELP_I2C_TSL2561},
};
const uint32_t hwi2c_commands_count=count_of(hwi2c_commands);


uint32_t hwi2c_setup(void){
	uint32_t temp;

	// menu items options
	static const struct prompt_item i2c_data_bits_menu[]={{T_HWI2C_DATA_BITS_MENU_1},{T_HWI2C_DATA_BITS_MENU_2}};
	static const struct prompt_item i2c_speed_menu[]={{T_HWI2C_SPEED_MENU_1}};		

	static const struct ui_prompt i2c_menu[]={
		{T_HWI2C_SPEED_MENU,i2c_speed_menu,	count_of(i2c_speed_menu),T_HWI2C_SPEED_PROMPT, 1,1000,400,		0,&prompt_int_cfg},
		{T_HWI2C_DATA_BITS_MENU,i2c_data_bits_menu,	count_of(i2c_data_bits_menu),T_HWI2C_DATA_BITS_PROMPT, 0,0,1, 	0,&prompt_list_cfg}
	};

	const char config_file[]="bpi2c.bp";

	struct _mode_config_t config_t[]={
		{"$.baudrate", &mode_config.baudrate},
		{"$.data_bits", &mode_config.data_bits}
	};
	prompt_result result;

	if(storage_load_mode(config_file, config_t, count_of(config_t))){
		printf("\r\n\r\n%s%s%s\r\n", ui_term_color_info(), t[T_USE_PREVIOUS_SETTINGS], ui_term_color_reset());
		printf(" %s: %dKHz\r\n", t[T_HWI2C_SPEED_MENU], mode_config.baudrate);			
		//printf(" %s: %s\r\n", t[T_HWI2C_DATA_BITS_MENU], t[i2c_data_bits_menu[mode_config.data_bits].description]);
		
		bool user_value;
		if(!ui_prompt_bool(&result, true, true, true, &user_value)) return 0;		
		if(user_value) return 1; //user said yes, use the saved settings
	}
	ui_prompt_uint32(&result, &i2c_menu[0], &mode_config.baudrate);
	if(result.exit) return 0;
	//printf("Result: %d\r\n", mode_config.baudrate);
	//ui_prompt_uint32(&result, &i2c_menu[1], &temp);
	//if(result.exit) return 0;
	//mode_config.data_bits=(uint8_t)temp-1;
	storage_save_mode(config_file, config_t, count_of(config_t));
	return 1;
}

uint32_t hwi2c_setup_exc(void){
	pio_i2c_init(pio, pio_state_machine, bio2bufiopin[M_I2C_SDA], bio2bufiopin[M_I2C_SCL], bio2bufdirpin[M_I2C_SDA], bio2bufdirpin[M_I2C_SCL], mode_config.baudrate);
	system_bio_claim(true, M_I2C_SDA, BP_PIN_MODE, pin_labels[0]);
	system_bio_claim(true, M_I2C_SCL, BP_PIN_MODE, pin_labels[1]);
	mode_config.start_sent=false;
	return 1;
}

bool hwi2c_error(uint32_t error, struct _bytecode *result){
	switch(error){
		case 1:
			result->error_message=t[T_HWI2C_I2C_ERROR];
			result->error=SRES_ERROR; 
			pio_i2c_resume_after_error();
			return true;
			break;
		case 2:
			result->error_message=t[T_HWI2C_TIMEOUT];
			result->error=SRES_ERROR; 
			pio_i2c_resume_after_error();
			return true;
			break;
		default:
			return false;
	}
}

void hwi2c_start(struct _bytecode *result, struct _bytecode *next){
	result->data_message=t[T_HWI2C_START];
	if(hwi2c_checkshort()){
		result->error_message=t[T_HWI2C_NO_PULLUP_DETECTED];
		result->error=SRES_WARN; 
	}
	uint8_t error=pio_i2c_start_timeout(0xfffff);
	if(!hwi2c_error(error, result)){
		mode_config.start_sent=true;
	}
}

void hwi2c_stop(struct _bytecode *result, struct _bytecode *next){
	result->data_message=t[T_HWI2C_STOP];
	uint32_t error=pio_i2c_stop_timeout( 0xffff);
	hwi2c_error(error, result);
}

void hwi2c_write(struct _bytecode *result, struct _bytecode *next){
	//if a start was just sent, determine if this is a read or write address
	// and configure the PIO I2C
	if(mode_config.start_sent){
		pio_i2c_rx_enable( (result->out_data & 1u));
		mode_config.start_sent=false;
	}
	uint32_t error=pio_i2c_write_timeout( result->out_data, 0xffff);
	hwi2c_error(error, result);
	result->data_message=(error?t[T_HWI2C_NACK]:t[T_HWI2C_ACK]);
}

void hwi2c_read(struct _bytecode *result, struct _bytecode *next){
	bool ack=(next?(next->command!=4):true);
	uint32_t error=pio_i2c_read_timeout( &result->in_data, ack, 0xffff);
    hwi2c_error(error, result);
	result->data_message=(ack?t[T_HWI2C_ACK]:t[T_HWI2C_NACK]);
}

void hwi2c_macro(uint32_t macro){
	uint32_t result=0;
	switch(macro){
		case 0:		printf(" 0. Macro menu\r\n");
				break;
		default:	printf("%s\r\n", t[T_MODE_ERROR_MACRO_NOT_DEFINED]);
				system_config.error=1;
	}
}


void hwi2c_cleanup(void){
	pio_i2c_cleanup();
	bio_init();
	system_bio_claim(false, M_I2C_SDA, BP_PIN_MODE,0);
	system_bio_claim(false, M_I2C_SCL, BP_PIN_MODE,0);
}

void hwi2c_settings(void){
	printf("HWI2C (speed)=(%d)", mode_config.baudrate_actual);
}

void hwi2c_printI2Cflags(void){
	uint32_t temp;
}

void hwi2c_help(void){
	printf("Muli-Master-multi-slave 2 wire protocol using a CLOCK and a bidirectional DATA\r\n");
	printf("line in opendrain configuration. Standard clock frequencies are 100KHz, 400KHz\r\n");
	printf("and 1MHz.\r\n");
	printf("\r\n");
	printf("More info: https://en.wikipedia.org/wiki/I2C\r\n");
	printf("\r\n");
	printf("Electrical:\r\n");
	printf("\r\n");
	printf("BPCMD\t   { |            ADDRESS(7bits+R/!W bit)             |\r\n");
	printf("CMD\tSTART| A6  | A5  | A4  | A3  | A2  | A1  | A0  | R/!W| ACK* \r\n");
	printf("\t-----|-----|-----|-----|-----|-----|-----|-----|-----|-----\r\n");
	printf("SDA\t\"\"___|_###_|_###_|_###_|_###_|_###_|_###_|_###_|_###_|_###_ ..\r\n");
	printf("SCL\t\"\"\"\"\"|__\"__|__\"__|__\"__|__\"__|__\"__|__\"__|__\"__|__\"__|__\"__ ..\r\n");
	printf("\r\n");
	printf("BPCMD\t   |                      DATA (8bit)              |     |  ]  |\r\n");
	printf("CMD\t.. | D7  | D6  | D5  | D4  | D3  | D2  | D1  | D0  | ACK*| STOP|  \r\n");
	printf("\t  -|-----|-----|-----|-----|-----|-----|-----|-----|-----|-----|\r\n");
	printf("SDA\t.. |_###_|_###_|_###_|_###_|_###_|_###_|_###_|_###_|_###_|___\"\"|\r\n");
	printf("SCL\t.. |__\"__|__\"__|__\"__|__\"__|__\"__|__\"__|__\"__|__\"__|__\"__|\"\"\"\"\"|\r\n");
	printf("\r\n");
	printf("* Receiver needs to pull SDA down when address/byte is received correctly\r\n");
	printf("\r\n");
	printf("Connection:\r\n");
	printf("\t\t  +--[2k]---+--- +3V3 or +5V0\r\n");
	printf("\t\t  | +-[2k]--|\r\n");
	printf("\t\t  | |\r\n");
	printf("\tSDA \t--+-|------------- SDA\r\n");
	printf("{BP}\tSCL\t----+------------- SCL  {DUT}\r\n");
	printf("\tGND\t------------------ GND\r\n\r\n");	

	ui_help_mode_commands(hwi2c_commands, hwi2c_commands_count);			
}

uint8_t hwi2c_checkshort(void){
	uint8_t temp;
	temp=(bio_get(M_I2C_SDA)==0?1:0);
	temp|=(bio_get(M_I2C_SCL)==0?2:0);
	return (temp==3);			// there is only a short when both are 0 otherwise repeated start wont work
}


