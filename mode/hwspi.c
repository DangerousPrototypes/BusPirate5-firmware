#include <stdio.h>
#include "pico/stdlib.h"
#include <stdint.h>
#include "hardware/spi.h"
#include "pirate.h"
#include "system_config.h"
#include "opt_args.h"
#include "bytecode.h"
#include "mode/hwspi.h"
#include "bio.h"
#include "ui/ui_prompt.h"
#include "ui/ui_term.h"
#include "storage.h"
#include "mode/spiflash.h"

#define M_SPI_PORT spi1
#define M_SPI_CLK BIO6
#define M_SPI_CDO BIO7
#define M_SPI_CDI BIO4
#define M_SPI_CS BIO5

#define M_SPI_SELECT 0
#define M_SPI_DESELECT 1

static const char pin_labels[][5]={
	"SCLK",
	"CDO",
	"CDI",
	"CS"
};

static struct _spi_mode_config mode_config;

uint32_t spi_setup(void)
{
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

		if(storage_load_mode(config_file, config_t, count_of(config_t)))
		{
			printf("\r\n\r\n%s%s%s\r\n", ui_term_color_info(), t[T_USE_PREVIOUS_SETTINGS], ui_term_color_reset());
			printf(" %s%s:%s %d KHz\r\n",ui_term_color_info(), t[T_HWSPI_SPEED_MENU], ui_term_color_reset(),mode_config.baudrate/1000);			
			printf(" %s%s:%s %d\r\n", ui_term_color_info(), t[T_HWSPI_BITS_MENU], ui_term_color_reset(), mode_config.data_bits);
			printf(" %s%s:%s %s\r\n", ui_term_color_info(), t[T_HWSPI_CLOCK_POLARITY_MENU], ui_term_color_reset(), t[spi_polarity_menu[mode_config.clock_polarity].description]);
			printf(" %s%s:%s %s\r\n", ui_term_color_info(), t[T_HWSPI_CLOCK_PHASE_MENU], ui_term_color_reset(), t[spi_phase_menu[mode_config.clock_phase].description]);
			//printf(" %s%s:%s %s\r\ny/n>", ui_term_color_info(), t[T_HWSPI_CS_IDLE_MENU], ui_term_color_reset(), t[spi_idle_menu[mode_config.cs_idle].description]);

			bool user_value;
			if(!ui_prompt_bool(&result, true, true, true, &user_value))
			{
				return 0;
			}
			
			if(user_value)
			{
				return 1; //user said yes, use the saved settings
			}
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

uint32_t spi_setup_exc(void)
{		
	//setup spi
	mode_config.baudrate_actual=spi_init(M_SPI_PORT, mode_config.baudrate);
	printf("\r\n%s%s:%s %uKHz",ui_term_color_notice(), t[T_HWSPI_ACTUAL_SPEED_KHZ], ui_term_color_reset(),mode_config.baudrate_actual/1000);
	spi_set_format(M_SPI_PORT,mode_config.data_bits, mode_config.clock_polarity, mode_config.clock_phase, SPI_MSB_FIRST);
	
	//set buffers to correct position
	bio_buf_output(M_SPI_CLK); //sck
	bio_buf_output(M_SPI_CDO); //tx
	bio_buf_input(M_SPI_CDI); //rx

	gpio_put(bio2bufiopin[M_SPI_CDO], 1);
	//assign spi functon to io pins
	bio_set_function(M_SPI_CLK, GPIO_FUNC_SPI); //sck
	bio_set_function(M_SPI_CDO, GPIO_FUNC_SPI); //tx
	bio_set_function(M_SPI_CDI, GPIO_FUNC_SPI); //rx

	//cs
	bio_set_function(M_SPI_CS, GPIO_FUNC_SIO);
	bio_output(M_SPI_CS);
	spi_set_cs(M_SPI_DESELECT);

	// 8bit and lsb/msb handled in UI.c
	//dff=SPI_CR1_DFF_8BIT;
	//lsbfirst=SPI_CR1_MSBFIRST;
	system_bio_claim(true, M_SPI_CLK, BP_PIN_MODE, pin_labels[0]);
	system_bio_claim(true, M_SPI_CDO, BP_PIN_MODE, pin_labels[1]);
	system_bio_claim(true, M_SPI_CDI, BP_PIN_MODE, pin_labels[2]);
	system_bio_claim(true, M_SPI_CS, BP_PIN_MODE, pin_labels[3]);
	// update system_config pins
	/*
	system_config.misoport=BP_SPI_MISO_PORT;
	system_config.mosiport=BP_SPI_MOSI_PORT;
	system_config.csport=BP_SPI_CS_PORT;
	system_config.clkport=BP_BP_SPI_CLK_PORT; 
	system_config.misopin=BP_SPI_MISO_PIN;
	system_config.mosipin=BP_SPI_MOSI_PIN;
	system_config.cspin=BP_SPI_CS_PIN;
	system_config.clkpin=BP_BP_SPI_CLK_PIN;
	*/

}

void spi_cleanup(void)
{
	// disable peripheral
	spi_deinit(M_SPI_PORT);

	system_bio_claim(false, M_SPI_CLK, BP_PIN_MODE,0);
	system_bio_claim(false, M_SPI_CDO, BP_PIN_MODE,0);
	system_bio_claim(false, M_SPI_CDI, BP_PIN_MODE,0);
	system_bio_claim(false, M_SPI_CS, BP_PIN_MODE,0);	

	// reset all pins to safe mode (done before mode change, but we do it here to be safe)
	bio_init();

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

void spi_set_cs(uint8_t cs)
{

	if(cs==M_SPI_SELECT) // 'start'
	{
		if(mode_config.cs_idle) bio_put(M_SPI_CS, 0);
			else bio_put(M_SPI_CS, 1);
	}
	else			// 'stop' 
	{
		if(mode_config.cs_idle) bio_put(M_SPI_CS, 1);
			else bio_put(M_SPI_CS, 0);
	}
}

uint8_t spi_xfer(const uint8_t out)
{
	uint8_t spi_in;
	spi_write_read_blocking(M_SPI_PORT, &out,&spi_in, 1);
	return spi_in;
}

void spi_start(struct _bytecode *result, struct _bytecode *next)
{
	result->data_message=t[T_HWSPI_CS_SELECT];
	spi_set_cs(M_SPI_SELECT);
}

void spi_startr(struct _bytecode *result, struct _bytecode *next)
{
	//printf(t[T_HWSPI_CS_SELECT], !mode_config.cs_idle);
	spi_set_cs(M_SPI_SELECT);
}

void spi_stop(struct _bytecode *result, struct _bytecode *next)
{
	result->data_message=t[T_HWSPI_CS_DESELECT];
	spi_set_cs(M_SPI_DESELECT);
}

void spi_stopr(struct _bytecode *result, struct _bytecode *next)
{
	//printf(t[T_HWSPI_CS_DESELECT], mode_config.cs_idle);
	spi_set_cs(M_SPI_DESELECT);
}

void spi_write(struct _bytecode *result, struct _bytecode *next)
{

/*	if((system_config.num_bits<4)||(system_config.num_bits>8))
	{
		printf("\r\nOnly 4 to 8 bits are currently allowed");
		system_config.error=1;
		return (uint16_t)0x00;
	}

	//TODO: lsb ??
	//TODO: on the fly num_bits from current command
	//if(system_config.num_bits==8) spi_set_dff_8bit(BP_SPI);			// is there a less overhead way of doing this?
	//if(system_config.num_bits==16) spi_set_dff_16bit(BP_SPI);

	returnval=spi_xfer((uint16_t)d);

	return (uint16_t) returnval;
	*/
    // Write to TX FIFO whilst ignoring RX, then clean up afterward. When RX
    // is full, PL022 inhibits RX pushes, and sets a sticky flag on
    // push-on-full, but continues shifting. Safe if SSPIMSC_RORIM is not set.
	while(!spi_is_writable(M_SPI_PORT))
	{
		tight_loop_contents();
	}

	spi_get_hw(M_SPI_PORT)->dr = (uint32_t)result->out_data;

    // Drain RX FIFO, then wait for shifting to finish (which may be *after*
    // TX FIFO drains), then drain RX FIFO again
    while(spi_is_readable(M_SPI_PORT))
	{
        (void)spi_get_hw(M_SPI_PORT)->dr;
	}

    while(spi_get_hw(M_SPI_PORT)->sr & SPI_SSPSR_BSY_BITS)
	{
        tight_loop_contents();
	}

    while(spi_is_readable(M_SPI_PORT))
	{
        (void)spi_get_hw(M_SPI_PORT)->dr;
	}

    // Don't leave overrun flag set
    spi_get_hw(M_SPI_PORT)->icr = SPI_SSPICR_RORIC_BITS;
}

void spi_read(struct _bytecode *result, struct _bytecode *next)
{

	while(!spi_is_writable(M_SPI_PORT));
	
	spi_get_hw(M_SPI_PORT)->dr = 0xff; //(uint32_t)result->out_data;

    while(!spi_is_readable(M_SPI_PORT));
	
	result->in_data = (uint8_t)spi_get_hw(M_SPI_PORT)->dr;
}

void spi_macro(uint32_t macro)
{
	switch(macro)
	{
		case 0:		printf(" 0. This menu\r\n 1. Query flash chip ID\r\n");
				break;
		case 1:	flash_probe();
				break;
		case 2: sfud_test(); break;
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

void spi_settings(void)
{
	printf("spi (baudrate, clock polarity, clock phase, cs)=(%dKHz, %d, %d, %d)", (mode_config.baudrate_actual/1000), (mode_config.clock_polarity>>1)+1, mode_config.clock_phase+1, mode_config.cs_idle+1);
}

void spi_printSPIflags(void)
{
	uint32_t temp;

/*	temp=SPI_SR(BP_SPI);

	if(temp&SPI_SR_BSY) printf(" BSY");
	if(temp&SPI_SR_OVR) printf(" OVR");
	if(temp&SPI_SR_MODF) printf(" MODF");
	if(temp&SPI_SR_CRCERR) printf(" CRCERR");
	if(temp&SPI_SR_UDR) printf(" USR");
	if(temp&SPI_SR_CHSIDE) printf(" CHSIDE");
//	if(temp&SPI_SR_TXE) printf(" TXE");
//	if(temp&SPI_SR_RXNE) printf(" RXNE");
*/

}

void spi_help(void)
{
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
}


// helpers for binmode and other protocols
/*
void spi_setspi_clock_polarity(uint32_t val)
{
	spi_clock_polarity=val;
}

void spi_setspi_clock_phase(uint32_t val)
{
	spi_clock_phase=val;
}

void spi_setbr(uint32_t val)
{
	br=val;
}

void spi_setdff(uint32_t val)
{
	dff=val;
}

void spi_setlsbfirst(uint32_t val)
{
	lsbfirst=val;
}

void spi_set_cs_idle(uint32_t val)
{
	csidle=val;
}
*/


void spi_write_simple(uint32_t data)
{
    // Write to TX FIFO whilst ignoring RX, then clean up afterward. When RX
    // is full, PL022 inhibits RX pushes, and sets a sticky flag on
    // push-on-full, but continues shifting. Safe if SSPIMSC_RORIM is not set.
	while(!spi_is_writable(M_SPI_PORT))
	{
		tight_loop_contents();
	}

	spi_get_hw(M_SPI_PORT)->dr = (uint32_t)data;

    // Drain RX FIFO, then wait for shifting to finish (which may be *after*
    // TX FIFO drains), then drain RX FIFO again
    while(spi_is_readable(M_SPI_PORT))
	{
        (void)spi_get_hw(M_SPI_PORT)->dr;
	}

    while(spi_get_hw(M_SPI_PORT)->sr & SPI_SSPSR_BSY_BITS)
	{
        tight_loop_contents();
	}

    while(spi_is_readable(M_SPI_PORT))
	{
        (void)spi_get_hw(M_SPI_PORT)->dr;
	}

    // Don't leave overrun flag set
    spi_get_hw(M_SPI_PORT)->icr = SPI_SSPICR_RORIC_BITS;
}

uint32_t spi_read_simple(void)
{
	while(!spi_is_writable(M_SPI_PORT));
	
	spi_get_hw(M_SPI_PORT)->dr = (uint32_t)0xff;

    while(!spi_is_readable(M_SPI_PORT));
	
	return (uint8_t)spi_get_hw(M_SPI_PORT)->dr;
}

