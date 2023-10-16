//TODO: add timeout to all I2C stuff that can hang!
#include <stdio.h>
#include "pico/stdlib.h"
#include <stdint.h>
#include "pirate.h"
#include "system_config.h"
#include "opt_args.h"
#include "mode/hwled.h"
#include "bio.h"
#include "ui/ui_prompt.h"
#include "hardware/pio.h"
#include "ws2812.pio.h"
#include "apa102.pio.h"
#include "rgb.h"
#include "storage.h"
#include "ui/ui_term.h"

#define M_LED_PIO pio1
#define M_LED_SDO BIO0
#define M_LED_SCL BIO1 //only used on APA102
static const char pin_labels[][5]={
	"SDO",
	"SCL",
};

static const char led_device_type[][7]={
	"WS2812",
	"APA102",
};

enum M_LED_DEVICE_TYPE{
	M_LED_WS2812,
	M_LED_APA102,
    M_LED_WS2812_ONBOARD	
};

static struct _led_mode_config mode_config;

static PIO pio = M_LED_PIO;
static uint pio_state_machine = 0;
static uint pio_loaded_offset;

uint32_t HWLED_setup(void)
{
	// speed
	/*if(cmdtail!=cmdhead) cmdtail=(cmdtail+1)&(CMDBUFFSIZE-1);
	consumewhitechars();
	speed=getint();
	if((speed>0)&&(speed<=2)) speed-=1;
	else modeConfig.error=1;
*/
	// did the user did it right?
	//if(modeConfig.error)			// go interactive 
	//{
		static const struct prompt_item leds_type_menu[]={{T_HWLED_DEVICE_MENU_1},{T_HWLED_DEVICE_MENU_2},{T_HWLED_DEVICE_MENU_3}};
		static const struct prompt_item leds_num_menu[]={{T_HWLED_NUM_LEDS_MENU_1}};		

		static const struct ui_prompt leds_menu[]={
			{T_HWLED_DEVICE_MENU, leds_type_menu, count_of(leds_type_menu),T_HWLED_DEVICE_PROMPT, 0,0,1, 	0,&prompt_list_cfg},
			{T_HWLED_NUM_LEDS_MENU,leds_num_menu,count_of(leds_num_menu),T_HWLED_NUM_LEDS_PROMPT, 1, 10000, 1,	0,&prompt_int_cfg}
		};
		prompt_result result;

		const char config_file[]="bpled.bp";

		struct _mode_config_t config_t[]={
			{"$.device", &mode_config.device},
			{"$.num_leds", &mode_config.num_leds}
		};

		if(storage_load_mode(config_file, config_t, count_of(config_t)))
		{
			uint32_t temp;

			printf("\r\n\r\n%s%s%s\r\n", ui_term_color_info(), t[T_USE_PREVIOUS_SETTINGS], ui_term_color_reset());
			printf(" %s: %s\r\n", t[T_HWLED_DEVICE_MENU], t[leds_type_menu[mode_config.device].description]);			
			printf(" %s: %d\r\ny/n> ", t[T_HWLED_NUM_LEDS_MENU], mode_config.num_leds);
			do{
				temp=ui_prompt_yes_no();
			}while(temp>1);

			printf("\r\n");

			if(temp==1) return 1;
		}		



        ui_prompt_uint32(&result, &leds_menu[0], &mode_config.device);
		if(result.exit) return 0;
        mode_config.device--;
        if(mode_config.device==2)
        {
            mode_config.num_leds=RGB_LEN;
        }
        else
        {
            ui_prompt_uint32(&result, &leds_menu[1], &mode_config.num_leds);
            if(result.exit) return 0;
        }

		storage_save_mode(config_file, config_t, count_of(config_t));

	//}
	return 1;
}

uint32_t HWLED_setup_exc(void)
{
	switch(mode_config.device){
		case M_LED_WS2812:
			bio_buf_output(M_LED_SDO);
			pio_loaded_offset = pio_add_program(pio, &ws2812_program);
    		ws2812_program_init(pio, pio_state_machine, pio_loaded_offset, bio2bufiopin[M_LED_SDO], 800000, false);
			system_bio_claim(true, M_LED_SDO, BP_PIN_MODE, pin_labels[0]);
			break;
		case M_LED_APA102:
			bio_buf_output(M_LED_SDO);
			bio_buf_output(M_LED_SCL);		
			pio_loaded_offset = pio_add_program(pio, &apa102_mini_program);
			#define SERIAL_FREQ (5 * 1000 * 1000)
    		apa102_mini_program_init(pio, pio_state_machine, pio_loaded_offset, SERIAL_FREQ, bio2bufiopin[M_LED_SCL], bio2bufiopin[M_LED_SDO]);
			system_bio_claim(true, M_LED_SDO, BP_PIN_MODE, pin_labels[0]);
			system_bio_claim(true, M_LED_SCL, BP_PIN_MODE, pin_labels[1]);
			break;
        case M_LED_WS2812_ONBOARD: //internal LEDs, stop any in-progress stuff
			rgb_irq_enable(false);
            break;
		default:
			printf("\r\nError: Invalid device type");
			return 0;

	}
	system_config.subprotocol_name=led_device_type[mode_config.device];
}


void HWLED_start(void)
{
	uint8_t timeout;

}

void HWLED_stop(void)
{
	uint8_t timeout;

}

uint32_t HWLED_send(uint32_t d)
{
	uint32_t temp;

	switch(mode_config.device){
		case M_LED_WS2812:
			//for(int i=0; i<RGB_LEN; i++){
				pio_sm_put_blocking(M_LED_PIO, pio_state_machine, (d << 8u));        
			//}
			break;
		case M_LED_APA102:
			pio_sm_put_blocking(M_LED_PIO, pio_state_machine, d);     
			break;
        case M_LED_WS2812_ONBOARD: //internal LEDs, stop any in-progress stuff
            rgb_put(d);
            break;
		default:
			printf("Error: Invalid device type");
			return 0;

	}
	return 0;
}

uint32_t HWLED_read(uint8_t next_command)
{
	uint32_t returnval;
	uint8_t timeout;
	return returnval;
}

void HWLED_macro(uint32_t macro)
{
	switch(macro)
	{
		case 0:		printf(" 1. LED strip counter\r\n");
					printf(" 2. LEDs per meter counter\r\n");
				break;
		case 1:		printf("Macro not available");
				break;
		case 2:		printf("Macro not available");
				break;
		default:	printf("Macro not defined");
				system_config.error=1;
	}
}

void HWLED_cleanup(void)
{
	switch(mode_config.device){
		case M_LED_WS2812:
			pio_remove_program(pio, &ws2812_program, pio_loaded_offset);
			break;
		case M_LED_APA102:
			pio_remove_program(pio, &apa102_mini_program, pio_loaded_offset);
			break;
		case M_LED_WS2812_ONBOARD:
			rgb_irq_enable(true);
			break;
	}
	
	//pio_clear_instruction_memory(pio);
	system_config.subprotocol_name=0x00;
	system_bio_claim(false, M_LED_SDO, BP_PIN_MODE,0);
	system_bio_claim(false, M_LED_SCL, BP_PIN_MODE,0);


	bio_init();
}

void HWLED_settings(void)
{
	//printf("HWI2C (speed)=(%d)", mode_config.baudrate_actual);
}

void HWLED_help(void)
{
	/*printf("Muli-Master-multi-slave 2 wire protocol using a CLOCK and a bidirectional DATA\r\n");
	printf("line in opendrain mode_configuration. Standard clock frequencies are 100KHz, 400KHz\r\n");
	printf("and 1MHz.\r\n");
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
	printf("\tGND\t------------------ GND\r\n");*/			
}
