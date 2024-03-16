#include <stdio.h>
#include <math.h>
#include "pico/stdlib.h"
#include <stdint.h>
#include "pirate.h"
#include "system_config.h"
#include "opt_args.h"
#include "bytecode.h"
#include "mode/hw2wire.h"
#include "bio.h"
#include "ui/ui_prompt.h"

#include "hw2wire.pio.h"
#include "mode/hw2wire_pio.h"
#include "storage.h"
#include "ui/ui_term.h"
#include "ui/ui_command.h"
#include "ui/ui_format.h"

#define M_2WIRE_PIO pio0
#define M_2WIRE_SDA BIO0
#define M_2WIRE_SCL BIO1
#define M_2WIRE_RST BIO2

void sle4442(struct command_result *res){
	printf("sle4442\r\n");
}

// command configuration
const struct _command_struct hw2wire_commands[]=
{   //HiZ? Function Help
    {"sle4442",true,&sle4442,T_CMDLN_LS}, //ls
};
const uint32_t hw2wire_commands_count=count_of(hw2wire_commands);



//TODO: RST pin optional
// controlled by {}, and also used by ATR SIM card stuff
// add all bitwise operators
// use own .json settings file
static const char pin_labels[][5]={
	"SDA",
	"SCL",
	"RST"
};

static struct _hw2wire_mode_config mode_config;

static PIO pio = M_2WIRE_PIO;
static uint pio_state_machine = 3;
static uint pio_loaded_offset;

static uint8_t checkshort(void);

uint32_t hw2wire_setup(void)
{
	uint32_t temp;

	// menu items options
	static const struct prompt_item i2c_data_bits_menu[]={{T_HWI2C_DATA_BITS_MENU_1},{T_HWI2C_DATA_BITS_MENU_2}};
	static const struct prompt_item i2c_speed_menu[]={{T_HWI2C_SPEED_MENU_1}};		

	static const struct ui_prompt i2c_menu[]={
		{T_HW2WIRE_SPEED_MENU,i2c_speed_menu,	count_of(i2c_speed_menu),T_HWI2C_SPEED_PROMPT, 1,1000,400,		0,&prompt_int_cfg},
		{T_HWI2C_DATA_BITS_MENU,i2c_data_bits_menu,	count_of(i2c_data_bits_menu),T_HWI2C_DATA_BITS_PROMPT, 0,0,1, 	0,&prompt_list_cfg}
	};

	const char config_file[]="bp2wire.bp";

	struct _mode_config_t config_t[]={
		{"$.baudrate", &mode_config.baudrate},
		{"$.data_bits", &mode_config.data_bits}
	};
	prompt_result result;

	if(storage_load_mode(config_file, config_t, count_of(config_t)))
	{
		printf("\r\n%s%s%s\r\n", ui_term_color_info(), t[T_USE_PREVIOUS_SETTINGS], ui_term_color_reset());
		printf(" %s: %dKHz\r\n", t[T_HW2WIRE_SPEED_MENU], mode_config.baudrate);			
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

uint32_t hw2wire_setup_exc(void){
	pio_loaded_offset = pio_add_program(pio, &hw2wire_program);
    hw2wire_program_init(pio, pio_state_machine, pio_loaded_offset, bio2bufiopin[M_2WIRE_SDA], bio2bufiopin[M_2WIRE_SCL], bio2bufdirpin[M_2WIRE_SDA], bio2bufdirpin[M_2WIRE_SCL], mode_config.baudrate);
	
	system_bio_claim(true, M_2WIRE_SDA, BP_PIN_MODE, pin_labels[0]);
	system_bio_claim(true, M_2WIRE_SCL, BP_PIN_MODE, pin_labels[1]);
	system_bio_claim(true, M_2WIRE_RST, BP_PIN_MODE, pin_labels[2]);

	pio_hw2wire_rx_enable(pio, pio_state_machine, false);
	mode_config.read=false;
	pio_hw2wire_reset(pio, pio_state_machine);

	bio_put(M_2WIRE_RST, 0); //preload the RST pin to be 0 when output	
}

void hw2wire_start(struct _bytecode *result, struct _bytecode *next){
	result->data_message=t[T_HWI2C_START];
	if(checkshort()){
		result->error_message=t[T_HWI2C_NO_PULLUP_DETECTED];
		result->error=SRES_WARN; 
	}
	pio_hw2wire_start(pio, pio_state_machine);
}

void hw2wire_start_alt(struct _bytecode *result, struct _bytecode *next){
	result->data_message=t[T_HW2WIRE_RST_HIGH];
	bio_input(M_2WIRE_RST);
}

void hw2wire_stop(struct _bytecode *result, struct _bytecode *next){
	result->data_message=t[T_HWI2C_STOP];
	pio_hw2wire_stop(pio, pio_state_machine);
}

void hw2wire_stop_alt(struct _bytecode *result, struct _bytecode *next){
	result->data_message=t[T_HW2WIRE_RST_LOW];
	bio_output(M_2WIRE_RST);
}

void hw2wire_write(struct _bytecode *result, struct _bytecode *next){
	if(mode_config.read){
		pio_hw2wire_rx_enable(pio, pio_state_machine, false);
		mode_config.read=false;
	}
	uint32_t temp=result->out_data;
	ui_format_bitorder_manual(&temp, result->bits, system_config.bit_order);
	pio_hw2wire_put16(pio, pio_state_machine, (temp<< 1)|(1u));
}

void hw2wire_read(struct _bytecode *result, struct _bytecode *next){
	if(!mode_config.read){
		pio_hw2wire_rx_enable(pio, pio_state_machine, true);
		mode_config.read=true;
	}
	uint32_t temp;
	pio_hw2wire_get16(pio, pio_state_machine, &temp); 
	ui_format_bitorder_manual(&temp, result->bits, system_config.bit_order);
	result->in_data=temp;
} 

void hw2wire_tick_clock(struct _bytecode *result, struct _bytecode *next){
	pio_hw2wire_clock_tick(pio, pio_state_machine);
}

void hw2wire_set_clk_high(struct _bytecode *result, struct _bytecode *next){
	pio_hw2wire_set_mask(pio, pio_state_machine, 1<<M_2WIRE_SCL, 1<<M_2WIRE_SCL);
}

void hw2wire_set_clk_low(struct _bytecode *result, struct _bytecode *next){
	pio_hw2wire_set_mask(pio, pio_state_machine, 1<<M_2WIRE_SCL, 0);
}

void hw2wire_set_dat_high(struct _bytecode *result, struct _bytecode *next){
	pio_hw2wire_set_mask(pio, pio_state_machine, 1<<M_2WIRE_SDA, 1<<M_2WIRE_SDA);
}

void hw2wire_set_dat_low(struct _bytecode *result, struct _bytecode *next){
	pio_hw2wire_set_mask(pio, pio_state_machine, 1<<M_2WIRE_SDA, 0);
}

void hw2wire_read_bit(struct _bytecode *result, struct _bytecode *next){
	result->in_data=bio_get(M_2WIRE_SDA);
}	


void hw2wire_macro(uint32_t macro){
	
	typedef struct __attribute__((packed)) sle44xx_atr_struct {
		uint8_t structure_identifier:3;
		bool rfu1:1;
		uint8_t protocol_type:4;
		uint8_t data_units_bits:3;
		uint8_t data_units:4;
		bool read_with_defined_length:1;
		uint16_t rfu2:16;
	} sle44xx_atr_t;	

	uint32_t result=0;
	switch(macro)
	{
		case 0:		printf(" 1. ISO/IEC 7816-3 Answer to Reset\r\n");
				break;
		case 1:
				//>a.2 D:1 @.2 [ d:10 a.2 r:4
				// IO2 low
				bio_output(2);
				bio_put(2,0);
				// delay
				busy_wait_ms(1);
				// IO2 high
				bio_input(2);
				// clock tick
				pio_hw2wire_clock_tick(pio, pio_state_machine);
				// wait till end of clock tick
				busy_wait_us(50);
				// IO2 low
				bio_output(2);
				bio_put(2,0);
				// read 4 bytes (32 bits)
				pio_hw2wire_rx_enable(pio, pio_state_machine, true);
				uint32_t temp;
				uint8_t atr[4]; 
				printf("ATR: ");
				for(uint i =0; i<4; i++)
				{
					pio_hw2wire_get16(pio, pio_state_machine, &temp);
					ui_format_bitorder_manual(&temp, 8, 1);
					atr[i]=(uint8_t) temp;
					printf("0x%02x ", atr[i]);
				}
				printf("\r\n");	
				if(atr[0]==0x00 || atr[0]==0xFF)
				{ 
					result=1;
					break;
				}
				sle44xx_atr_t *atr_head;
    			atr_head = (sle44xx_atr_t *)&atr;
				//lets try to decode that
				printf("--SLE44xx decoder--\r\n");
				printf("Protocol Type: %s %d\r\n", (atr_head->protocol_type==0b1010?"S":"unknown"), atr_head->protocol_type);
				printf("Structure Identifier: %s\r\n", (atr_head->structure_identifier&0b11==0b000?"ISO Reserved": (atr_head->structure_identifier==0b010)?"General Purpose (Structure 1)":(atr_head->structure_identifier==0b110)?"Proprietary":"Special Application"));
				printf("Read: %s\r\n", (atr_head->read_with_defined_length?"Defined Length":"Read to end"));
				printf("Data Units: ");
				if(atr_head->data_units==0b0000) printf("Undefined\r\n");
				else printf("%.0f\r\n", pow(2,atr_head->data_units+6));
				printf("Data Units Bits: %.0f\r\n", pow(2, atr_head->data_units_bits));		

				break;
		default:	printf("%s\r\n", t[T_MODE_ERROR_MACRO_NOT_DEFINED]);
				system_config.error=1;
	}

	if(result)
	{
		printf("Device not found\r\n");
	}
}

void hw2wire_cleanup(void){
	pio_remove_program (pio, &hw2wire_program, pio_loaded_offset);
	bio_init();
	system_bio_claim(false, M_2WIRE_SDA, BP_PIN_MODE,0);
	system_bio_claim(false, M_2WIRE_SCL, BP_PIN_MODE,0);
	system_bio_claim(false, M_2WIRE_RST, BP_PIN_MODE,0);
}

/*void hw2wire_pins(void){
	printf("-\t-\tSCL\tSDA");
}*/

void hw2wire_settings(void){
	printf("HW2WIRE (speed)=(%d)", mode_config.baudrate_actual);
}

void hw2wire_printI2Cflags(void){
	uint32_t temp;
}

void hw2wire_help(void)
{
	printf("Muli-Master-multi-slave 2 wire protocol using a CLOCK and a bidirectional DATA\r\n");
	printf("line in opendrain configuration. Standard clock frequencies are 100KHz, 400KHz\r\n");
	printf("and 1MHz. Includes RST pin commonly used with 2 wire protocols, controlled with { & }.\r\n");
	#if 0
	printf("\r\n");
	printf("More info: https://en.wikipedia.org/wiki/I2C\r\n");
	printf("\r\n");
	printf("Electrical:\r\n");
	printf("\r\n");
	printf("BPCMD\t   { |            ADDRES(7bits+R/!W bit)             |\r\n");
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
	printf("\tGND\t------------------ GND\r\n");	
	#endif		
}


static uint8_t checkshort(void){
	uint8_t temp;
	temp=(bio_get(M_2WIRE_SDA)==0?1:0);
	temp|=(bio_get(M_2WIRE_SCL)==0?2:0);
	return (temp==3);			// there is only a short when both are 0 otherwise repeated start wont work
}
