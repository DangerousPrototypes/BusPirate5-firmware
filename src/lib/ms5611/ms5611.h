/**
 * \file ms5611.h
 *
 * \brief ms5611 Temperature sensor driver header file
 *
 * Copyright (c) 2016 Measurement Specialties. All rights reserved.
 *
 * \asf_license_start
 *
 * \page License
 *
 *
 * \asf_license_stop
 *
 */

#ifndef ms5611_H_INCLUDED
#define ms5611_H_INCLUDED

#include <stdint.h>
#include <stdbool.h>
#include <math.h>

enum ms5611_resolution_osr {
	ms5611_resolution_osr_256 = 0,
	ms5611_resolution_osr_512,
	ms5611_resolution_osr_1024,
	ms5611_resolution_osr_2048,
	ms5611_resolution_osr_4096
};

enum ms5611_status {
	ms5611_status_ok,
	ms5611_status_no_i2c_acknowledge,
	ms5611_status_i2c_transfer_error,
	ms5611_status_crc_error
};
	
// Functions
uint32_t ms5611_read_temperature_and_pressure_simple(float *temperature, float *pressure);

/**
 * \brief Configures the SERCOM I2C master to be used with the ms5611 device.
 */
void ms5611_init(void);

/**
 * \brief Check whether ms5611 device is connected
 *
 * \return bool : status of ms5611
 *       - true : Device is present
 *       - false : Device is not acknowledging I2C address
  */
bool ms5611_is_connected(void);

/**
 * \brief Reset the ms5611 device
 *
 * \return ms5611_status : status of ms5611
 *       - ms5611_status_ok : I2C transfer completed successfully
 *       - ms5611_status_i2c_transfer_error : Problem with i2c transfer
 *       - ms5611_status_no_i2c_acknowledge : I2C did not acknowledge
 */
enum ms5611_status ms5611_reset(void);

/**
 * \brief Set  ADC resolution.
 *
 * \param[in] ms5611_resolution_osr : Resolution requested
 *
 */
void ms5611_set_resolution(enum ms5611_resolution_osr );

/**
 * \brief Reads the temperature and pressure ADC value and compute the compensated values.
 *
 * \param[out] float* : Celsius Degree temperature value
 * \param[out] float* : mbar pressure value
 *
 * \return ms5611_status : status of ms5611
 *       - ms5611_status_ok : I2C transfer completed successfully
 *       - ms5611_status_i2c_transfer_error : Problem with i2c transfer
 *       - ms5611_status_no_i2c_acknowledge : I2C did not acknowledge
 *       - ms5611_status_crc_error : CRC check error on on the PROM coefficients
 */
enum ms5611_status ms5611_read_temperature_and_pressure(float *, float *);

#endif /* ms5611_H_INCLUDED */