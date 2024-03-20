/**
 * \file ms5611.c
 *
 * \brief MS5611 Temperature sensor driver source file
 *
 * Copyright (c) 2016 Measurement Specialties. All rights reserved.
 *
 * For details on programming, refer to ms5611 datasheet :
 * http://www.meas-spec.com/downloads/MS5611-01BA03.pdf
 *
 */
//#include "i2c.h"
#include <stdio.h>
#include "pico/stdlib.h"
#include <stdint.h>
#include "pirate.h"
#include "i2c.pio.h"
#include "../../mode/pio_i2c.h"
#include "lib/ms5611/ms5611.h"

// Constants

// MS5611 device address
#define MS5611_ADDR													0x77 //0b1110111

// MS5611 device commands
#define MS5611_RESET_COMMAND										0x1E
#define MS5611_START_PRESSURE_ADC_CONVERSION						0x40
#define MS5611_START_TEMPERATURE_ADC_CONVERSION						0x50
#define MS5611_READ_ADC												0x00

#define MS5611_CONVERSION_OSR_MASK									0x0F

#define MS5611_CONVERSION_TIME_OSR_256								1000
#define MS5611_CONVERSION_TIME_OSR_512								2000
#define MS5611_CONVERSION_TIME_OSR_1024								3000
#define MS5611_CONVERSION_TIME_OSR_2048								5000
#define MS5611_CONVERSION_TIME_OSR_4096								9000

// MS5611 commands
#define MS5611_PROM_ADDRESS_READ_ADDRESS_0							0xA0
#define MS5611_PROM_ADDRESS_READ_ADDRESS_1							0xA2
#define MS5611_PROM_ADDRESS_READ_ADDRESS_2							0xA4
#define MS5611_PROM_ADDRESS_READ_ADDRESS_3							0xA6
#define MS5611_PROM_ADDRESS_READ_ADDRESS_4							0xA8
#define MS5611_PROM_ADDRESS_READ_ADDRESS_5							0xAA
#define MS5611_PROM_ADDRESS_READ_ADDRESS_6							0xAC
#define MS5611_PROM_ADDRESS_READ_ADDRESS_7							0xAE

// Coefficients indexes for temperature and pressure computation
#define MS5611_CRC_INDEX											7
#define MS5611_PRESSURE_SENSITIVITY_INDEX							1 
#define MS5611_PRESSURE_OFFSET_INDEX								2
#define MS5611_TEMP_COEFF_OF_PRESSURE_SENSITIVITY_INDEX				3
#define MS5611_TEMP_COEFF_OF_PRESSURE_OFFSET_INDEX					4
#define MS5611_REFERENCE_TEMPERATURE_INDEX							5
#define MS5611_TEMP_COEFF_OF_TEMPERATURE_INDEX						6
#define MS5611_COEFFICIENT_NUMBERS									8

// Static functions
static enum ms5611_status ms5611_write_command(uint8_t);
static enum ms5611_status ms5611_read_eeprom_coeff(uint8_t, uint16_t*);
static enum ms5611_status ms5611_read_eeprom(void);
static enum ms5611_status ms5611_conversion_and_read_adc( uint8_t, uint32_t *);
static bool ms5611_crc_check (uint16_t *n_prom, uint8_t crc);

/*
enum ms5611_resolution_osr ms5611_resolution_osr;
static uint16_t eeprom_coeff[MS5611_COEFFICIENT_NUMBERS];
static uint32_t conversion_time[5] = {	MS5611_CONVERSION_TIME_OSR_256,
										MS5611_CONVERSION_TIME_OSR_512,
										MS5611_CONVERSION_TIME_OSR_1024,
										MS5611_CONVERSION_TIME_OSR_2048,
										MS5611_CONVERSION_TIME_OSR_4096};
*/
// Default value to ensure coefficients are read before converting temperature
bool ms5611_coeff_read = false;


/**
 * \brief Reads the temperature and pressure ADC value and compute the compensated values.
 *
 * \param[out] float* : Celsius Degree temperature value
 * \param[out] float* : mbar pressure value
 *
 * \return ms5611_status : status of MS5611
 *       - ms5611_status_ok : I2C transfer completed successfully
 *       - ms5611_status_i2c_transfer_error : Problem with i2c transfer
 *       - ms5611_status_no_i2c_acknowledge : I2C did not acknowledge
 *       - ms5611_status_crc_error : CRC check error on the coefficients
 */
uint32_t ms5611_read_temperature_and_pressure_simple(PIO pio, uint pio_state_machine, float *temperature, float *pressure)
{
	uint32_t adc_temperature, adc_pressure;
	int32_t dT, TEMP;
	int64_t OFF, SENS, P, T2, OFF2, SENS2;
	uint16_t eeprom_coeff[MS5611_COEFFICIENT_NUMBERS];
	char data[3];

	//reset until device responds
	for(uint8_t i=0; i<5; i++)
	{
		data[0]= MS5611_RESET_COMMAND;
		if(!pio_i2c_write_blocking_timeout(pio, pio_state_machine,0b11101110, data, 1, 0xffff))
		{
			break;
		}		
	}

	busy_wait_ms(4); //2.8ms reload time after reset
	
	// If first time adc is requested, get EEPROM coefficients
	//PROM calibration data is at 0xa0-0xae
	for( uint8_t i=0 ; i< MS5611_COEFFICIENT_NUMBERS ; i++)
	{
		data[0]= MS5611_PROM_ADDRESS_READ_ADDRESS_0 + i*2;
		if(pio_i2c_transaction_blocking_timeout(pio, pio_state_machine, 0b11101110, data, 1, data, 2, 0xffff))
		{
			return 1;
		} 			
		eeprom_coeff[i]=data[0]<<8|data[1];
	}
	
	// First read temperature 0x50 + resolution = 0x58 4096 over sampling
	data[0]= MS5611_START_TEMPERATURE_ADC_CONVERSION + 0x08;
	if(pio_i2c_write_blocking_timeout(pio, pio_state_machine,0b11101110, data, 1, 0xffff))
	{
		return 1;
	}
	busy_wait_ms(10); //conversion time
	data[0]= MS5611_READ_ADC;
	if(pio_i2c_transaction_blocking_timeout(pio, pio_state_machine, 0b11101110, data, 1, data, 3, 0xffff))
	{
		return 1;
	}	
				
	adc_temperature=data[0]<<16|data[1]<<8|data[2];	

	// Now read pressure 0x40 + resolution = 0x48 4096 over sampling
	data[0]= MS5611_START_PRESSURE_ADC_CONVERSION + 0x08;
	if(pio_i2c_write_blocking_timeout(pio, pio_state_machine,0b11101110, data, 1, 0xffff))
	{
		return 1;
	}
	busy_wait_ms(10); //conversion time	
	data[0]= MS5611_READ_ADC;
	if(pio_i2c_transaction_blocking_timeout(pio, pio_state_machine, 0b11101110, data, 1, data, 3, 0xffff))
	{
		return 1;
	}		
					
    adc_pressure=data[0]<<16|data[1]<<8|data[2];	
    if(adc_temperature == 0 || adc_pressure == 0)
	{
        return 1;
	}
	
	// Difference between actual and reference temperature = D2 - Tref
	dT = (int32_t)adc_temperature - ((int32_t)eeprom_coeff[MS5611_REFERENCE_TEMPERATURE_INDEX] <<8 );
	
	// Actual temperature = 2000 + dT * TEMPSENS
	TEMP = 2000 + ((int64_t)dT * (int64_t)eeprom_coeff[MS5611_TEMP_COEFF_OF_TEMPERATURE_INDEX] >> 23) ;
	
	// Second order temperature compensation
	if( TEMP < 2000 )
	{
		T2 = ( 3 * ( (int64_t)dT  * (int64_t)dT  ) ) >> 33;
		OFF2 = 61 * ((int64_t)TEMP - 2000) * ((int64_t)TEMP - 2000) / 16 ;
		SENS2 = 29 * ((int64_t)TEMP - 2000) * ((int64_t)TEMP - 2000) / 16 ;
		
		if( TEMP < -1500 )
		{
			OFF2 += 17 * ((int64_t)TEMP + 1500) * ((int64_t)TEMP + 1500) ;
			SENS2 += 9 * ((int64_t)TEMP + 1500) * ((int64_t)TEMP + 1500) ;
		}
	}
	else
	{
		T2 = ( 5 * ( (int64_t)dT  * (int64_t)dT  ) ) >> 38;
		OFF2 = 0 ;
		SENS2 = 0 ;
	}
	
	// OFF = OFF_T1 + TCO * dT
	OFF = ( (int64_t)(eeprom_coeff[MS5611_PRESSURE_OFFSET_INDEX]) << 16 ) + ( ( (int64_t)(eeprom_coeff[MS5611_TEMP_COEFF_OF_PRESSURE_OFFSET_INDEX]) * dT ) >> 7 ) ;
	OFF -= OFF2 ;
	
	// Sensitivity at actual temperature = SENS_T1 + TCS * dT
	SENS = ( (int64_t)eeprom_coeff[MS5611_PRESSURE_SENSITIVITY_INDEX] << 15 ) + ( ((int64_t)eeprom_coeff[MS5611_TEMP_COEFF_OF_PRESSURE_SENSITIVITY_INDEX] * dT) >> 8 ) ;
	SENS -= SENS2 ;
	
	// Temperature compensated pressure = D1 * SENS - OFF
	P = ( ( (adc_pressure * SENS) >> 21 ) - OFF ) >> 15 ;
	
	*temperature = ( (float)TEMP - T2 ) / 100;
	*pressure = (float)P / 100;
	
	return 0;
}

/**
 * \brief CRC check
 *
 * \param[in] uint16_t *: List of EEPROM coefficients
 * \param[in] uint8_t : crc to compare with
 *
 * \return bool : TRUE if CRC is OK, FALSE if KO
 */
bool ms5611_crc_check (uint16_t *n_prom, uint8_t crc)
{
    uint8_t cnt, n_bit; 
    uint16_t n_rem; 
    uint16_t crc_read;

    n_rem = 0x00;
    crc_read = n_prom[7]; 
    n_prom[7] = (0xFF00 & (n_prom[7])); 
    for (cnt = 0; cnt < 16; cnt++) 
    {
        if (cnt%2==1) n_rem ^= (unsigned short) ((n_prom[cnt>>1]) & 0x00FF);
        else n_rem ^= (unsigned short) (n_prom[cnt>>1]>>8);
        for (n_bit = 8; n_bit > 0; n_bit--)
        {
            if (n_rem & (0x8000))
                n_rem = (n_rem << 1) ^ 0x3000;
            else
                n_rem = (n_rem << 1);
        }
    }
    n_rem = (0x000F & (n_rem >> 12)); 
    n_prom[7] = crc_read;
    n_rem ^= 0x00;
        
	return  ( n_rem == crc );
}
