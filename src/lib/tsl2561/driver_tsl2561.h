/**
 * Copyright (c) 2015 - present LibDriver All rights reserved
 * 
 * The MIT License (MIT)
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in all
 * copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE. 
 *
 * @file      driver_tsl2561.h
 * @brief     driver tsl2561 header file
 * @version   2.0.0
 * @author    Shifeng Li
 * @date      2021-02-26
 *
 * <h3>history</h3>
 * <table>
 * <tr><th>Date        <th>Version  <th>Author      <th>Description
 * <tr><td>2021/02/26  <td>2.0      <td>Shifeng Li  <td>format the code
 * <tr><td>2020/10/28  <td>1.0      <td>Shifeng Li  <td>first upload
 * </table>
 */

#ifndef DRIVER_TSL2561_H
#define DRIVER_TSL2561_H

#include <stdio.h>
#include <stdint.h>
#include <string.h>

#ifdef __cplusplus
extern "C"{
#endif

/**
 * @defgroup tsl2561_driver tsl2561 driver function
 * @brief    tsl2561 driver modules
 * @{
 */

/**
 * @addtogroup tsl2561_base_driver
 * @{
 */

/**
 * @brief tsl2561 address enumeration definition
 */
typedef enum
{
    TSL2561_ADDRESS_GND   = 0x52,        /**< ADDR pin connected to GND */
    TSL2561_ADDRESS_FLOAT = 0x72,        /**< ADDR pin connected to FLOAT */
    TSL2561_ADDRESS_VCC   = 0x92,        /**< ADDR pin connected to VCC */
} tsl2561_address_t;

/**
 * @brief tsl2561 bool enumeration definition
 */
typedef enum
{
    TSL2561_BOOL_FALSE = 0x00,        /**< disable function */
    TSL2561_BOOL_TRUE  = 0x01,        /**< enable function */
} tsl2561_bool_t;

/**
 * @brief tsl2561 gain enumeration definition
 */
typedef enum
{
    TSL2561_GAIN_1  = 0x00,        /**< 1x gain */
    TSL2561_GAIN_16 = 0x01,        /**< 16x gain */
} tsl2561_gain_t;

/**
 * @brief tsl2561 integration time enumeration definition
 */
typedef enum
{
    TSL2561_INTEGRATION_TIME_13MS  = 0x00,        /**< 13 ms integration time */
    TSL2561_INTEGRATION_TIME_101MS = 0x01,        /**< 101 ms integration time */
    TSL2561_INTEGRATION_TIME_402MS = 0x02,        /**< 402 ms integration time */
} tsl2561_integration_time_t;

/**
 * @}
 */

/**
 * @addtogroup tsl2561_interrupt_driver
 * @{
 */

/**
 * @brief tsl2561 interrupt mode enumeration definition
 */
typedef enum
{
    TSL2561_INTERRUPT_MODE_EVERY_ADC_CYCLE                  = 0x00,        /**< every adc cycle interrupt */
    TSL2561_INTERRUPT_MODE_ANY_VALUE_OUT_OF_THRESHOLD_RANGE = 0x01,        /**< any value out of threshold range interrupt */
    TSL2561_INTERRUPT_MODE_2_INTEGRATION_TIME_OUT_OF_RANGE  = 0x02,        /**< 2 integration time out of range */
    TSL2561_INTERRUPT_MODE_3_INTEGRATION_TIME_OUT_OF_RANGE  = 0x03,        /**< 3 integration time out of range */
    TSL2561_INTERRUPT_MODE_4_INTEGRATION_TIME_OUT_OF_RANGE  = 0x04,        /**< 4 integration time out of range */
    TSL2561_INTERRUPT_MODE_5_INTEGRATION_TIME_OUT_OF_RANGE  = 0x05,        /**< 5 integration time out of range */
    TSL2561_INTERRUPT_MODE_6_INTEGRATION_TIME_OUT_OF_RANGE  = 0x06,        /**< 6 integration time out of range */
    TSL2561_INTERRUPT_MODE_7_INTEGRATION_TIME_OUT_OF_RANGE  = 0x07,        /**< 7 integration time out of range */
    TSL2561_INTERRUPT_MODE_8_INTEGRATION_TIME_OUT_OF_RANGE  = 0x08,        /**< 8 integration time out of range */
    TSL2561_INTERRUPT_MODE_9_INTEGRATION_TIME_OUT_OF_RANGE  = 0x09,        /**< 9 integration time out of range */
    TSL2561_INTERRUPT_MODE_10_INTEGRATION_TIME_OUT_OF_RANGE = 0x0A,        /**< 10 integration time out of range */
    TSL2561_INTERRUPT_MODE_11_INTEGRATION_TIME_OUT_OF_RANGE = 0x0B,        /**< 11 integration time out of range */
    TSL2561_INTERRUPT_MODE_12_INTEGRATION_TIME_OUT_OF_RANGE = 0x0C,        /**< 12 integration time out of range */
    TSL2561_INTERRUPT_MODE_13_INTEGRATION_TIME_OUT_OF_RANGE = 0x0D,        /**< 13 integration time out of range */
    TSL2561_INTERRUPT_MODE_14_INTEGRATION_TIME_OUT_OF_RANGE = 0x0E,        /**< 14 integration time out of range */
    TSL2561_INTERRUPT_MODE_15_INTEGRATION_TIME_OUT_OF_RANGE = 0x0F,        /**< 15 integration time out of range */
} tsl2561_interrupt_mode_t;

/**
 * @}
 */

/**
 * @addtogroup tsl2561_base_driver
 * @{
 */

/**
 * @brief tsl2561 handle structure definition
 */
typedef struct tsl2561_handle_s
{
    uint8_t iic_addr;                                                                   /**< iic device address */
    uint8_t (*iic_init)(void);                                                          /**< point to an iic_init function address */
    uint8_t (*iic_deinit)(void);                                                        /**< point to an iic_deinit function address */
    uint8_t (*iic_read)(uint8_t addr, uint8_t reg, uint8_t *buf, uint16_t len);         /**< point to an iic_read function address */
    uint8_t (*iic_write)(uint8_t addr, uint8_t reg, uint8_t *buf, uint16_t len);        /**< point to an iic_write function address */
    void (*delay_ms)(uint32_t ms);                                                      /**< point to a delay_ms function address */
    void (*debug_print)(const char *const fmt, ...);                                    /**< point to a debug_print function address */
    uint8_t inited;                                                                     /**< inited flag */
} tsl2561_handle_t;

/**
 * @brief tsl2561 information structure definition
 */
typedef struct tsl2561_info_s
{
    char chip_name[32];                /**< chip name */
    char manufacturer_name[32];        /**< manufacturer name */
    char interface[8];                 /**< chip interface name */
    float supply_voltage_min_v;        /**< chip min supply voltage */
    float supply_voltage_max_v;        /**< chip max supply voltage */
    float max_current_ma;              /**< chip max current */
    float temperature_min;             /**< chip min operating temperature */
    float temperature_max;             /**< chip max operating temperature */
    uint32_t driver_version;           /**< driver version */
} tsl2561_info_t;

/**
 * @}
 */

/**
 * @defgroup tsl2561_link_driver tsl2561 link driver function
 * @brief    tsl2561 link driver modules
 * @ingroup  tsl2561_driver
 * @{
 */

/**
 * @brief     initialize tsl2561_handle_t structure
 * @param[in] HANDLE points to a tsl2561 handle structure
 * @param[in] STRUCTURE is tsl2561_handle_t
 * @note      none
 */
#define DRIVER_TSL2561_LINK_INIT(HANDLE, STRUCTURE)   memset(HANDLE, 0, sizeof(STRUCTURE))

/**
 * @brief     link iic_init function
 * @param[in] HANDLE points to a tsl2561 handle structure
 * @param[in] FUC points to an iic_init function address
 * @note      none
 */
#define DRIVER_TSL2561_LINK_IIC_INIT(HANDLE, FUC)    (HANDLE)->iic_init = FUC

/**
 * @brief     link iic_deinit function
 * @param[in] HANDLE points to a tsl2561 handle structure
 * @param[in] FUC points to an iic_deinit function address
 * @note      none
 */
#define DRIVER_TSL2561_LINK_IIC_DEINIT(HANDLE, FUC)  (HANDLE)->iic_deinit = FUC

/**
 * @brief     link iic_read function
 * @param[in] HANDLE points to a tsl2561 handle structure
 * @param[in] FUC points to an iic_read function address
 * @note      none
 */
#define DRIVER_TSL2561_LINK_IIC_READ(HANDLE, FUC)    (HANDLE)->iic_read = FUC

/**
 * @brief     link iic_write function
 * @param[in] HANDLE points to a tsl2561 handle structure
 * @param[in] FUC points to an iic_write function address
 * @note      none
 */
#define DRIVER_TSL2561_LINK_IIC_WRITE(HANDLE, FUC)   (HANDLE)->iic_write = FUC

/**
 * @brief     link delay_ms function
 * @param[in] HANDLE points to a tsl2561 handle structure
 * @param[in] FUC points to a delay_ms function address
 * @note      none
 */
#define DRIVER_TSL2561_LINK_DELAY_MS(HANDLE, FUC)    (HANDLE)->delay_ms = FUC

/**
 * @brief     link debug_print function
 * @param[in] HANDLE points to a tsl2561 handle structure
 * @param[in] FUC points to a debug_print function address
 * @note      none
 */
#define DRIVER_TSL2561_LINK_DEBUG_PRINT(HANDLE, FUC) (HANDLE)->debug_print = FUC

/**
 * @}
 */

/**
 * @defgroup tsl2561_base_driver tsl2561 base driver function
 * @brief    tsl2561 base driver modules
 * @ingroup  tsl2561_driver
 * @{
 */

/**
 * @brief      get chip's information
 * @param[out] *info points to a tsl2561 info structure
 * @return     status code
 *             - 0 success
 *             - 2 handle is NULL
 * @note       none
 */
uint8_t tsl2561_info(tsl2561_info_t *info);

/**
 * @brief     set the iic address pin
 * @param[in] *handle points to a tsl2561 handle structure
 * @param[in] addr_pin is the chip iic address pin
 * @return    status code
 *            - 0 success
 *            - 1 set addr pin failed
 *            - 2 handle is NULL
 * @note      none
 */
uint8_t tsl2561_set_addr_pin(tsl2561_handle_t *handle, tsl2561_address_t addr_pin);

/**
 * @brief      get the iic address pin
 * @param[in]  *handle points to a tsl2561 handle structure
 * @param[out] *addr_pin points to a chip iic address pin buffer
 * @return      status code
 *              - 0 success
 *              - 1 get addr pin failed
 *              - 2 handle is NULL
 * @note        none
 */
uint8_t tsl2561_get_addr_pin(tsl2561_handle_t *handle, tsl2561_address_t *addr_pin);

/**
 * @brief     initialize the chip
 * @param[in] *handle points to a tsl2561 handle structure
 * @return    status code
 *            - 0 success
 *            - 1 iic initialization failed
 *            - 2 handle is NULL
 *            - 3 linked functions is NULL
 * @note      none
 */
uint8_t tsl2561_init(tsl2561_handle_t *handle);

/**
 * @brief     close the chip
 * @param[in] *handle points to a tsl2561 handle structure
 * @return    status code
 *            - 0 success
 *            - 1 iic deinit failed
 *            - 2 handle is NULL
 *            - 3 handle is not initialized
 * @note      none
 */
uint8_t tsl2561_deinit(tsl2561_handle_t *handle);

/**
 * @brief      read data from the chip
 * @param[in]  *handle points to a tsl2561 handle structure
 * @param[out] *channel_0_raw points to a channel 0 raw data
 * @param[out] *channel_1_raw points to a channel 1 raw data
 * @param[out] *lux points to a converted lux
 * @return     status code
 *             - 0 success
 *             - 1 read failed
 *             - 2 handle is NULL
 *             - 3 handle is not initialized
 * @note       none
 */
uint8_t tsl2561_read(tsl2561_handle_t *handle, uint16_t *channel_0_raw, uint16_t *channel_1_raw, uint32_t *lux);

/**
 * @brief     power down the chip
 * @param[in] *handle points to a tsl2561 handle structure
 * @return    status code
 *            - 0 success
 *            - 1 power down failed
 *            - 2 handle is NULL
 *            - 3 handle is not initialized
 * @note      none
 */
uint8_t tsl2561_power_down(tsl2561_handle_t *handle);

/**
 * @brief     wake up the chip
 * @param[in] *handle points to a tsl2561 handle structure
 * @return    status code
 *            - 0 success
 *            - 1 wake up failed
 *            - 2 handle is NULL
 *            - 3 handle is not initialized
 * @note      none
 */
uint8_t tsl2561_wake_up(tsl2561_handle_t *handle);

/**
 * @brief     set the adc gain
 * @param[in] *handle points to a tsl2561 handle structure
 * @param[in] gain is the adc gain
 * @return    status code
 *            - 0 success
 *            - 1 set gain failed
 *            - 2 handle is NULL
 * @note      none
 */
uint8_t tsl2561_set_gain(tsl2561_handle_t *handle, tsl2561_gain_t gain);

/**
 * @brief      get the adc gain
 * @param[in]  *handle points to a tsl2561 handle structure
 * @param[out] *gain points to an adc gain buffer
 * @return     status code
 *             - 0 success
 *             - 1 get gain failed
 *             - 2 handle is NULL
 * @note       none
 */
uint8_t tsl2561_get_gain(tsl2561_handle_t *handle, tsl2561_gain_t *gain);

/**
 * @brief     set the integration time
 * @param[in] *handle points to a tsl2561 handle structure
 * @param[in] t is the integration time
 * @return    status code
 *            - 0 success
 *            - 1 set integration time failed
 *            - 2 handle is NULL
 * @note      none
 */
uint8_t tsl2561_set_integration_time(tsl2561_handle_t *handle, tsl2561_integration_time_t t);

/**
 * @brief      get the integration time
 * @param[in]  *handle points to a tsl2561 handle structure
 * @param[out] *t points to an integration time buffer
 * @return     status code
 *             - 0 success
 *             - 1 get integration time failed
 *             - 2 handle is NULL
 * @note       none
 */
uint8_t tsl2561_get_integration_time(tsl2561_handle_t *handle, tsl2561_integration_time_t *t);

/**
 * @}
 */

/**
 * @defgroup tsl2561_interrupt_driver tsl2561 interrupt driver function
 * @brief    tsl2561 interrupt driver modules
 * @ingroup  tsl2561_driver
 * @{
 */

/**
 * @brief     set the interrupt mode
 * @param[in] *handle points to a tsl2561 handle structure
 * @param[in] mode is the interrupt mode
 * @return    status code
 *            - 0 success
 *            - 1 set interrupt mode failed
 *            - 2 handle is NULL
 * @note      none
 */
uint8_t tsl2561_set_interrupt_mode(tsl2561_handle_t *handle, tsl2561_interrupt_mode_t mode);

/**
 * @brief      get the interrupt mode
 * @param[in]  *handle points to a tsl2561 handle structure
 * @param[out] *mode points to an interrupt mode buffer
 * @return     status code
 *             - 0 success
 *             - 1 get interrupt mode failed
 *             - 2 handle is NULL
 * @note       none
 */
uint8_t tsl2561_get_interrupt_mode(tsl2561_handle_t *handle, tsl2561_interrupt_mode_t *mode);

/**
 * @brief     enable or disable the chip interrupt
 * @param[in] *handle points to a tsl2561 handle structure
 * @param[in] enable is a bool value
 * @return    status code
 *            - 0 success
 *            - 1 set interrupt failed
 *            - 2 handle is NULL
 * @note      none
 */
uint8_t tsl2561_set_interrupt(tsl2561_handle_t *handle, tsl2561_bool_t enable);

/**
 * @brief      get the chip interrupt
 * @param[in]  *handle points to a tsl2561 handle structure
 * @param[out] *enable points to a bool buffer
 * @return     status code
 *             - 0 success
 *             - 1 get interrupt failed
 *             - 2 handle is NULL
 * @note       none
 */
uint8_t tsl2561_get_interrupt(tsl2561_handle_t *handle, tsl2561_bool_t *enable);

/**
 * @brief     set the interrupt high threshold
 * @param[in] *handle points to a tsl2561 handle structure
 * @param[in] ch0_raw is the channel 0 raw data
 * @return    status code
 *            - 0 success
 *            - 1 set interrupt high threshold failed
 *            - 2 handle is NULL
 * @note      none
 */
uint8_t tsl2561_set_interrupt_high_threshold(tsl2561_handle_t *handle, uint16_t ch0_raw);

/**
 * @brief      get the interrupt high threshold
 * @param[in]  *handle points to a tsl2561 handle structure
 * @param[out] *ch0_raw points to a channel 0 raw data buffer
 * @return     status code
 *             - 0 success
 *             - 1 get interrupt high threshold failed
 *             - 2 handle is NULL
 * @note       none
 */
uint8_t tsl2561_get_interrupt_high_threshold(tsl2561_handle_t *handle, uint16_t *ch0_raw);

/**
 * @brief     set the interrupt low threshold
 * @param[in] *handle points to a tsl2561 handle structure
 * @param[in] ch0_raw is the channel 0 raw data
 * @return    status code
 *            - 0 success
 *            - 1 set interrupt low threshold failed
 *            - 2 handle is NULL
 * @note      none
 */
uint8_t tsl2561_set_interrupt_low_threshold(tsl2561_handle_t *handle, uint16_t ch0_raw);

/**
 * @brief      get the interrupt low threshold
 * @param[in]  *handle points to a tsl2561 handle structure
 * @param[out] *ch0_raw points to a channel 0 raw data buffer
 * @return     status code
 *             - 0 success
 *             - 1 get interrupt low threshold failed
 *             - 2 handle is NULL
 * @note       none
 */
uint8_t tsl2561_get_interrupt_low_threshold(tsl2561_handle_t *handle, uint16_t *ch0_raw);

/**
 * @}
 */

/**
 * @defgroup tsl2561_extend_driver tsl2561 extend driver function
 * @brief    tsl2561 extend driver modules
 * @ingroup  tsl2561_driver
 * @{
 */

/**
 * @brief     set the chip register
 * @param[in] *handle points to a tsl2561 handle structure
 * @param[in] reg is the iic register address
 * @param[in] *buf points to a data buffer
 * @param[in] len is the data buffer length
 * @return    status code
 *            - 0 success
 *            - 1 write failed
 *            - 2 handle is NULL
 *            - 3 handle is not initialized
 * @note      none
 */
uint8_t tsl2561_set_reg(tsl2561_handle_t *handle, uint8_t reg, uint8_t *buf, uint16_t len);

/**
 * @brief      get the chip register
 * @param[in]  *handle points to a tsl2561 handle structure
 * @param[in]  reg is the iic register address
 * @param[out] *buf points to a data buffer
 * @param[in]  len is the data buffer length
 * @return     status code
 *             - 0 success
 *             - 1 read failed
 *             - 2 handle is NULL
 *             - 3 handle is not initialized
 * @note       none
 */
uint8_t tsl2561_get_reg(tsl2561_handle_t *handle, uint8_t reg, uint8_t *buf, uint16_t len);


uint32_t a_tsl2561_calculate_lux(uint16_t gain, uint16_t t, uint16_t ch0, uint16_t ch1);
/**
 * @}
 */

/**
 * @}
 */

#ifdef __cplusplus
}
#endif

#endif
