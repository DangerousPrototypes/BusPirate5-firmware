#include <stdio.h>
#include "pico/stdlib.h"
#include <stdint.h>
#include "pirate.h"
#include "system_config.h"
#include "opt_args.h"
#include "bytecode.h"
#include "mode/hwi2c.h"
#include "bio.h"
#include "ui/ui_prompt.h"
#include "i2c.pio.h"
#include "pio_i2c.h"
#include "storage.h"
#include "ui/ui_term.h"
#include "lib/ms5611/ms5611.h"
#include "lib/tsl2561/driver_tsl2561.h" 

#define M_I2C_PIO pio1
#define M_I2C_SDA BIO6
#define M_I2C_SCL BIO7

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

uint32_t hwi2c_setup(void)
{
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
	
	return 1;
}

uint32_t hwi2c_setup_exc(void)
{
	pio_loaded_offset = pio_add_program(pio, &i2c_program);
    i2c_program_init(pio, pio_state_machine, pio_loaded_offset, bio2bufiopin[M_I2C_SDA], bio2bufiopin[M_I2C_SCL], bio2bufdirpin[M_I2C_SDA], bio2bufdirpin[M_I2C_SCL]);
	system_bio_claim(true, M_I2C_SDA, BP_PIN_MODE, pin_labels[0]);
	system_bio_claim(true, M_I2C_SCL, BP_PIN_MODE, pin_labels[1]);
	mode_config.start_sent=false;
	return 1;
}

bool hwi2c_error(uint32_t error, struct _bytecode *result)
{
	switch(error)
	{
		case 1:
			result->error_message=t[T_HWI2C_I2C_ERROR];
			result->error=SRES_ERROR; 
			pio_i2c_resume_after_error(pio, pio_state_machine);
			return true;
			break;
		case 2:
			result->error_message=t[T_HWI2C_TIMEOUT];
			result->error=SRES_ERROR; 
			pio_i2c_resume_after_error(pio, pio_state_machine);
			return true;
			break;
		default:
			return false;
	}
}

void hwi2c_start(struct _bytecode *result, struct _bytecode *next)
{
	result->data_message=t[T_HWI2C_START];

	if(checkshort())
	{
		result->error_message=t[T_HWI2C_NO_PULLUP_DETECTED];
		result->error=SRES_WARN; 
	}
	
	uint8_t error=pio_i2c_start_timeout(pio, pio_state_machine, 0xfffff);

	if(!hwi2c_error(error, result))
	{
		mode_config.start_sent=true;
	}
}

void hwi2c_stop(struct _bytecode *result, struct _bytecode *next)
{
	result->data_message=t[T_HWI2C_STOP];

	uint32_t error=pio_i2c_stop_timeout(pio, pio_state_machine, 0xffff);

	hwi2c_error(error, result);
}

void hwi2c_write(struct _bytecode *result, struct _bytecode *next)
{
	//if a start was just sent, determine if this is a read or write address
	// and configure the PIO I2C
	if(mode_config.start_sent)
	{
		pio_i2c_rx_enable(pio, pio_state_machine, (result->out_data & 1u));
		mode_config.start_sent=false;
	}
	
	uint32_t error=pio_i2c_write_timeout(pio, pio_state_machine, result->out_data, 0xffff);

	hwi2c_error(error, result);

	result->data_message=(error?t[T_HWI2C_NACK]:t[T_HWI2C_ACK]);

}

void hwi2c_read(struct _bytecode *result, struct _bytecode *next)
{
	bool ack=(next?(next->command!=4):true);

	uint32_t error=pio_i2c_read_timeout(pio, pio_state_machine, &result->in_data, ack, 0xffff);

    hwi2c_error(error, result);

	result->data_message=(ack?t[T_HWI2C_ACK]:t[T_HWI2C_NACK]);
}
uint32_t macro_si7021(void);
uint32_t macro_ms5611(void);
uint32_t macro_tsl2561(void);
unsigned int CalculateLux(unsigned int iGain, unsigned int tInt, unsigned int ch0, unsigned int ch1, int iType);
void hwi2c_macro(uint32_t macro)
{
	uint32_t result=0;
	switch(macro)
	{
		case 0:		printf(" 1. I2C Address search\r\n 2. SI7021/HTU21/SHT21/HDC1080\r\n 3. MS5611\r\n 4. TSL2561\r\n");
//				printf(" 2. I2C sniffer\r\n";
				break;
		case 1:		I2Csearch();	break;
		case 2: 	result=macro_si7021(); break;
		case 3:		result=macro_ms5611(); break;
		case 4:		result=macro_tsl2561(); break;
		default:	printf("%s\r\n", t[T_MODE_ERROR_MACRO_NOT_DEFINED]);
				system_config.error=1;
	}

	if(result)
	{
		printf("Device not found\r\n");
	}
}

// From SparkFun TSL2561 library
// https://github.com/sparkfun/SparkFun_TSL2561_Arduino_Library/blob/master/src/SparkFunTSL2561.cpp
bool tsl2561_getLux(unsigned char gain, unsigned int CH0, unsigned int CH1, double *lux)
	// Convert raw data to lux
	// gain: 0 (1X) or 1 (16X), see setTiming()
	// ms: integration time in ms, from setTiming() or from manual integration
	// CH0, CH1: results from getData()
	// lux will be set to resulting lux calculation
	// returns true (1) if calculation was successful
	// RETURNS false (0) AND lux = 0.0 IF EITHER SENSOR WAS SATURATED (0XFFFF)
{
	double ratio, d0, d1;

	// Determine if either sensor saturated (0xFFFF)
	// If so, abandon ship (calculation will not be accurate)
	if ((CH0 == 0xFFFF) || (CH1 == 0xFFFF))
	{
		*lux = 0.0;
		return(false);
	}

	// Convert from unsigned integer to floating point
	d0 = CH0; d1 = CH1;

	// We will need the ratio for subsequent calculations
	ratio = d1 / d0;

	// Normalize for integration time
	//d0 *= (402.0/ms);
	//d1 *= (402.0/ms);

	// Normalize for gain
	if (!gain)
	{
		d0 *= 16;
		d1 *= 16;
	}

	// Determine lux per datasheet equations:
	
	if (ratio < 0.5)
	{
		*lux = 0.0304 * d0 - 0.062 * d0 * pow(ratio,1.4);
		return(true);
	}

	if (ratio < 0.61)
	{
		*lux = 0.0224 * d0 - 0.031 * d1;
		return(true);
	}

	if (ratio < 0.80)
	{
		*lux = 0.0128 * d0 - 0.0153 * d1;
		return(true);
	}

	if (ratio < 1.30)
	{
		*lux = 0.00146 * d0 - 0.00112 * d1;
		return(true);
	}

	// if (ratio > 1.30)
	*lux = 0.0;
	return(true);
}

uint32_t macro_tsl2561()
{
	//select register [0b01110010 0b11100000]
	// start device [0b01110010 3]
	//confirm start [0b01110011 r]
	// select ID register [0b01110010 0b11101010]
	// read ID register [0b01110011 r] 7:4 0101 = TSL2561T 3:0 0 = revision
	// select ADC register [0b01110010 0b11101100]
	//0b11011100
	uint16_t chan0, chan1;
	char data[4];

	printf("TSL2561 LUX sensor\r\n");

	//select register [0b01110010 0b11100000]
	data[0]=0b11100000;
	if(pio_i2c_write_blocking_timeout(pio, pio_state_machine, 0b01110010, data, 1, 0xffff)) return 1;
	// start device [0b01110010 3]
	data[0]=3;
	if(pio_i2c_write_blocking_timeout(pio, pio_state_machine, 0b01110010, data, 1, 0xffff)) return 1;
	delayms(500);
	// select ID register [0b01110010 0b11101010]
	// read ID register [0b01110011 r] 7:4 0101 = TSL2561T 3:0 0 = revision
	data[0]=0b11101010;
	if(pio_i2c_transaction_blocking_timeout(pio, pio_state_machine, 0b01110010, data, 1, data, 1, 0xffff)) return 1;	
	printf("ID: %d REV: %d\r\n", data[0]>>4, data[0]&0b1111);
	// select ADC register [0b01110010 0b11101100]	
	data[0]=0b11101100;
	if(pio_i2c_transaction_blocking_timeout(pio, pio_state_machine, 0b01110010, data, 1, data, 4, 0xffff)) return 1;	
	chan0=data[1]<<8|data[0];
	chan1=data[3]<<8|data[2];

	//uint32_t lux1=a_tsl2561_calculate_lux(0, 2,chan0, chan1);
	double lux1;
	tsl2561_getLux(0, chan0, chan1, &lux1); // 0 = 1x gain, 402ms integration time

	printf("Chan0: %d Chan1: %d LUX: %f\r\n", chan0, chan1, lux1);

	return 0;
}

uint32_t macro_ms5611()
{
	//PS high, CSB low
	//reset [0b11101110 0b00011110]
	//PROM read [0b11101110 0b10100110] [0b11101111 r:2]
	//start conversion [0b11101110 0b01001000]
	//ADC read [0b11101110 0] [0b11101111 r:3]
	float temperature;
	float pressure;
	printf("MS5611 Temp & Pressure sensor\r\n");
	if(ms5611_read_temperature_and_pressure_simple(pio, pio_state_machine, &temperature, &pressure))
	{
		return 1;
	}
	printf("Temperature: %f\r\nPressure: %f\r\n", temperature, pressure);
	return 0;
}

uint32_t macro_si7021()
{
	uint8_t data[4];

	printf("SI7021/HTU21/SHT21/HDC1080 Temp & Humidity sensor\r\n");

	// humidity
	data[0]=0xf5;
	if(pio_i2c_write_blocking_timeout(pio, pio_state_machine, 0x80, data, 1, 0xffff))
	{
		return 1;
	}
	delayms(23); //delay for max conversion time
	if(pio_i2c_read_blocking_timeout(pio, pio_state_machine, 0x81, data, 2, 0xffff))
	{
		return 1;
	}
	float f=(float)((float)(125*(data[0]<<8 | data[1]))/65536)-6;
	printf("Humidity:\r\n [0x80 0xf5] D:23 [0x81 r:2]\r\n %.2f%% (%#04x %#04x)\r\n", f, data[0], data[1]);

	// temperature [0x80 0xe0] [0x81 r:2]
	
	data[0]=0xe0;
	if(pio_i2c_transaction_blocking_timeout(pio, pio_state_machine, 0x80, data, 1, data, 2, 0xffff))
	{
		return 1;
	} 
	f=(float)((float)(175.72*(data[0]<<8 | data[1]))/65536)-46.85;
	printf("Temperature:\r\n [0x80 0xe0] [0x81 r:2]\r\n %.2fC (%#04x %#04x)\r\n", f, data[0], data[1]);

	//SN
	data[0]=0xfa;
	data[1]=0xf0;
	if(pio_i2c_transaction_blocking_timeout(pio, pio_state_machine, 0x80, data, 2, data, 4, 0xffff))
	{
		return 1;
	} 
	printf("Serial Number:\r\n [0x80 0xfa 0xf0] [0x81 r:4] [0x80 0xfc 0xc9] [0x81 r:4]\r\n 0x%02x%02x%02x%02x", data[0],data[1],data[2],data[3]);
	data[0]=0xfc;
	data[1]=0xc9;
	if(pio_i2c_transaction_blocking_timeout(pio, pio_state_machine, 0x80, data, 2, data, 4, 0xffff))
	{
		return 1;
	} 	
	printf("%02x%02x%02x%02x\r\n", data[0],data[1],data[2],data[3]);

	//firmware version [0x80 0x84 0xb8] [0x81 r]
	data[0]=0x84;
	data[1]=0xb8;
	if(pio_i2c_transaction_blocking_timeout(pio, pio_state_machine, 0x80, data, 2, data, 1, 0xffff))
	{
		return 1;
	} 
	printf("Firmware Version:\r\n [0x80 0x84 0xb8] [0x81 r]\r\n %#04x\r\n", data[0]);

	return 0;
}

void hwi2c_cleanup(void)
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

/*void hwi2c_pins(void)
{
	printf("-\t-\tSCL\tSDA");
}*/

void hwi2c_settings(void)
{
	printf("HWI2C (speed)=(%d)", mode_config.baudrate_actual);
}

void hwi2c_printI2Cflags(void)
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

void hwi2c_help(void)
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

	temp=(bio_get(M_I2C_SDA)==0?1:0);
	temp|=(bio_get(M_I2C_SCL)==0?2:0);

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


