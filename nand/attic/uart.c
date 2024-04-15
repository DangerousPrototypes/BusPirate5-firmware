/**
 * @file		uart.c
 * @author		Andrew Loebs
 * @brief		Implementation file of the UART module
 *
 */

#include "uart.h"

#include <stdint.h>
#include <stdlib.h>

#include "../st/ll/stm32l4xx_ll_bus.h"
#include "../st/ll/stm32l4xx_ll_gpio.h"
#include "../st/ll/stm32l4xx_ll_rcc.h"
#include "../st/ll/stm32l4xx_ll_usart.h"
#include "../st/stm32l4xx.h"

#include "fifo.h"
#include "sys_time.h"

// defines
#define VCP_TX_PORT GPIOA
#define VCP_TX_PIN  LL_GPIO_PIN_2
#define VCP_RX_PORT GPIOA
#define VCP_RX_PIN  LL_GPIO_PIN_15

#define VCP_UART USART2

#define BAUD_RATE    115200
#define PUTC_TIMEOUT 10 // ms

#define UART_PREEMPT_PRIORITY 7 // low
#define UART_SUB_PRIORITY     0

#define RX_BUFF_LEN 256

// private function prototypes
static uint32_t get_pclk1_hz(void);

// private variables
static uint8_t rx_fifo_memblock[RX_BUFF_LEN];
static fifo_t rx_fifo = FIFO_STATIC_INIT(rx_fifo_memblock, sizeof(rx_fifo_memblock));

// public function definitions
void uart_init(void)
{
    // enable peripheral clocks
    LL_APB1_GRP1_EnableClock(LL_APB1_GRP1_PERIPH_USART2);
    if (!LL_AHB2_GRP1_IsEnabledClock(LL_AHB2_GRP1_PERIPH_GPIOA))
        LL_AHB2_GRP1_EnableClock(LL_AHB2_GRP1_PERIPH_GPIOA);

    // setup pins
    LL_GPIO_SetPinMode(VCP_TX_PORT, VCP_TX_PIN, LL_GPIO_MODE_ALTERNATE);
    LL_GPIO_SetPinOutputType(VCP_TX_PORT, VCP_TX_PIN, LL_GPIO_OUTPUT_PUSHPULL);
    LL_GPIO_SetPinSpeed(VCP_TX_PORT, VCP_TX_PIN, LL_GPIO_SPEED_FREQ_VERY_HIGH);
    LL_GPIO_SetPinPull(VCP_TX_PORT, VCP_TX_PIN, LL_GPIO_PULL_NO);
    LL_GPIO_SetAFPin_0_7(VCP_TX_PORT, VCP_TX_PIN, LL_GPIO_AF_7);

    LL_GPIO_SetPinMode(VCP_RX_PORT, VCP_RX_PIN, LL_GPIO_MODE_ALTERNATE);
    LL_GPIO_SetPinOutputType(VCP_RX_PORT, VCP_RX_PIN, LL_GPIO_OUTPUT_PUSHPULL);
    LL_GPIO_SetPinSpeed(VCP_RX_PORT, VCP_RX_PIN, LL_GPIO_SPEED_FREQ_VERY_HIGH);
    LL_GPIO_SetPinPull(VCP_RX_PORT, VCP_RX_PIN, LL_GPIO_PULL_NO);
    LL_GPIO_SetAFPin_8_15(VCP_RX_PORT, VCP_RX_PIN, LL_GPIO_AF_3);

    // configure UART CR1
    LL_USART_SetDataWidth(VCP_UART, LL_USART_DATAWIDTH_8B);
    LL_USART_SetParity(VCP_UART, LL_USART_PARITY_NONE);
    LL_USART_EnableDirectionTx(VCP_UART);
    LL_USART_EnableDirectionRx(VCP_UART);
    LL_USART_SetOverSampling(VCP_UART, LL_USART_OVERSAMPLING_16);

    // configure UART CR2
    LL_USART_SetStopBitsLength(VCP_UART, LL_USART_STOPBITS_1);

    // configure UART CR3
    LL_USART_DisableOneBitSamp(VCP_UART);
    LL_USART_SetHWFlowCtrl(VCP_UART, LL_USART_HWCONTROL_NONE);

    // config UART BRR
    LL_USART_SetBaudRate(VCP_UART, get_pclk1_hz(), LL_USART_OVERSAMPLING_16, BAUD_RATE);

    // enable UART interrupt for rx
    LL_USART_EnableIT_RXNE(VCP_UART);
    NVIC_SetPriority(USART2_IRQn, NVIC_EncodePriority(NVIC_GetPriorityGrouping(),
                                                      UART_PREEMPT_PRIORITY, UART_SUB_PRIORITY));
    NVIC_EnableIRQ(USART2_IRQn);

    // enable UART
    LL_USART_Enable(VCP_UART);
}

void _uart_putc(char c)
{
    // Blocking single-byte transmit with timeout
    uint32_t start = sys_time_get_ms();
    LL_USART_TransmitData8(VCP_UART, c);
    // wait for completion
    while (!LL_USART_IsActiveFlag_TC(VCP_UART) && !sys_time_is_elapsed(start, PUTC_TIMEOUT))
        ;
}

bool _uart_try_getc(char *c) { return fifo_try_dequeue(&rx_fifo, c); }

void _uart_isr(void)
{
    // if there is an overrun flag, clear it
    if (LL_USART_IsActiveFlag_ORE(VCP_UART)) {
        LL_USART_ClearFlag_ORE(VCP_UART);
    }
    // if there is an rxn flag, read byte
    if (LL_USART_IsActiveFlag_RXNE(VCP_UART)) {
        // TODO: grab the return value from this and log a "uart fifo full"
        // status somewhere if false
        fifo_enqueue(&rx_fifo, LL_USART_ReceiveData8(VCP_UART));
    }
}

// private function definitions
static uint32_t get_pclk1_hz(void)
{
    uint32_t pclk1 = SystemCoreClock;
    switch (LL_RCC_GetAPB1Prescaler()) {
        case LL_RCC_APB1_DIV_1:
            // do nothing
            break;
        case LL_RCC_APB1_DIV_2:
            pclk1 /= 2;
            break;
        case LL_RCC_APB1_DIV_4:
            pclk1 /= 4;
            break;
        case LL_RCC_APB1_DIV_8:
            pclk1 /= 8;
            break;
        case LL_RCC_APB1_DIV_16:
            pclk1 /= 16;
            break;
        default:
            break;
    }

    return pclk1;
}
