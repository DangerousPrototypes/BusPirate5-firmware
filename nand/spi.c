/**
 * @file		spi.c
 * @author		Andrew Loebs
 * @brief		Implementation file of the spi module
 *
 */

#include "spi.h"

#include "../st/ll/stm32l4xx_ll_bus.h"
#include "../st/ll/stm32l4xx_ll_gpio.h"
#include "../st/ll/stm32l4xx_ll_spi.h"

#include "sys_time.h"

// defines
#define SPI_INSTANCE SPI1

#define MOSI_PORT GPIOA
#define MOSI_PIN  LL_GPIO_PIN_7
#define MOSI_AF   LL_GPIO_AF_5

#define MISO_PORT GPIOA
#define MISO_PIN  LL_GPIO_PIN_6
#define MISO_AF   LL_GPIO_AF_5

#define SCK_PORT GPIOA
#define SCK_PIN  LL_GPIO_PIN_1
#define SCK_AF   LL_GPIO_AF_5

// public function definitions
void spi_init(void)
{
    // enable peripheral clocks
    LL_APB2_GRP1_EnableClock(LL_APB2_GRP1_PERIPH_SPI1);
    if (!LL_AHB2_GRP1_IsEnabledClock(LL_AHB2_GRP1_PERIPH_GPIOA))
        LL_AHB2_GRP1_EnableClock(LL_AHB2_GRP1_PERIPH_GPIOA);

    // configure pins
    LL_GPIO_SetPinMode(MOSI_PORT, MOSI_PIN, LL_GPIO_MODE_ALTERNATE);
    LL_GPIO_SetAFPin_0_7(MOSI_PORT, MOSI_PIN, MOSI_AF);
    LL_GPIO_SetPinSpeed(MOSI_PORT, MOSI_PIN, LL_GPIO_SPEED_FREQ_VERY_HIGH);
    LL_GPIO_SetPinPull(MOSI_PORT, MOSI_PIN, LL_GPIO_PULL_DOWN);

    LL_GPIO_SetPinMode(MISO_PORT, MISO_PIN, LL_GPIO_MODE_ALTERNATE);
    LL_GPIO_SetAFPin_0_7(MISO_PORT, MISO_PIN, MISO_AF);
    LL_GPIO_SetPinSpeed(MISO_PORT, MISO_PIN, LL_GPIO_SPEED_FREQ_VERY_HIGH);
    LL_GPIO_SetPinPull(MISO_PORT, MISO_PIN, LL_GPIO_PULL_DOWN);

    LL_GPIO_SetPinMode(SCK_PORT, SCK_PIN, LL_GPIO_MODE_ALTERNATE);
    LL_GPIO_SetAFPin_0_7(SCK_PORT, SCK_PIN, SCK_AF);
    LL_GPIO_SetPinSpeed(SCK_PORT, SCK_PIN, LL_GPIO_SPEED_FREQ_VERY_HIGH);
    LL_GPIO_SetPinPull(SCK_PORT, SCK_PIN, LL_GPIO_PULL_DOWN);

    // configure SPI module
    LL_SPI_SetBaudRatePrescaler(SPI_INSTANCE, LL_SPI_BAUDRATEPRESCALER_DIV2);
    LL_SPI_SetTransferDirection(SPI_INSTANCE, LL_SPI_FULL_DUPLEX);
    LL_SPI_SetClockPolarity(SPI_INSTANCE, LL_SPI_POLARITY_LOW);
    LL_SPI_SetClockPhase(SPI_INSTANCE, LL_SPI_PHASE_1EDGE);

    LL_SPI_SetDataWidth(SPI_INSTANCE, LL_SPI_DATAWIDTH_8BIT);
    LL_SPI_SetNSSMode(SPI_INSTANCE, LL_SPI_NSS_SOFT);
    LL_SPI_SetRxFIFOThreshold(SPI_INSTANCE, LL_SPI_RX_FIFO_TH_QUARTER);

    LL_SPI_SetMode(SPI_INSTANCE, LL_SPI_MODE_MASTER);
    LL_SPI_Enable(SPI_INSTANCE);
}

int spi_write(const uint8_t *write_buff, size_t write_len, uint32_t timeout_ms)
{
    // validate input
    if (!write_buff) return SPI_RET_NULL_PTR;

    // perform transfer
    uint32_t start_time = sys_time_get_ms();
    for (int i = 0; i < write_len; i++) {
        // block until tx empty or timeout
        while (!LL_SPI_IsActiveFlag_TXE(SPI_INSTANCE)) {
            if (sys_time_is_elapsed(start_time, timeout_ms)) {
                return SPI_RET_TIMEOUT;
            }
        }
        // transmit data
        LL_SPI_TransmitData8(SPI_INSTANCE, write_buff[i]);
        // block until rx not empty
        while (!LL_SPI_IsActiveFlag_RXNE(SPI_INSTANCE)) {
            if (sys_time_is_elapsed(start_time, timeout_ms)) {
                return SPI_RET_TIMEOUT;
            }
        }
        // read data to clear buffer
        LL_SPI_ReceiveData8(SPI_INSTANCE);
    }

    return SPI_RET_OK;
}

int spi_read(uint8_t *read_buff, size_t read_len, uint32_t timeout_ms)
{
    // validate input
    if (!read_buff) return SPI_RET_NULL_PTR;

    // perform transfer
    uint32_t start_time = sys_time_get_ms();
    for (int i = 0; i < read_len; i++) {
        // block until tx empty or timeout
        while (!LL_SPI_IsActiveFlag_TXE(SPI_INSTANCE)) {
            if (sys_time_is_elapsed(start_time, timeout_ms)) {
                return SPI_RET_TIMEOUT;
            }
        }
        // transmit data
        LL_SPI_TransmitData8(SPI_INSTANCE, 0);
        // block until rx not empty
        while (!LL_SPI_IsActiveFlag_RXNE(SPI_INSTANCE)) {
            if (sys_time_is_elapsed(start_time, timeout_ms)) {
                return SPI_RET_TIMEOUT;
            }
        }
        // read data from buffer
        read_buff[i] = LL_SPI_ReceiveData8(SPI_INSTANCE);
    }

    return SPI_RET_OK;
}

int spi_write_read(const uint8_t *write_buff, uint8_t *read_buff, size_t transfer_len,
                   uint32_t timeout_ms)
{
    // validate input
    if (!read_buff) return SPI_RET_NULL_PTR;

    // perform transfer
    uint32_t start_time = sys_time_get_ms();
    for (int i = 0; i < transfer_len; i++) {
        // block until tx empty or timeout
        while (!LL_SPI_IsActiveFlag_TXE(SPI_INSTANCE)) {
            if (sys_time_is_elapsed(start_time, timeout_ms)) {
                return SPI_RET_TIMEOUT;
            }
        }
        // transmit data
        LL_SPI_TransmitData8(SPI_INSTANCE, write_buff[i]);
        // block until rx not empty
        while (!LL_SPI_IsActiveFlag_RXNE(SPI_INSTANCE)) {
            if (sys_time_is_elapsed(start_time, timeout_ms)) {
                return SPI_RET_TIMEOUT;
            }
        }
        // read data from buffer
        read_buff[i] = LL_SPI_ReceiveData8(SPI_INSTANCE);
    }

    return SPI_RET_OK;
}
