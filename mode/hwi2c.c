//TODO: add timeout to all I2C stuff that can hang!
#include <stdio.h>
#include "pico/stdlib.h"
#include <stdint.h>
#include "pirate.h"
#include "system_config.h"
#include "mode/hwi2c.h"
#include "bio.h"
#include "ui/ui_prompt.h"
#include "i2c.pio.h"
#include "pio_i2c.h"
#include "storage.h"
#include "ui/ui_term.h"

#define M_I2C_PIO pio1
#define M_I2C_SDA BIO0
#define M_I2C_SCL BIO1


static const char pin_labels[][5]={
	"SDA",
	"SCL",
};

static struct _i2c_mode_config mode_config;

static PIO pio = M_I2C_PIO;
static uint pio_state_machine = 0;
static uint pio_loaded_offset;

static uint8_t checkshort(void);
static void I2Csearch(void);

uint32_t HWI2C_setup(void)
{
	uint32_t temp;
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

		if(storage_load_mode(config_file, config_t, count_of(config_t)))
		{
			printf("\r\n\r\n%s%s%s\r\n", ui_term_color_info(), t[T_USE_PREVIOUS_SETTINGS], ui_term_color_reset());
			printf(" %s: %dKHz\r\n", t[T_HWI2C_SPEED_MENU], mode_config.baudrate);			
			printf(" %s: %s\r\ny/n> ", t[T_HWI2C_DATA_BITS_MENU], t[i2c_data_bits_menu[mode_config.data_bits].description]);
			do{
				temp=ui_prompt_yes_no();
			}while(temp>1);

			printf("\r\n");

			if(temp==1) return 1;
		}

		prompt_result result;
        ui_prompt_uint32(&result, &i2c_menu[0], &mode_config.baudrate);
		if(result.exit) return 0;
		ui_prompt_uint32(&result, &i2c_menu[1], &temp);
		if(result.exit) return 0;
		mode_config.data_bits=(uint8_t)temp-1;

		storage_save_mode(config_file, config_t, count_of(config_t));


		
	//}

	return 1;

}

uint32_t HWI2C_setup_exc(void)
{
	pio_loaded_offset = pio_add_program(pio, &i2c_program);
    i2c_program_init(pio, pio_state_machine, pio_loaded_offset, bio2bufiopin[BIO0], bio2bufiopin[BIO1], bio2bufdirpin[BIO0], bio2bufdirpin[BIO1]);

	system_bio_claim(true, M_I2C_SDA, BP_PIN_MODE, pin_labels[0]);
	system_bio_claim(true, M_I2C_SCL, BP_PIN_MODE, pin_labels[1]);
	/*
	// update modeConfig pins
	modeConfig.mosiport=BP_I2C_SDA_PORT;
	modeConfig.clkport=BP_I2C_SDA_PORT;
	modeConfig.mosipin=BP_I2C_SDA_PIN;
	modeConfig.clkpin=BP_I2C_SDA_PIN;

	modeConfig.logicanalyzerperiod=LA_period[speed];
	*/
	mode_config.start_sent=false;
	return 1;
}


void HWI2C_start(void)
{
	uint8_t timeout;

/*	if(checkshort())
	{
		printf("no pullup or short");
		modeConfig.error=1;
		return;
	}

	printf("I2C START");
	i2c_send_start(BP_I2C);

	timeout=100; // 1 us enough?

	// wait for start (SB), switched to master (MSL) and a taken bus (BUSY)
	while ((!((I2C_SR1(BP_I2C)&I2C_SR1_SB)&&(I2C_SR2(BP_I2C)&(I2C_SR2_MSL|I2C_SR2_BUSY))))&&timeout) { timeout--; delayus(1); }

	if(timeout==0)
	{
		printf(" TIMEOUT");
		modeConfig.error=1;
	}
	*/
	pio_i2c_start(pio, pio_state_machine);
	mode_config.start_sent=true;
	//HWI2C_send(0b10100000);
	//HWI2C_send(0);
	//pio_i2c_stop(pio, pio_state_machine);

	
//void pio_i2c_repstart(PIO pio, uint sm);
}

void HWI2C_start_post(void)
{
	printf("I2C START");
}

void HWI2C_stop_post(void)
{
	printf("I2C STOP");
}

void HWI2C_stop(void)
{
	//uint8_t timeout;

	/*if(!(I2C_SR2(BP_I2C)&I2C_SR2_TRA))
	{
		printf("!!WARNING: two extra bytes read!!");

	//	i2c_nack_current(BP_I2C);
		i2c_disable_ack(BP_I2C);

		timeout=100; // 100 us enough?

		// two bytes are buffered :( 
		while ((!(I2C_SR1(BP_I2C) & I2C_SR1_RxNE))&&timeout) { timeout--; delayus(1); }		// wait until data available
		(void)i2c_get_data(BP_I2C);

		if(timeout==0) printf(" TIMEOUT");

		timeout=100;

		while ((!(I2C_SR1(BP_I2C) & I2C_SR1_RxNE))&&timeout) { timeout--; delayus(1); }		// wait until data available
		(void)i2c_get_data(BP_I2C);

		if(timeout==0) printf(" TIMEOUT");

		printf("\r\n");
	}


	i2c_send_stop(BP_I2C);
	*/
	/*uint32_t timeout=10000;
    while(pio_sm_is_tx_fifo_full(pio, pio_state_machine)) 
	{
		timeout--;
		if(!timeout)
		{
			printf("stop timeout\r\n");
			pio_i2c_resume_after_error(pio, pio_state_machine);
			return; // 0xffff;
		}
    }*/



	pio_i2c_stop(pio, pio_state_machine);
}

uint32_t HWI2C_send(uint32_t d)
{
	uint8_t ack, timeout;
	uint32_t temp;

//	HWI2C_printI2Cflags();

/*	if(I2C_SR2(BP_I2C)&I2C_SR2_MSL)					// we can only send if master! (please issue a start condition frist)
	{
		if((I2C_SR1(BP_I2C)&I2C_SR1_SB)||(I2C_SR2(BP_I2C)&I2C_SR2_TRA))	// writing is only enable after start or during transmisson
		{
			temp=(I2C_SR1(BP_I2C)&I2C_SR1_SB); 		// gets destroyed by writing?

			i2c_send_data(BP_I2C, d);

//			HWI2C_printI2Cflags();

			timeout=100;	// 100us enough?

			if (temp) while(((!(I2C_SR1(BP_I2C)&I2C_SR1_ADDR))&&(!(I2C_SR1(BP_I2C)&I2C_SR1_AF)))&&timeout) { timeout--; delayus(1); } // or BTF??
			else while(((!(I2C_SR1(BP_I2C)&I2C_SR1_BTF))&&(!(I2C_SR1(BP_I2C)&I2C_SR1_AF)))&&timeout) { timeout--; delayus(1); }

//			HWI2C_printI2Cflags();

//			if(I2C_SR2(BP_I2C)&I2C_SR2_MSL)			// are we mastah??
			ack=!(I2C_SR1(BP_I2C)&I2C_SR1_AF);		// no ack error is ack
//			else
//				ack=0;		
	
			temp=I2C_SR1(BP_I2C);				// need to read both status registers??
			temp=I2C_SR2(BP_I2C);
			I2C_SR1(BP_I2C)=0;				// clear all errors/flags
			I2C_SR2(BP_I2C)=0;				// clear all errors/flags

			printf(" %s", (ack?"ACK":"NACK"));
		}
		else
		{
			printf("Not allowed to send (wrong address)");
			modeConfig.error=1;
			ack=0;
		}
	}
	else
	{
		printf("Not allowed to send (START)");
		modeConfig.error=1;
		ack=0;
	}
*/

    int err = 0;

	//if a start was just sent, determine if this is a read or write address
	// and configure the PIO I2C
	if(mode_config.start_sent)
	{
		pio_i2c_rx_enable(pio, pio_state_machine, (d&0b1));
		mode_config.start_sent=false;
	}

    while(pio_sm_is_tx_fifo_full(pio, pio_state_machine));

	pio_i2c_put_or_err(pio, pio_state_machine, (d << 1)|(1u));

    pio_i2c_wait_idle(pio, pio_state_machine);
    if (pio_i2c_check_error(pio, pio_state_machine)) {
        err = -1;
        pio_i2c_resume_after_error(pio, pio_state_machine);
		printf("I2C Error");
    }
    return err;

}

uint32_t HWI2C_read(uint8_t next_command)
{
	uint32_t returnval;
	uint8_t timeout;

/*	if(!(I2C_SR2(BP_I2C)&I2C_SR2_TRA))
	{
		i2c_enable_ack(BP_I2C);				// TODO: clever way to nack last byte as per spec

		timeout=100; // 100us enough?

		while ((!(I2C_SR1(BP_I2C) & I2C_SR1_RxNE))&&timeout) { timeout--; delayus(1); };		// wait until data available

		if(timeout==0) printf(" TIMEOUT");

		returnval=i2c_get_data(BP_I2C);
	}
	else
	{
		printf("Not allowed to read (wrong address)");
		modeConfig.error=1;
		returnval=0;
	}
*/

   int err = 0;
	while(pio_sm_is_tx_fifo_full(pio, pio_state_machine));
    
	while(!pio_sm_is_rx_fifo_empty(pio, pio_state_machine))
        (void)pio_i2c_get(pio, pio_state_machine);
	uint16_t nack=0;
	if(next_command==4)
	{
		nack=(1u << 9) | (1u << 0);
	}
	printf("%d/%d",next_command, nack);
    pio_i2c_put16(pio, pio_state_machine, (0xffu << 1) | nack);

	while(pio_sm_is_rx_fifo_empty(pio, pio_state_machine));

	returnval = pio_i2c_get(pio, pio_state_machine);
	/*while(!pio_sm_is_rx_fifo_empty(pio, pio_state_machine))
	{
		returnval=pio_sm_get(pio, pio_state_machine);
		printf("return %d\r\n", returnval);
	}*/
	
    pio_i2c_wait_idle(pio, pio_state_machine);
    if (pio_i2c_check_error(pio, pio_state_machine)) {
        err = -1;
		printf("error\r\n");
        pio_i2c_resume_after_error(pio, pio_state_machine);
		return 0xffff;
    }

	return returnval;
}

void HWI2C_macro(uint32_t macro)
{
	switch(macro)
	{
		case 0:		printf(" 1. I2C Address search\r\n");
//				printf(" 2. I2C sniffer\r\n";
				break;
		case 1:		I2Csearch();
				break;
		case 2:		printf("Macro not available");
				break;
		default:	printf("Macro not defined");
				system_config.error=1;
	}
}

void HWI2C_cleanup(void)
{
	pio_remove_program (pio, &i2c_program, pio_loaded_offset);
	//pio_clear_instruction_memory(pio);

	bio_init();

	system_bio_claim(false, M_I2C_SDA, BP_PIN_MODE,0);
	system_bio_claim(false, M_I2C_SCL, BP_PIN_MODE,0);

	/*rcc_periph_clock_disable(RCC_I2C2);
	i2c_peripheral_disable(I2C2);
	gpio_set_mode(BP_I2C_SDA_PORT, GPIO_MODE_INPUT, GPIO_CNF_INPUT_FLOAT, BP_I2C_SDA_PIN);
	gpio_set_mode(BP_I2C_CLK_PORT, GPIO_MODE_INPUT, GPIO_CNF_INPUT_FLOAT, BP_I2C_CLK_PIN);

	// update modeConfig pins
	modeConfig.misoport=0;
	modeConfig.mosiport=0;
	modeConfig.csport=0;
	modeConfig.clkport=0;
	modeConfig.misopin=0;
	modeConfig.mosipin=0;
	modeConfig.cspin=0;
	modeConfig.clkpin=0;*/

}

/*void HWI2C_pins(void)
{
	printf("-\t-\tSCL\tSDA");
}*/

void HWI2C_settings(void)
{
	printf("HWI2C (speed)=(%d)", mode_config.baudrate_actual);
}

void HWI2C_printI2Cflags(void)
{
	uint32_t temp;
/*
	temp=I2C_SR1(BP_I2C);

	if(temp&I2C_SR1_SMBALERT) printf(" SMBALERT");
	if(temp&I2C_SR1_TIMEOUT) printf(" TIMEOUT");
	if(temp&I2C_SR1_PECERR) printf(" PECERR");
	if(temp&I2C_SR1_OVR) printf(" OVR");
	if(temp&I2C_SR1_AF) printf(" AF");
	if(temp&I2C_SR1_ARLO) printf(" ARLO");
	if(temp&I2C_SR1_BERR) printf(" BERR");
	if(temp&I2C_SR1_TxE) printf(" TxE");
	if(temp&I2C_SR1_RxNE) printf(" RxNE");
	if(temp&I2C_SR1_STOPF) printf(" STOPF");
	if(temp&I2C_SR1_ADD10) printf(" ADD10");
	if(temp&I2C_SR1_BTF) printf(" BTF");
	if(temp&I2C_SR1_ADDR) printf(" ADDR");
	if(temp&I2C_SR1_SB) printf(" SB");

	temp=I2C_SR2(BP_I2C);

	if(temp&I2C_SR2_DUALF) printf(" DUALF");
	if(temp&I2C_SR2_SMBHOST) printf(" SMBHOST");
	if(temp&I2C_SR2_SMBDEFAULT) printf(" SMBDEFAULT");
	if(temp&I2C_SR2_GENCALL) printf(" GENCALL");
	if(temp&I2C_SR2_TRA) printf(" TRA");
	if(temp&I2C_SR2_BUSY) printf(" BUSY");
	if(temp&I2C_SR2_MSL) printf(" MSL");
	*/
}

void HWI2C_help(void)
{
	printf("Muli-Master-multi-slave 2 wire protocol using a CLOCK and a bidirectional DATA\r\n");
	printf("line in opendrain configuration. Standard clock frequencies are 100KHz, 400KHz\r\n");
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
	printf("\tGND\t------------------ GND\r\n");			
}


static uint8_t checkshort(void)
{
	uint8_t temp;

	//temp=(gpio_get(BP_I2C_SDA_SENSE_PORT, BP_I2C_SDA_SENSE_PIN)==0?1:0);
	//temp|=(gpio_get(BP_I2C_SCL_SENSE_PORT, BP_I2C_SCL_SENSE_PIN)==0?2:0);

	return (temp==3);			// there is only a short when both are 0 otherwise repeated start wont work
}

bool reserved_addr(uint8_t addr) {
    return (addr & 0x78) == 0 || (addr & 0x78) == 0x78;
}

static void I2Csearch(void)
{
	/*int i=0;
	uint8_t timeout;
	uint16_t ack;

	if(checkshort())
	{
		printf("no pullup or short");
		modeConfig.error=1;
		return;
	}

	printf("Found:\r\n");

	for(i=0; i<256; i++)
	{
		i2c_send_start(BP_I2C);

		timeout=100; // 1 us enough?

		// wait for start (SB), switched to master (MSL) and a taken bus (BUSY)
		while ((!((I2C_SR1(BP_I2C)&I2C_SR1_SB)&&(I2C_SR2(BP_I2C)&(I2C_SR2_MSL|I2C_SR2_BUSY))))&&timeout) { timeout--; delayus(1); }

		if(timeout==0)
		{
				
		}

		i2c_send_data(BP_I2C, i);

		timeout=100;	// 100us enough?

		while(((!(I2C_SR1(BP_I2C)&I2C_SR1_ADDR))&&(!(I2C_SR1(BP_I2C)&I2C_SR1_AF)))&&timeout) { timeout--; delayus(1); } // or BTF??

		ack=!(I2C_SR1(BP_I2C)&I2C_SR1_AF);

		if(ack) printf("0x%02X(%c) ", i, ((i&0x1)?'R':'W'));

		ack=I2C_SR1(BP_I2C);				// need to read both status registers??
		ack=I2C_SR2(BP_I2C);
		I2C_SR1(BP_I2C)=0;				// clear all errors/flags
		I2C_SR2(BP_I2C)=0;				// clear all errors/flags

		i2c_send_stop(BP_I2C);
	}*/

	printf("\r\nI2C Bus Scan\r\n");
    printf("   0  1  2  3  4  5  6  7  8  9  A  B  C  D  E  F\r\n");

    for (int addr = 0; addr < (1 << 7); ++addr) {
        if (addr % 16 == 0) {
            printf("%02x ", addr);
        }
        // Perform a 0-byte read from the probe address. The read function
        // returns a negative result NAK'd any time other than the last data
        // byte. Skip over reserved addresses.
        int result;
        if (reserved_addr(addr)) 
            result = -1;
        else
            result = pio_i2c_read_blocking(pio, pio_state_machine, addr, NULL, 0);

        printf(result < 0 ? "." : "@");
        printf(addr % 16 == 15 ? "\r\n" : "  ");
    }
    printf("Done.\r\n");
}


