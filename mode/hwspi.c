#include <stdio.h>
#include "pico/stdlib.h"
#include <stdint.h>
#include "hardware/spi.h"
#include "pirate.h"
#include "system_config.h"
#include "opt_args.h"
#include "bytecode.h"
#include "mode/hwspi.h"
#include "pirate/bio.h"
#include "ui/ui_prompt.h"
#include "ui/ui_term.h"
#include "pirate/storage.h"
#include "lib/sfud/inc/sfud.h"
#include "lib/sfud/inc/sfud_def.h"
#include "commands/spi/flash.h"
#include "ui/ui_help.h"
#include "pirate/hwspi.h"
#include "commands/spi/sniff.h"

// command configuration
const struct _command_struct hwspi_commands[]={   //Function Help
// note: for now the allow_hiz flag controls if the mode provides it's own help
    {"flash",0x00,&flash,T_HELP_CMD_FLASH}, // the help is shown in the -h *and* the list of mode apps
	{"sniff",0x00,&sniff_handler,T_HELP_CMD_FLASH}, // the help is shown in the -h *and* the list of mode apps
};
const uint32_t hwspi_commands_count=count_of(hwspi_commands);

static const char pin_labels[][5]={
	"CLK",
	"MOSI",
	"MISO",
	"CS"
};

static struct _spi_mode_config mode_config;

uint32_t spi_setup(void){
	uint32_t temp;
	// did the user leave us arguments?
	// baudrate
/*	if(cmdtail!=cmdhead) cmdtail=(cmdtail+1)&(CMDBUFFSIZE-1);
	consumewhitechars();
	br=getint();
	if((br>=1)&&(br<=7)) br<<=3;
		else system_config.error=1;

	// clock polarity
	if(cmdtail!=cmdhead) cmdtail=(cmdtail+1)&(CMDBUFFSIZE-1);
	consumewhitechars();
	spi_clock_polarity=getint()-1;
	if(spi_clock_polarity<=1) spi_clock_polarity<<=1;
		else system_config.error=1;

	// clock phase
	if(cmdtail!=cmdhead) cmdtail=(cmdtail+1)&(CMDBUFFSIZE-1);
	consumewhitechars();
	spi_clock_phase=getint()-1;
	if(spi_clock_phase<=1) spi_clock_phase=spi_clock_phase;
		else system_config.error=1;

	// cs behauviour
	if(cmdtail!=cmdhead) cmdtail=(cmdtail+1)&(CMDBUFFSIZE-1);
	consumewhitechars();
	csidle=getint()-1;
	if(csidle<=1) csidle=csidle;
		else system_config.error=1;
*/
	// did the user did it right?
	//if(system_config.error)			// go interactive 
	//{

		static const struct prompt_item spi_speed_menu[]={{T_HWSPI_SPEED_MENU_1}};
		static const struct prompt_item spi_bits_menu[]={{T_HWSPI_BITS_MENU_1}};		
		static const struct prompt_item spi_polarity_menu[]={{T_HWSPI_CLOCK_POLARITY_MENU_1},{T_HWSPI_CLOCK_POLARITY_MENU_2}};		
		static const struct prompt_item spi_phase_menu[]={{T_HWSPI_CLOCK_PHASE_MENU_1},{T_HWSPI_CLOCK_PHASE_MENU_2}};		
		static const struct prompt_item spi_idle_menu[]={{T_HWSPI_CS_IDLE_MENU_1},{T_HWSPI_CS_IDLE_MENU_2}};		

		static const struct ui_prompt spi_menu[]={
			{T_HWSPI_SPEED_MENU,spi_speed_menu,count_of(spi_speed_menu),T_HWSPI_SPEED_PROMPT, 1, 625000, 100, 	0,&prompt_int_cfg},
			{T_HWSPI_BITS_MENU,spi_bits_menu,count_of(spi_bits_menu),T_HWSPI_BITS_PROMPT,4, 8, 8,	0,&prompt_int_cfg},
			{T_HWSPI_CLOCK_POLARITY_MENU,spi_polarity_menu,count_of(spi_polarity_menu),T_HWSPI_CLOCK_POLARITY_PROMPT, 0, 0, 1,	0,&prompt_list_cfg},
			{T_HWSPI_CLOCK_PHASE_MENU,spi_phase_menu,count_of(spi_phase_menu),T_HWSPI_CLOCK_PHASE_PROMPT, 0, 0, 1,	0,&prompt_list_cfg},
			{T_HWSPI_CS_IDLE_MENU,spi_idle_menu,count_of(spi_idle_menu),T_HWSPI_CS_IDLE_PROMPT, 0, 0, 2,	0,&prompt_list_cfg}					
		};
		prompt_result result;

		const char config_file[]="bpspi.bp";

		struct _mode_config_t config_t[]={
			{"$.baudrate", &mode_config.baudrate},
			{"$.data_bits", &mode_config.data_bits},
			{"$.stop_bits", &mode_config.clock_polarity},
			{"$.parity", &mode_config.clock_phase},
			{"$.cs_idle", &mode_config.cs_idle}
		};

		if(storage_load_mode(config_file, config_t, count_of(config_t))){
			printf("\r\n\r\n%s%s%s\r\n", ui_term_color_info(), t[T_USE_PREVIOUS_SETTINGS], ui_term_color_reset());
			printf(" %s%s:%s %d kHz\r\n",ui_term_color_info(), t[T_HWSPI_SPEED_MENU], ui_term_color_reset(),mode_config.baudrate/1000);			
			printf(" %s%s:%s %d\r\n", ui_term_color_info(), t[T_HWSPI_BITS_MENU], ui_term_color_reset(), mode_config.data_bits);
			printf(" %s%s:%s %s\r\n", ui_term_color_info(), t[T_HWSPI_CLOCK_POLARITY_MENU], ui_term_color_reset(), t[spi_polarity_menu[mode_config.clock_polarity].description]);
			printf(" %s%s:%s %s\r\n", ui_term_color_info(), t[T_HWSPI_CLOCK_PHASE_MENU], ui_term_color_reset(), t[spi_phase_menu[mode_config.clock_phase].description]);
			printf(" %s%s:%s %s\r\n", ui_term_color_info(), t[T_HWSPI_CS_IDLE_MENU], ui_term_color_reset(), t[spi_idle_menu[mode_config.cs_idle].description]);

			bool user_value;
			if(!ui_prompt_bool(&result, true, true, true, &user_value)) return 0;		
			if(user_value) return 1; //user said yes, use the saved settings
		}

        ui_prompt_uint32(&result, &spi_menu[0], &temp);
		if(result.exit) return 0;
		mode_config.baudrate=temp*1000;
		
        ui_prompt_uint32(&result, &spi_menu[1], &temp);
		if(result.exit) return 0;
		mode_config.data_bits=(uint8_t)temp;
        system_config.num_bits=(uint8_t)temp;
		
        ui_prompt_uint32(&result, &spi_menu[2], &temp);
		if(result.exit) return 0;
		mode_config.clock_polarity=(uint8_t)((temp-1)<<1);

		ui_prompt_uint32(&result, &spi_menu[3], &temp);
		if(result.exit) return 0;
		mode_config.clock_phase=(uint8_t)(temp-1);

		ui_prompt_uint32(&result, &spi_menu[4], &temp);
		if(result.exit) return 0;
		mode_config.cs_idle=(uint8_t)(temp-1);

	storage_save_mode(config_file, config_t, count_of(config_t));
	//}
	
	return 1;
}

uint32_t spi_setup_exc(void){		
	//setup spi
	mode_config.read_with_write=false;
	mode_config.baudrate_actual=spi_init(M_SPI_PORT, mode_config.baudrate);
	printf("\r\n%s%s:%s %ukHz",ui_term_color_notice(), t[T_HWSPI_ACTUAL_SPEED_KHZ], ui_term_color_reset(),mode_config.baudrate_actual/1000);
	hwspi_init(mode_config.data_bits, mode_config.clock_polarity, mode_config.clock_phase);
	system_bio_claim(true, M_SPI_CLK, BP_PIN_MODE, pin_labels[0]);
	system_bio_claim(true, M_SPI_CDO, BP_PIN_MODE, pin_labels[1]);
	system_bio_claim(true, M_SPI_CDI, BP_PIN_MODE, pin_labels[2]);
	system_bio_claim(true, M_SPI_CS, BP_PIN_MODE, pin_labels[3]);
}

void spi_cleanup(void){
	// disable peripheral
	hwspi_deinit();
	//release pin claims
	system_bio_claim(false, M_SPI_CLK, BP_PIN_MODE,0);
	system_bio_claim(false, M_SPI_CDO, BP_PIN_MODE,0);
	system_bio_claim(false, M_SPI_CDI, BP_PIN_MODE,0);
	system_bio_claim(false, M_SPI_CS, BP_PIN_MODE,0);	
	// update system_config pins
	system_config.misoport=0;
	system_config.mosiport=0;
	system_config.csport=0;
	system_config.clkport=0;
	system_config.misopin=0;
	system_config.mosipin=0;
	system_config.cspin=0;
	system_config.clkpin=0;
}

void spi_set_cs(uint8_t cs){
	if(cs==M_SPI_SELECT){ // 'start'
		if(mode_config.cs_idle) hwspi_select();
		else hwspi_deselect();
	}else{			// 'stop' 
		if(mode_config.cs_idle) hwspi_deselect();
		else hwspi_select();
	}
}

void spi_start(bytecode_t *result, bytecode_t *next){
	mode_config.read_with_write=false;
	result->data_message=t[T_HWSPI_CS_SELECT];
	spi_set_cs(M_SPI_SELECT);
}

void spi_startr(bytecode_t *result, bytecode_t *next){
	mode_config.read_with_write=true;
	result->data_message=t[T_HWSPI_CS_SELECT];
	spi_set_cs(M_SPI_SELECT);
}

void spi_stop(bytecode_t *result, bytecode_t *next){
	mode_config.read_with_write=false;
	result->data_message=t[T_HWSPI_CS_DESELECT];
	spi_set_cs(M_SPI_DESELECT);
}

void spi_stopr(bytecode_t *result, bytecode_t *next){
	mode_config.read_with_write=false;
	result->data_message=t[T_HWSPI_CS_DESELECT];
	spi_set_cs(M_SPI_DESELECT);
}

void spi_write(bytecode_t *result, bytecode_t *next){
	//hwspi_write((uint32_t)result->out_data);
	result->in_data=hwspi_write_read((uint8_t)result->out_data);
	result->read_with_write=mode_config.read_with_write;
}

void spi_read(bytecode_t *result, bytecode_t *next){
	result->in_data = (uint8_t)hwspi_read();
}

void spi_macro(uint32_t macro){
	switch(macro){
		case 0:		printf(" 0. This menu\r\n");
				break;
		case 1:	break;
		case 3: ui_term_detect(); break;
		default:	printf("%s\r\n", t[T_MODE_ERROR_MACRO_NOT_DEFINED]);
				system_config.error=1;
	}
}
/*
void spi_pins(void)
{
	printf("CS\tMISO\tCLK\tMOSI");
}
*/

void spi_settings(void){
	printf("spi (baudrate, clock polarity, clock phase, cs)=(%dkHz, %d, %d, %d)", (mode_config.baudrate_actual/1000), (mode_config.clock_polarity>>1)+1, mode_config.clock_phase+1, mode_config.cs_idle+1);
}

void spi_printSPIflags(void){
	uint32_t temp;
}

void spi_help(void){
	printf("Peer to peer 3 or 4 wire full duplex protocol. Very\r\n");
	printf("high clockrates upto 20MHz are possible.\r\n");
	printf("\r\n");
	printf("More info: https://en.wikipedia.org/wiki/Serial_Peripheral_Interface_Bus\r\n");
	printf("\r\n");


	printf("BPCMD\t {,] |                 DATA (1..32bit)               | },]\r\n");
	printf("CMD\tSTART| D7  | D6  | D5  | D4  | D3  | D2  | D1  | D0  | STOP\r\n");

	if(mode_config.clock_phase)
	{	
		printf("MISO\t-----|{###}|{###}|{###}|{###}|{###}|{###}|{###}|{###}|------\r\n");
		printf("MOSI\t-----|{###}|{###}|{###}|{###}|{###}|{###}|{###}|{###}|------\r\n");
	}
	else
	{
		printf("MISO\t---{#|##}{#|##}{#|##}{#|##}{#|##}{#|##}{#|##}{#|##}--|------\r\n");
		printf("MOSI\t---{#|##}{#|##}{#|##}{#|##}{#|##}{#|##}{#|##}{#|##}--|------\r\n");
	}

	if(mode_config.clock_polarity>>1)
		printf("CLK     \"\"\"\"\"|\"\"__\"|\"\"__\"|\"\"__\"|\"\"__\"|\"\"__\"|\"\"__\"|\"\"__\"|\"\"__\"|\"\"\"\"\"\"\r\n");
	else
		printf("CLK\t_____|__\"\"_|__\"\"_|__\"\"_|__\"\"_|__\"\"_|__\"\"_|__\"\"_|__\"\"_|______\r\n");

	if(mode_config.cs_idle)
		printf("CS\t\"\"___|_____|_____|_____|_____|_____|_____|_____|_____|___\"\"\"\r\n");
	else
		printf("CS\t__\"\"\"|\"\"\"\"\"|\"\"\"\"\"|\"\"\"\"\"|\"\"\"\"\"|\"\"\"\"\"|\"\"\"\"\"|\"\"\"\"\"|\"\"\"\"\"|\"\"\"___\r\n");

	printf("\r\nCurrent mode is spi_clock_phase=%d and spi_clock_polarity=%d\r\n",mode_config.clock_phase, mode_config.clock_polarity>>1);
	printf("\r\n");
	printf("Connection:\r\n");
	printf("\tMOSI \t------------------ MOSI\r\n");
	printf("\tMISO \t------------------ MISO\r\n");
	printf("{BP}\tCLK\t------------------ CLK\t{DUT}\r\n");
	printf("\tCS\t------------------ CS\r\n");
	printf("\tGND\t------------------ GND\r\n");

	ui_help_mode_commands(hwspi_commands, hwspi_commands_count);
}

