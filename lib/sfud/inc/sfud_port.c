/*
 * This file is part of the Serial Flash Universal Driver Library.
 *
 * Copyright (c) 2016-2018, Armink, <armink.ztl@gmail.com>
 *
 * Permission is hereby granted, free of charge, to any person obtaining
 * a copy of this software and associated documentation files (the
 * 'Software'), to deal in the Software without restriction, including
 * without limitation the rights to use, copy, modify, merge, publish,
 * distribute, sublicense, and/or sell copies of the Software, and to
 * permit persons to whom the Software is furnished to do so, subject to
 * the following conditions:
 *
 * The above copyright notice and this permission notice shall be
 * included in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED 'AS IS', WITHOUT WARRANTY OF ANY KIND,
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
 * IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY
 * CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT,
 * TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE
 * SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 *
 * Function: Portable interface for each platform.
 * Created on: 2016-04-23
 */



#include <stdio.h>
#include "pico/stdlib.h"
#include <stdint.h>
#include "hardware/spi.h"
#include "pirate.h"
#include <stdarg.h>
#include "sfud.h"
#include "bio.h"

#define M_SPI_PORT spi1
#define M_SPI_CLK BIO6
#define M_SPI_CDO BIO7
#define M_SPI_CDI BIO4
#define M_SPI_CS BIO5

#define M_SPI_SELECT 0
#define M_SPI_DESELECT 1

static char log_buf[256];

void sfud_log_debug(const char *file, const long line, const char *format, ...);

/**
 * SPI write data then read data
 */
static sfud_err spi_write_read(const sfud_spi *spi, const uint8_t *write_buf, size_t write_size, uint8_t *read_buf, size_t read_size) 
{
    sfud_err result = SFUD_SUCCESS;
    uint8_t send_data, read_data;

    /**
     * add your spi write and read code
     */

    if (write_size) {
        SFUD_ASSERT(write_buf);
    }
    if (read_size) {
        SFUD_ASSERT(read_buf);
    }

    //CS low
    bio_put(M_SPI_CS, 0);
    for (size_t i = 0, retry_times; i < write_size + read_size; i++) {
        if (i < write_size) {
            send_data = *write_buf++;
        } else {
            send_data = SFUD_DUMMY_DATA;
        }

        while(!spi_is_writable(M_SPI_PORT));
	
        spi_get_hw(M_SPI_PORT)->dr = (uint32_t)send_data;

        while(!spi_is_readable(M_SPI_PORT));
        
        read_data= (uint8_t)spi_get_hw(M_SPI_PORT)->dr;

        // Write to TX FIFO whilst ignoring RX, then clean up afterward. When RX
        // is full, PL022 inhibits RX pushes, and sets a sticky flag on
        // push-on-full, but continues shifting. Safe if SSPIMSC_RORIM is not set.
        /*
        while(!spi_is_writable(M_SPI_PORT))
        {
            tight_loop_contents();
        }

        spi_get_hw(M_SPI_PORT)->dr = (uint32_t)send_data;

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
            read_data=spi_get_hw(M_SPI_PORT)->dr;
        }

        // Don't leave overrun flag set
        spi_get_hw(M_SPI_PORT)->icr = SPI_SSPICR_RORIC_BITS;
        */

/*
        retry_times = 1000;
        while (SPI_I2S_GetFlagStatus(spi_dev->spix, SPI_I2S_FLAG_TXE) == RESET) {
            SFUD_RETRY_PROCESS(NULL, retry_times, result);
        }
        if (result != SFUD_SUCCESS) {
            goto exit;
        }
        SPI_I2S_SendData(spi_dev->spix, send_data);

        retry_times = 1000;
        while (SPI_I2S_GetFlagStatus(spi_dev->spix, SPI_I2S_FLAG_RXNE) == RESET) {
            SFUD_RETRY_PROCESS(NULL, retry_times, result);
        }
        if (result != SFUD_SUCCESS) {
            goto exit;
        }
        read_data = SPI_I2S_ReceiveData(spi_dev->spix);
*/

        if (i >= write_size) {
            *read_buf++ = read_data;
        }
    }

exit:
    //CS High
    bio_put(M_SPI_CS, 1);

    return result;
}

#ifdef SFUD_USING_QSPI
/**
 * read flash data by QSPI
 */
static sfud_err qspi_read(const struct __sfud_spi *spi, uint32_t addr, sfud_qspi_read_cmd_format *qspi_read_cmd_format,
        uint8_t *read_buf, size_t read_size) {
    sfud_err result = SFUD_SUCCESS;

    /**
     * add your qspi read flash data code
     */

    return result;
}
#endif /* SFUD_USING_QSPI */

void delay_100us(void)
{
    busy_wait_us(100);
}

sfud_err sfud_spi_port_init(sfud_flash *flash) {
    sfud_err result = SFUD_SUCCESS;

    /**
     * add your port spi bus and device object initialize code like this:
     * 1. rcc initialize
     * 2. gpio initialize
     * 3. spi device initialize
     * 4. flash->spi and flash->retry item initialize
     *    flash->spi.wr = spi_write_read; //Required
     *    flash->spi.qspi_read = qspi_read; //Required when QSPI mode enable
     *    flash->spi.lock = spi_lock;
     *    flash->spi.unlock = spi_unlock;
     *    flash->spi.user_data = &spix;
     *    flash->retry.delay = null;
     *    flash->retry.times = 10000; //Required
     */
    //switch (flash->index) {
        //case SFUD_SST25_DEVICE_INDEX: {
            flash->spi.wr = spi_write_read;
            //flash->spi.lock = spi_lock;
            //flash->spi.unlock = spi_unlock;
            //flash->spi.user_data = &spi1;
            /* about 100 microsecond delay */
            flash->retry.delay = &delay_100us;
            /* adout 60 seconds timeout */
            flash->retry.times = 60 * 10000;

            //break;
        //}

    return result;
}

/**
 * This function is print debug info.
 *
 * @param file the file which has call this function
 * @param line the line number which has call this function
 * @param format output format
 * @param ... args
 */
void sfud_log_debug(const char *file, const long line, const char *format, ...) {
    va_list args;

    /* args point to the first variable parameter */
    va_start(args, format);
    printf("[SFUD](%s:%ld) ", file, line);
    /* must use vprintf to print */
    vsnprintf(log_buf, sizeof(log_buf), format, args);
    printf("%s\r\n", log_buf);
    va_end(args);
}

/**
 * This function is print routine info.
 *
 * @param format output format
 * @param ... args
 */
void sfud_log_info(const char *format, ...) {
    va_list args;

    /* args point to the first variable parameter */
    va_start(args, format);
    printf("[SFUD]");
    /* must use vprintf to print */
    vsnprintf(log_buf, sizeof(log_buf), format, args);
    printf("%s\r\n", log_buf);
    va_end(args);
}
