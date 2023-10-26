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
 * @file      driver_tsl2561.c
 * @brief     driver tsl2561 source file
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

#include "lib/tsl2561/driver_tsl2561.h" 

/**
 * @brief chip information definition
 */
#define CHIP_NAME                 "AMS TSL2561"        /**< chip name */
#define MANUFACTURER_NAME         "AMS"                /**< manufacturer name */
#define SUPPLY_VOLTAGE_MIN        2.7f                 /**< chip min supply voltage */
#define SUPPLY_VOLTAGE_MAX        3.6f                 /**< chip max supply voltage */
#define MAX_CURRENT               0.60f                /**< chip max current */
#define TEMPERATURE_MIN           -40.0f               /**< chip min operating temperature */
#define TEMPERATURE_MAX           85.0f                /**< chip max operating temperature */
#define DRIVER_VERSION            2000                 /**< driver version */

/**
 * @brief chip register definition
 */
#define TSL2561_REG_CONTROL               0x80        /**< control register */
#define TSL2561_REG_TIMING                0x81        /**< timing register */
#define TSL2561_REG_THRESHLOWLOW          0xA2        /**< thresh-low low register */
#define TSL2561_REG_THRESHLOWHIGH         0xA3        /**< thresh-low high register */
#define TSL2561_REG_THRESHHIGHLOW         0xA4        /**< thresh-high low register */
#define TSL2561_REG_THRESHHIGHHIGH        0xA5        /**< thresh-high high register */
#define TSL2561_REG_INTERRUPT             0x86        /**< interrupt register */
#define TSL2561_REG_ID                    0x8A        /**< id register */
#define TSL2561_REG_DATA0LOW              0xEC        /**< data 0 low register */
#define TSL2561_REG_DATA0HIGH             0xED        /**< data 0 high register */
#define TSL2561_REG_DATA1LOW              0xEE        /**< data 1 low register */
#define TSL2561_REG_DATA1HIGH             0xEF        /**< data 1 high register */

/**
 * @brief command definition
 */
#define TSL2561_CONTROL_POWERON          0x03          /**< power on */
#define TSL2561_CONTROL_POWEROFF         0x00          /**< power off */
#define TSL2561_LUX_LUXSCALE             14            /**< scale by 2^14 */
#define TSL2561_LUX_RATIOSCALE           9             /**< scale ratio by 2^9 */
#define TSL2561_LUX_CHSCALE              10            /**< scale channel values by 2^10 */
#define TSL2561_LUX_CHSCALE_TINT0        0x7517U       /**< 322/11 * 2^TSL2561_LUX_CHSCALE */
#define TSL2561_LUX_CHSCALE_TINT1        0x0FE7U       /**< 322/81 * 2^TSL2561_LUX_CHSCALE */
#define TSL2561_LUX_K1T                  0x0040U       /**< 0.125 * 2^RATIO_SCALE */
#define TSL2561_LUX_B1T                  0x01F2U       /**< 0.0304 * 2^LUX_SCALE */
#define TSL2561_LUX_M1T                  0x01BEU       /**< 0.0272 * 2^LUX_SCALE */
#define TSL2561_LUX_K2T                  0x0080U       /**< 0.250 * 2^RATIO_SCALE */
#define TSL2561_LUX_B2T                  0x0214U       /**< 0.0325 * 2^LUX_SCALE */
#define TSL2561_LUX_M2T                  0x02D1U       /**< 0.0440 * 2^LUX_SCALE */
#define TSL2561_LUX_K3T                  0x00C0U       /**< 0.375 * 2^RATIO_SCALE */
#define TSL2561_LUX_B3T                  0x023FU       /**< 0.0351 * 2^LUX_SCALE */
#define TSL2561_LUX_M3T                  0x037BU       /**< 0.0544 * 2^LUX_SCALE */
#define TSL2561_LUX_K4T                  0x0100U       /**< 0.50 * 2^RATIO_SCALE */
#define TSL2561_LUX_B4T                  0x0270U       /**< 0.0381 * 2^LUX_SCALE */
#define TSL2561_LUX_M4T                  0x03FEU       /**< 0.0624 * 2^LUX_SCALE */
#define TSL2561_LUX_K5T                  0x0138U       /**< 0.61 * 2^RATIO_SCALE */
#define TSL2561_LUX_B5T                  0x016FU       /**< 0.0224 * 2^LUX_SCALE */
#define TSL2561_LUX_M5T                  0x01FCU       /**< 0.0310 * 2^LUX_SCALE */
#define TSL2561_LUX_K6T                  0x019AU       /**< 0.80 * 2^RATIO_SCALE */
#define TSL2561_LUX_B6T                  0x00D2U       /**< 0.0128 * 2^LUX_SCALE */
#define TSL2561_LUX_M6T                  0x00FBU       /**< 0.0153 * 2^LUX_SCALE */
#define TSL2561_LUX_K7T                  0x029AU       /**< 1.3 * 2^RATIO_SCALE */
#define TSL2561_LUX_B7T                  0x0018U       /**< 0.00146 * 2^LUX_SCALE */
#define TSL2561_LUX_M7T                  0x0012U       /**< 0.00112 * 2^LUX_SCALE */
#define TSL2561_LUX_K8T                  0x029AU       /**< 1.3 * 2^RATIO_SCALE */
#define TSL2561_LUX_B8T                  0x0000U       /**< 0.000 * 2^LUX_SCALE */
#define TSL2561_LUX_M8T                  0x0000U       /**< 0.000 * 2^LUX_SCALE */
#define TSL2561_GAIN_0X                  0x00          /**< gain 0x */
#define TSL2561_GAIN_16X                 0x10          /**< gain 16x */

/**
 * @brief     calculate the lux
 * @param[in] gain is the adc gain
 * @param[in] t is the integration time
 * @param[in] ch0 is the channel 0 raw data
 * @param[in] ch1 is the channel 1 raw data
 * @return    calculated lux
 * @note      none
 */
uint32_t a_tsl2561_calculate_lux(uint16_t gain, uint16_t t, uint16_t ch0, uint16_t ch1)
{
    uint32_t ch_scale;
    uint32_t channel_1;
    uint32_t channel_0;
    uint32_t temp;
    uint32_t ratio1 = 0;
    uint32_t ratio;
    uint32_t lux;
    uint16_t b,m;
    
    switch (t)                                                               /* choose integration time */
    {
        case 0 : 
        {
            ch_scale = TSL2561_LUX_CHSCALE_TINT0;                            /* 13.7ms */
            
            break;                                                           /* break */
        }
        case 1 : 
        {
            ch_scale = TSL2561_LUX_CHSCALE_TINT1;                            /* 101ms */
            
            break;                                                           /* break */
        }
        default : 
        {
            ch_scale = 1 << TSL2561_LUX_CHSCALE;                             /* assume no scaling */
            
            break;                                                           /* break */
        }
    }
    if (!gain)                                                               /* check gain */
    {
        ch_scale = ch_scale << 4;                                            /* cale 1X to 16X */
    }
    channel_0 = (ch0 * ch_scale) >> TSL2561_LUX_CHSCALE;                     /* set channel 0 */
    channel_1 = (ch1 * ch_scale) >> TSL2561_LUX_CHSCALE;                     /* set channel 1 */
    if (channel_0 != 0)                                                      /* check channel 0 */
    {
        ratio1 = (channel_1<<(TSL2561_LUX_RATIOSCALE+1)) / channel_0;        /* get ratio */
    }
    ratio = (ratio1 + 1) >> 1;                                               /* right shift 1 */
    if ((ratio > 0) && (ratio <= TSL2561_LUX_K1T))                           /* if K1T */
    {
        b = TSL2561_LUX_B1T;                                                 /* set B1T */
        m = TSL2561_LUX_M1T;                                                 /* set M1T */
    }
    else if (ratio <= TSL2561_LUX_K2T)                                       /* if K2T */
    {
        b = TSL2561_LUX_B2T;                                                 /* set B2T */
        m = TSL2561_LUX_M2T;                                                 /* set M2T */
    }
    else if (ratio <= TSL2561_LUX_K3T)                                       /* if K3T */
    {
        b = TSL2561_LUX_B3T;                                                 /* set B3T */
        m = TSL2561_LUX_M3T;                                                 /* set M3T */
    }
    else if (ratio <= TSL2561_LUX_K4T)                                       /* if K4T */
    {
        b = TSL2561_LUX_B4T;                                                 /* set B4T */
        m = TSL2561_LUX_M4T;                                                 /* set M4T */
    }
    else if (ratio <= TSL2561_LUX_K5T)                                       /* if K5T */
    {
        b = TSL2561_LUX_B5T;                                                 /* set B5T */
        m = TSL2561_LUX_M5T;                                                 /* set M5T */
    }
    else if (ratio <= TSL2561_LUX_K6T)                                       /* if K6T */
    {
        b = TSL2561_LUX_B6T;                                                 /* set B6T */
        m = TSL2561_LUX_M6T;                                                 /* set M6T */
    }
    else if (ratio <= TSL2561_LUX_K7T)                                       /* if K7T */
    {
        b = TSL2561_LUX_B7T;                                                 /* set B7T */
        m = TSL2561_LUX_M7T;                                                 /* set M7T */
    }
    else                                                                     /* if K8T */
    {
        b = TSL2561_LUX_B8T;                                                 /* set B8T */
        m = TSL2561_LUX_M8T;                                                 /* set M8T */
    }
    temp = ((channel_0 * b) - (channel_1 * m));                              /* get temp */
    temp += (1 << (TSL2561_LUX_LUXSCALE - 1));                               /* 2^(temp-1) */
    lux = temp >> TSL2561_LUX_LUXSCALE;                                      /* calculate lux */
    
    return lux;                                                              /* return lux */
}

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
uint8_t tsl2561_init(tsl2561_handle_t *handle)
{
    uint8_t res, id;
    
    if (handle == NULL)                                                                 /* check handle */
    {
        return 2;                                                                       /* return error */
    }
    if (handle->debug_print == NULL)                                                    /* check debug_print */
    {
        return 3;                                                                       /* return error */
    }
    if (handle->iic_init == NULL)                                                       /* check iic_init */
    {
        handle->debug_print("tsl2561: iic_init is null.\n");                            /* iic_init is null */
        
        return 3;                                                                       /* return error */
    }
    if (handle->iic_deinit == NULL)                                                     /* check iic_deinit */
    {
        handle->debug_print("tsl2561: iic_deinit is null.\n");                          /* iic_deinit is null */
        
        return 3;                                                                       /* return error */
    }
    if (handle->iic_read == NULL)                                                       /* check iic_read */
    {
        handle->debug_print("tsl2561: iic_read is null.\n");                            /* iic_read is null */
        
        return 3;                                                                       /* return error */
    }
    if (handle->iic_write == NULL)                                                      /* check iic_write */
    {
        handle->debug_print("tsl2561: iic_write is null.\n");                           /* iic_write is null */
        
        return 3;                                                                       /* return error */
    }
    if (handle->delay_ms == NULL)                                                       /* check delay_ms */
    {
        handle->debug_print("tsl2561: delay_ms is null.\n");                            /* delay_ms is null */
        
        return 3;                                                                       /* return error */
    }
    
    if (handle->iic_init() != 0)                                                        /* iic init */
    {
        handle->debug_print("tsl2561: iic init failed.\n");                             /* iic init failed */
        
        return 1;                                                                       /* return error */
    }
    res = handle->iic_read(handle->iic_addr, TSL2561_REG_ID, (uint8_t *)&id, 1);        /* read id */
    if (res != 0)                                                                       /* check the result */
    {
        handle->debug_print("tsl2561: read id failed.\n");                              /* read id failed */
        (void)handle->iic_deinit();                                                     /* iic deinit */
        
        return 1;                                                                       /* return error */
    }
    if (id == 0x10)                                                                     /* check id */
    {
        handle->debug_print("tsl2561: not support this series.\n");                     /* not support this series */
        (void)handle->iic_deinit();                                                     /* iic deinit */
        
        return 1;                                                                       /* return error */
    }
    if (id != 0x50)                                                                     /* check id */
    {
        handle->debug_print("tsl2561: id is error.\n");                                 /* id is error */
        (void)handle->iic_deinit();                                                     /* iic deinit */
        
        return 1;                                                                       /* return error */
    }
    handle->inited = 1;                                                                 /* flag finish initialization */

    return 0;                                                                           /* success return 0 */
}

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
uint8_t tsl2561_deinit(tsl2561_handle_t *handle)
{
    uint8_t reg;
    
    if (handle == NULL)                                                                      /* check handle */
    {
        return 2;                                                                            /* return error */
    }
    if (handle->inited != 1)                                                                 /* check handle initialization */
    {
        return 3;                                                                            /* return error */
    }   
    
    reg = TSL2561_CONTROL_POWEROFF;                                                          /* set power down command */
    if (handle->iic_write(handle->iic_addr, TSL2561_REG_CONTROL, (uint8_t *)&reg, 1) != 0)   /* power down */
    {
        handle->debug_print("tsl2561: power down failed.\n");                                /* power down failed */
        
        return 1;                                                                            /* return error */
    }
    if (handle->iic_deinit() != 0)                                                           /* iic deinit */
    {
        handle->debug_print("tsl2561: iic deinit failed.\n");                                /* iic deinit failed */
        
        return 1;                                                                            /* return error */
    }   
    handle->inited = 0;                                                                      /* flag close */
    
    return 0;                                                                                /* success return 0 */
}

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
uint8_t tsl2561_set_addr_pin(tsl2561_handle_t *handle, tsl2561_address_t addr_pin)
{
    if (handle == NULL)                          /* check handle */
    {
        return 2;                                /* return error */
    }
    
    handle->iic_addr = (uint8_t)addr_pin;        /* set iic address */
    
    return 0;                                    /* success return 0 */
}

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
uint8_t tsl2561_get_addr_pin(tsl2561_handle_t *handle, tsl2561_address_t *addr_pin)
{
    if (handle == NULL)                                     /* check handle */
    {
        return 2;                                           /* return error */
    }
    
    *addr_pin = (tsl2561_address_t)handle->iic_addr;        /* get iic address */
 
    return 0;                                               /* success return 0 */
}

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
uint8_t tsl2561_read(tsl2561_handle_t *handle, uint16_t *channel_0_raw, uint16_t *channel_1_raw, uint32_t *lux)
{
    uint8_t prev;
    uint8_t buf[2];
    
    if (handle == NULL)                                                                                    /* check handle */
    {
        return 2;                                                                                          /* return error */
    }
    if (handle->inited != 1)                                                                               /* check handle initialization */
    {
        return 3;                                                                                          /* return error */
    }
    
    memset(buf, 0, sizeof(uint8_t) * 2);                                                                   /* clear the buffer */
    if (handle->iic_read(handle->iic_addr, TSL2561_REG_DATA0LOW, (uint8_t *)buf, 2) != 0)                  /* read data0 low */
    {
        handle->debug_print("tsl2561: read failed.\n");                                                    /* read failed */
       
        return 1;                                                                                          /* return error */
    }
    *channel_0_raw = ((uint16_t)buf[1] << 8) | buf[0];                                                     /* get channel 0 */
    memset(buf, 0, sizeof(uint8_t) * 2);                                                                   /* clear the buffer */
    if (handle->iic_read(handle->iic_addr, TSL2561_REG_DATA1LOW, (uint8_t *)buf, 2) != 0)                  /* read data1 low */
    {
        handle->debug_print("tsl2561: read failed.\n");                                                    /* read failed */
       
        return 1;                                                                                          /* return error */
    }
    *channel_1_raw = ((uint16_t)buf[1] << 8) | buf[0];                                                     /* get channel 1 */
    if (handle->iic_read(handle->iic_addr, TSL2561_REG_TIMING, (uint8_t *)&prev, 1) != 0)                  /* read data */
    {
        handle->debug_print("tsl2561: read failed.\n");                                                    /* read failed */
        
        return 1;                                                                                          /* return error */
    }
    *lux = a_tsl2561_calculate_lux((prev & 0x10) >> 4, prev & 0x03, *channel_0_raw, *channel_1_raw);       /* calculate lux */
    
    return 0;                                                                                              /* success return 0 */
}

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
uint8_t tsl2561_set_gain(tsl2561_handle_t *handle, tsl2561_gain_t gain)
{
    uint8_t res, prev;
    
    if (handle == NULL)                                                                         /* check handle */
    {
        return 2;                                                                               /* return error */
    }
    if (handle->inited != 1)                                                                    /* check handle initialization */
    {
        return 3;                                                                               /* return error */
    }
    
    res = handle->iic_read(handle->iic_addr, TSL2561_REG_TIMING, (uint8_t *)&prev, 1);          /* read timing */
    if (res != 0)                                                                               /* check result */
    {
        handle->debug_print("tsl2561: read failed.\n");                                         /* read timing failed */
        
        return 1;                                                                               /* return error */
    }
    prev &= 0xEF;                                                                               /* clear gain bit */
    prev |= gain << 4;                                                                          /* set gain */
    if (handle->iic_write(handle->iic_addr, TSL2561_REG_TIMING, (uint8_t *)&prev, 1) != 0)      /* write command */
    {
        handle->debug_print("tsl2561: write failed.\n");                                        /* write failed */
        
        return 1;                                                                               /* return error */
    }
    else
    {
        return 0;                                                                               /* success return 0 */
    }
}

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
uint8_t tsl2561_get_gain(tsl2561_handle_t *handle, tsl2561_gain_t *gain)
{
    uint8_t res, prev;
    
    if (handle == NULL)                                                                       /* check handle */
    {
        return 2;                                                                             /* return error */
    }
    if (handle->inited != 1)                                                                  /* check handle initialization */
    {
        return 3;                                                                             /* return error */
    }
    
    res = handle->iic_read(handle->iic_addr, TSL2561_REG_TIMING, (uint8_t *)&prev, 1);        /* read timing */
    if (res != 0)                                                                             /* check result */
    {
        handle->debug_print("tsl2561: read failed.\n");                                       /* read failed */
        
        return 1;                                                                             /* return error */
    }
    prev &= ~0xEF;                                                                            /* get gain bits */
    *gain = (tsl2561_gain_t)(prev >> 4);                                                      /* get gain */
    
    return 0;                                                                                 /* success return 0 */
}

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
uint8_t tsl2561_set_integration_time(tsl2561_handle_t *handle, tsl2561_integration_time_t t)
{
    uint8_t res, prev;
    
    if (handle == NULL)                                                                         /* check handle */
    {
        return 2;                                                                               /* return error */
    }
    if (handle->inited != 1)                                                                    /* check handle initialization */
    {
        return 3;                                                                               /* return error */
    }
    
    res = handle->iic_read(handle->iic_addr, TSL2561_REG_TIMING, (uint8_t *)&prev, 1);          /* read timing */
    if (res != 0)                                                                               /* check result */
    {
        handle->debug_print("tsl2561: read failed.\n");                                         /* read failed */
       
        return 1;                                                                               /* return error */
    }
    prev &= ~0x03;                                                                              /* clear integration time bits */
    prev |= t;                                                                                  /* set integration bits */
    if (handle->iic_write(handle->iic_addr, TSL2561_REG_TIMING, (uint8_t *)&prev, 1) != 0)      /* write config */
    {
        handle->debug_print("tsl2561: write failed.\n");                                        /* write failed */
        
        return 1;                                                                               /* return error */
    }
    else
    {
        return 0;                                                                               /* success return 0 */
    }
}

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
uint8_t tsl2561_get_integration_time(tsl2561_handle_t *handle, tsl2561_integration_time_t *t)
{
    uint8_t res, prev;
    
    if (handle == NULL)                                                                       /* check handle */
    {
        return 2;                                                                             /* return error */
    }
    if (handle->inited != 1)                                                                  /* check handle initialization */
    {
        return 3;                                                                             /* return error */
    }
    
    res = handle->iic_read(handle->iic_addr, TSL2561_REG_TIMING, (uint8_t *)&prev, 1);        /* read timing */
    if (res != 0)                                                                             /* check result */
    {
        handle->debug_print("tsl2561: read failed.\n");                                       /* read failed */
        
        return 1;                                                                             /* return error */
    }
    prev &= 0x03;                                                                             /* get integration time bits */
    *t = (tsl2561_integration_time_t)prev;                                                    /* get integration time */
    
    return 0;                                                                                 /* success return 0 */
}

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
uint8_t tsl2561_set_interrupt_mode(tsl2561_handle_t *handle, tsl2561_interrupt_mode_t mode)
{
    uint8_t res, prev;
    
    if (handle == NULL)                                                                            /* check handle */
    {
        return 2;                                                                                  /* return error */
    }
    if (handle->inited != 1)                                                                       /* check handle initialization */
    {
        return 3;                                                                                  /* return error */
    }
    
    res = handle->iic_read(handle->iic_addr, TSL2561_REG_INTERRUPT, (uint8_t *)&prev, 1);          /* read interrupt reg */
    if (res != 0)                                                                                  /* check result */
    {
        handle->debug_print("tsl2561: read failed.\n");                                            /* read failed */
       
        return 1;                                                                                  /* return error */
    }
    prev &= ~0x0F;                                                                                 /* clear mode */
    prev |= mode;                                                                                  /* set mode */
    if (handle->iic_write(handle->iic_addr, TSL2561_REG_INTERRUPT, (uint8_t *)&prev, 1) != 0)      /* write interrupt config */
    {
        handle->debug_print("tsl2561: write failed.\n");                                           /* write failed */
        
        return 1;                                                                                  /* return error */
    }
    else
    {
        return 0;                                                                                  /* success return 0 */
    }
}

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
uint8_t tsl2561_get_interrupt_mode(tsl2561_handle_t *handle, tsl2561_interrupt_mode_t *mode)
{
    uint8_t res, prev;
    
    if (handle == NULL)                                                                          /* check handle */
    {
        return 2;                                                                                /* return error */
    }
    if (handle->inited != 1)                                                                     /* check handle initialization */
    {
        return 3;                                                                                /* return error */
    }
    
    res = handle->iic_read(handle->iic_addr, TSL2561_REG_INTERRUPT, (uint8_t *)&prev, 1);        /* read interrupt */
    if (res != 0)                                                                                /* check result */
    {
        handle->debug_print("tsl2561: read failed.\n");                                          /* read interrupt failed */
        
        return 1;                                                                                /* return error */
    }
    prev &= 0x0F;                                                                                /* get mode bits */
    *mode = (tsl2561_interrupt_mode_t)prev;                                                      /* get mode */
    
    return 0;                                                                                    /* success return 0 */
}

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
uint8_t tsl2561_set_interrupt(tsl2561_handle_t *handle, tsl2561_bool_t enable)
{
    uint8_t res, prev;
    
    if (handle == NULL)                                                                            /* check handle */
    {
        return 2;                                                                                  /* return error */
    }
    if (handle->inited != 1)                                                                       /* check handle initialization */
    {
        return 3;                                                                                  /* return error */
    }
    
    res = handle->iic_read(handle->iic_addr, TSL2561_REG_INTERRUPT, (uint8_t *)&prev, 1);          /* read interrupt */
    if (res != 0)                                                                                  /* check result */
    {
        handle->debug_print("tsl2561: read failed.\n");                                            /* read interrupt failed */
        
        return 1;                                                                                  /* return error */
    }
    prev &= ~0x30;                                                                                 /* clear interrupt bit */
    prev |= enable << 4;                                                                           /* set interrupt */
    if (handle->iic_write(handle->iic_addr, TSL2561_REG_INTERRUPT, (uint8_t *)&prev, 1) != 0)      /* write interrupt config */
    {
        handle->debug_print("tsl2561: write failed.\n");                                           /* write failed */
        
        return 1;                                                                                  /* return error */
    }
    else
    {
        return 0;                                                                                  /* success return 0 */
    }
}

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
uint8_t tsl2561_get_interrupt(tsl2561_handle_t *handle, tsl2561_bool_t *enable)
{
    uint8_t res, prev;
    
    if (handle == NULL)                                                                          /* check handle */
    {
        return 2;                                                                                /* return error */
    }
    if (handle->inited != 1)                                                                     /* check handle initialization */
    {
        return 3;                                                                                /* return error */
    }
    
    res = handle->iic_read(handle->iic_addr, TSL2561_REG_INTERRUPT, (uint8_t *)&prev, 1);        /* read interrupt */
    if (res != 0)                                                                                /* check result */
    {
        handle->debug_print("tsl2561: read failed.\n");                                          /* read interrupt failed */
        
        return 1;                                                                                /* return error */
    }
    prev &= 0x30;                                                                                /* get interrupt bit */
    *enable = (tsl2561_bool_t)(prev >> 4);                                                       /* get interrupt */
    
    return 0;                                                                                    /* success return 0 */
}

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
uint8_t tsl2561_set_interrupt_high_threshold(tsl2561_handle_t *handle, uint16_t ch0_raw)
{
    uint8_t buf[2];
    
    if (handle == NULL)                                                                              /* check handle */
    {
        return 2;                                                                                    /* return error */
    }
    if (handle->inited != 1)                                                                         /* check handle initialization */
    {
        return 3;                                                                                    /* return error */
    }
    
    buf[0] = ch0_raw & 0xFF;                                                                         /* set ch0 raw LSB */
    buf[1] = (ch0_raw >> 8) & 0xFF;                                                                  /* set ch0 raw MSB */
    if (handle->iic_write(handle->iic_addr, TSL2561_REG_THRESHHIGHLOW, (uint8_t *)buf, 2) != 0)      /* write config */
    {
        handle->debug_print("tsl2561: write failed.\n");                                             /* write failed */
        
        return 1;                                                                                    /* return error */
    }
    else
    {
        return 0;                                                                                    /* success return 0 */
    }
}

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
uint8_t tsl2561_get_interrupt_high_threshold(tsl2561_handle_t *handle, uint16_t *ch0_raw)
{
    uint8_t res;
    uint8_t buf[2];
    
    if (handle == NULL)                                                                            /* check handle */
    {
        return 2;                                                                                  /* return error */
    }
    if (handle->inited != 1)                                                                       /* check handle initialization */
    {
        return 3;                                                                                  /* return error */
    }
    
    memset(buf, 0, sizeof(uint8_t) * 2);                                                           /* clear the buffer */
    res = handle->iic_read(handle->iic_addr, TSL2561_REG_THRESHHIGHLOW, (uint8_t *)buf, 2);        /* read config */
    if (res != 0)                                                                                  /*check the result */
    {
        handle->debug_print("tsl2561: write failed.\n");                                           /* write failed */
        
        return 1;                                                                                  /* return error */
    }
    *ch0_raw = ((uint16_t)buf[1] << 8) | buf[0];                                                   /* get ch0 raw */
    
    return 0;                                                                                      /* success return 0 */
}

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
uint8_t tsl2561_set_interrupt_low_threshold(tsl2561_handle_t *handle, uint16_t ch0_raw)
{
    uint8_t buf[2];
    
    if (handle == NULL)                                                                             /* check handle */
    {
        return 2;                                                                                   /* return error */
    }
    if (handle->inited != 1)                                                                        /* check handle initialization */
    {
        return 3;                                                                                   /* return error */
    }
    
    buf[0] = ch0_raw & 0xFF;                                                                        /* set ch0 raw LSB */
    buf[1] = (ch0_raw >> 8) & 0xFF;                                                                 /* set ch0 raw MSB */
    if (handle->iic_write(handle->iic_addr, TSL2561_REG_THRESHLOWLOW, (uint8_t *)buf, 2) != 0)      /* write config */
    {
        handle->debug_print("tsl2561: write failed.\n");                                            /* write failed */
        
        return 1;                                                                                   /* return error */
    }
    else
    {
        return 0;                                                                                   /* success return 0 */
    }
}

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
uint8_t tsl2561_get_interrupt_low_threshold(tsl2561_handle_t *handle, uint16_t *ch0_raw)
{
    uint8_t res;
    uint8_t buf[2];
    
    if (handle == NULL)                                                                           /* check handle */
    {
        return 2;                                                                                 /* return error */
    }
    if (handle->inited != 1)                                                                      /* check handle initialization */
    {
        return 3;                                                                                 /* return error */
    }
    
    memset(buf, 0, sizeof(uint8_t) * 2);                                                          /* clear the buffer */
    res = handle->iic_read(handle->iic_addr, TSL2561_REG_THRESHLOWLOW, (uint8_t *)buf, 2);        /* read config */
    if (res != 0)                                                                                 /* check result */
    {
        handle->debug_print("tsl2561: read failed.\n");                                           /* read failed */
       
        return 1;                                                                                 /* return error */
    }
    *ch0_raw = ((uint16_t)buf[1] << 8) | buf[0];                                                  /* get ch0 raw */
    
    return 0;                                                                                     /* return error */
}

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
uint8_t tsl2561_power_down(tsl2561_handle_t *handle)
{
    uint8_t reg;
 
    if (handle == NULL)                                                                         /* check handle */
    {
        return 2;                                                                               /* return error */
    }
    if (handle->inited != 1)                                                                    /* check handle initialization */
    {
        return 3;                                                                               /* return error */
    } 
    
    reg = TSL2561_CONTROL_POWEROFF;                                                             /* set power off command */
    if (handle->iic_write(handle->iic_addr, TSL2561_REG_CONTROL, (uint8_t *)&reg, 1) != 0)      /* write control */
    {
        handle->debug_print("tsl2561: write failed.\n");                                        /* write failed */
        
        return 1;                                                                               /* return error */
    }
    else
    {
        return 0;                                                                               /* success return 0 */
    }
}

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
uint8_t tsl2561_wake_up(tsl2561_handle_t *handle)
{
    uint8_t reg;
    
    if (handle == NULL)                                                                         /* check handle */
    {
        return 2;                                                                               /* return error */
    }
    if (handle->inited != 1)                                                                    /* check handle initialization */
    {
        return 3;                                                                               /* return error */
    }
    
    reg = TSL2561_CONTROL_POWERON;                                                              /* set power on command */
    if (handle->iic_write(handle->iic_addr, TSL2561_REG_CONTROL, (uint8_t *)&reg, 1) != 0)      /* write control */
    {
        handle->debug_print("tsl2561: write failed.\n");                                        /* write failed */
        
        return 1;                                                                               /* return error */
    }
    else
    {
        return 0;                                                                               /* success return 0 */
    }
}

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
uint8_t tsl2561_set_reg(tsl2561_handle_t *handle, uint8_t reg, uint8_t *buf, uint16_t len)
{
    if (handle == NULL)                                               /* check handle */
    {
        return 2;                                                     /* return error */
    }
    if (handle->inited != 1)                                          /* check handle initialization */
    {
        return 3;                                                     /* return error */
    }
    
    if (handle->iic_write(handle->iic_addr, reg, buf, len) != 0)      /* write data */
    {
        handle->debug_print("tsl2561: write failed.\n");              /* write failed */
        
        return 1;                                                     /* return error */
    }
    else
    {
        return 0;                                                     /* success return 0 */
    }
}

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
uint8_t tsl2561_get_reg(tsl2561_handle_t *handle, uint8_t reg, uint8_t *buf, uint16_t len)
{
    if (handle == NULL)                                              /* check handle */
    {
        return 2;                                                    /* return error */
    }
    if (handle->inited != 1)                                         /* check handle initialization */
    {
        return 3;                                                    /* return error */
    }
    
    if (handle->iic_read(handle->iic_addr, reg, buf, len) != 0)      /* read data */
    {
        handle->debug_print("tsl2561: read failed.\n");              /* read failed */
        
        return 1;                                                    /* return error */
    }
    else
    {
        return 0;                                                    /* success return 0 */
    }
}

/**
 * @brief      get chip's information
 * @param[out] *info points to a tsl2561 info structure
 * @return     status code
 *             - 0 success
 *             - 2 handle is NULL
 * @note       none
 */
uint8_t tsl2561_info(tsl2561_info_t *info)
{
    if (info == NULL)                                               /* check handle */
    {
        return 2;                                                   /* return error */
    }
    
    memset(info, 0, sizeof(tsl2561_info_t));                        /* initialize tsl2561 info structure */
    strncpy(info->chip_name, CHIP_NAME, 32);                        /* copy chip name */
    strncpy(info->manufacturer_name, MANUFACTURER_NAME, 32);        /* copy manufacturer name */
    strncpy(info->interface, "IIC", 8);                             /* copy interface name */
    info->supply_voltage_min_v = SUPPLY_VOLTAGE_MIN;                /* set minimal supply voltage */
    info->supply_voltage_max_v = SUPPLY_VOLTAGE_MAX;                /* set maximum supply voltage */
    info->max_current_ma = MAX_CURRENT;                             /* set maximum current */
    info->temperature_max = TEMPERATURE_MAX;                        /* set minimal temperature */
    info->temperature_min = TEMPERATURE_MIN;                        /* set maximum temperature */
    info->driver_version = DRIVER_VERSION;                          /* set driver version */
    
    return 0;                                                       /* success return 0 */
}
