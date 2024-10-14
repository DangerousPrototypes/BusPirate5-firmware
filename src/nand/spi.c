/**
 * @file		spi.c
 * @author		Andrew Loebs
 * @brief		Implementation file of the spi module
 *
 */
#include <stdint.h>
#include "pico/stdlib.h"
#include "pirate.h"
#include "hardware/spi.h"
#include "pirate/bio.h"
#include "system_config.h"
#include "ui/ui_term.h"
#include "spi.h"
#include "nand/sys_time.h"

// public function definitions
/*
void nand_spi_init(void)
{
    uint64_t baudrate=spi_init(BP_SPI_PORT, 1000*1000*1);
    //printf("\r\n%s%s:%s %uMHz",ui_term_color_notice(), GET_T(T_HWSPI_ACTUAL_SPEED_KHZ), ui_term_color_reset(),
baudrate/1000000); spi_set_format(BP_SPI_PORT,8, SPI_CPOL_0, SPI_CPHA_0, SPI_MSB_FIRST);

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
    bio_put(M_SPI_CS, 1);

    // 8bit and lsb/msb handled in UI.c
    //dff=SPI_CR1_DFF_8BIT;
    //lsbfirst=SPI_CR1_MSBFIRST;
    system_bio_claim(true, M_SPI_CLK, BP_PIN_MODE, pin_labels[0]);
    system_bio_claim(true, M_SPI_CDO, BP_PIN_MODE, pin_labels[1]);
    system_bio_claim(true, M_SPI_CDI, BP_PIN_MODE, pin_labels[2]);
    system_bio_claim(true, M_SPI_CS, BP_PIN_MODE, pin_labels[3]);

}
*/

int nand_spi_write(const uint8_t* write_buff, size_t write_len, uint32_t timeout_ms) {
    // validate input
    if (!write_buff) {
        return SPI_RET_NULL_PTR;
    }

    // perform transfer
    uint32_t start_time = sys_time_get_ms();
    for (int i = 0; i < write_len; i++) {
        // block until tx empty or timeout
        while (!spi_is_writable(BP_SPI_PORT)) {
            if (sys_time_is_elapsed(start_time, timeout_ms)) {
                return SPI_RET_TIMEOUT;
            }
        }
        // transmit data
        // LL_SPI_TransmitData8(SPI_INSTANCE, write_buff[i]);
        spi_get_hw(BP_SPI_PORT)->dr = write_buff[i];

        /*// block until rx not empty
        while (!LL_SPI_IsActiveFlag_RXNE(SPI_INSTANCE)) {
            if (sys_time_is_elapsed(start_time, timeout_ms)) {
                return SPI_RET_TIMEOUT;
            }
        }
        // read data to clear buffer
        LL_SPI_ReceiveData8(SPI_INSTANCE);*/
        // Drain RX FIFO, then wait for shifting to finish (which may be *after*
        // TX FIFO drains), then drain RX FIFO again
        while (spi_is_readable(BP_SPI_PORT)) {
            (void)spi_get_hw(BP_SPI_PORT)->dr;
        }

        while (spi_get_hw(BP_SPI_PORT)->sr & SPI_SSPSR_BSY_BITS) {
            tight_loop_contents();
        }

        while (spi_is_readable(BP_SPI_PORT)) {
            (void)spi_get_hw(BP_SPI_PORT)->dr;
        }

        // Don't leave overrun flag set
        spi_get_hw(BP_SPI_PORT)->icr = SPI_SSPICR_RORIC_BITS;
    }

    return SPI_RET_OK;
}

int nand_spi_read(uint8_t* read_buff, size_t read_len, uint32_t timeout_ms) {
    // validate input
    if (!read_buff) {
        return SPI_RET_NULL_PTR;
    }

    // perform transfer
    uint32_t start_time = sys_time_get_ms();
    for (int i = 0; i < read_len; i++) {
        // block until tx empty or timeout
        while (!spi_is_writable(BP_SPI_PORT)) {
            if (sys_time_is_elapsed(start_time, timeout_ms)) {
                return SPI_RET_TIMEOUT;
            }
        }
        // transmit data
        // LL_SPI_TransmitData8(SPI_INSTANCE, 0);
        spi_get_hw(BP_SPI_PORT)->dr = 0;

        // block until rx not empty
        while (!spi_is_readable(BP_SPI_PORT)) {
            if (sys_time_is_elapsed(start_time, timeout_ms)) {
                return SPI_RET_TIMEOUT;
            }
        }
        // read data from buffer
        read_buff[i] = (uint8_t)spi_get_hw(BP_SPI_PORT)->dr;
    }

    return SPI_RET_OK;
}

int nand_spi_write_read(const uint8_t* write_buff, uint8_t* read_buff, size_t transfer_len, uint32_t timeout_ms) {
    // validate input
    if (!read_buff) {
        return SPI_RET_NULL_PTR;
    }

    // perform transfer
    uint32_t start_time = sys_time_get_ms();
    for (int i = 0; i < transfer_len; i++) {
        // block until tx empty or timeout
        while (!spi_is_writable(BP_SPI_PORT)) {
            if (sys_time_is_elapsed(start_time, timeout_ms)) {
                return SPI_RET_TIMEOUT;
            }
        }
        // transmit data
        // LL_SPI_TransmitData8(SPI_INSTANCE, write_buff[i]);
        spi_get_hw(BP_SPI_PORT)->dr = write_buff[i];

        // block until rx not empty
        while (!spi_is_readable(BP_SPI_PORT)) {
            if (sys_time_is_elapsed(start_time, timeout_ms)) {
                return SPI_RET_TIMEOUT;
            }
        }
        // read data from buffer
        read_buff[i] = spi_get_hw(BP_SPI_PORT)->dr;
    }

    return SPI_RET_OK;
}
