/**
 * @file		main.c
 * @author		Andrew Loebs
 * @brief		Main application
 *
 */

#include <stdint.h>

#include "st/ll/stm32l4xx_ll_bus.h"
#include "st/ll/stm32l4xx_ll_rcc.h"
#include "st/ll/stm32l4xx_ll_system.h"
#include "st/ll/stm32l4xx_ll_utils.h"
#include "st/stm32_assert.h"

#include "modules/led.h"
#include "modules/mem.h"
#include "modules/shell.h"
#include "modules/spi.h"
#include "modules/sys_time.h"
#include "modules/uart.h"

#include "fatfs/ff.h"
#include "fatfs/ffconf.h"

// defines
#define STARTUP_LED_DURATION_MS 200

// private function prototypes
static void clock_config(void);

// private variables
FATFS fs; // file system object

// application main function
int main(void)
{
    // setup clock
    clock_config();

    // init base modules
    led_init();
    sys_time_init();
    uart_init();
    shell_init();
    spi_init();

    // blink LED to let user know we're on
    led_set_output(true);
    sys_time_delay(STARTUP_LED_DURATION_MS);
    led_set_output(false);

    // mount file system
    FRESULT res = f_mount(&fs, "", 1);
    if (FR_OK == res) {
        shell_prints_line("f_mount succeeded!");
    }
    else {
        shell_printf_line("f_mount failed, result: %d.", res);
    }

    // if filesystem mount failed due to no filesystem, attempt to make it
    if (FR_NO_FILESYSTEM == res) {
        shell_prints_line("No filesystem present. Attempting to make file system..");
        uint8_t *work_buffer = mem_alloc(FF_MAX_SS);
        if (!work_buffer) {
            shell_prints_line("Unable to allocate f_mkfs work buffer. File system not created.");
        }
        else {
            // make the file system
            res = f_mkfs("", 0, work_buffer, FF_MAX_SS);
            if (FR_OK != res) {
                shell_printf_line("f_mkfs failed, result: %d.", res); // fs make failure
            }
            else {
                shell_prints_line("f_mkfs succeeded!"); // fs make success
                // retry mount
                res = f_mount(&fs, "", 1);
                if (FR_OK == res) {
                    shell_prints_line("f_mount succeeded!");
                }
                else {
                    shell_printf_line("f_mount failed, result: %d.", res);
                }
            }

            mem_free(work_buffer);
        }
    }

    for (;;) {
        shell_tick();
    }
}

// private function definitions
static void clock_config(void)
{
    // enable prefetch & set latency -- icache and dcache are enabled by default
    LL_FLASH_EnablePrefetch(); // according to the ref manual, this negates perf impact due to flash
                               // latency
    LL_FLASH_SetLatency(LL_FLASH_LATENCY_4);
    // enable MSI
    LL_RCC_MSI_Enable();
    while (LL_RCC_MSI_IsReady() != 1)
        ; // TODO: add timeouts to these while loops

    // pll config & enable
    LL_RCC_PLL_ConfigDomain_SYS(LL_RCC_PLLSOURCE_MSI, LL_RCC_PLLM_DIV_1, 40, LL_RCC_PLLR_DIV_2);
    LL_RCC_PLL_Enable();
    LL_RCC_PLL_EnableDomain_SYS();
    while (LL_RCC_PLL_IsReady() != 1)
        ;

    // sys clk activation
    LL_RCC_SetAHBPrescaler(LL_RCC_SYSCLK_DIV_1);
    LL_RCC_SetSysClkSource(LL_RCC_SYS_CLKSOURCE_PLL);
    while (LL_RCC_GetSysClkSource() != LL_RCC_SYS_CLKSOURCE_STATUS_PLL)
        ;

    // setup APB1 & APB2
    LL_RCC_SetAPB1Prescaler(LL_RCC_APB1_DIV_1);
    LL_RCC_SetAPB2Prescaler(LL_RCC_APB2_DIV_1);

    // configure peripheral clock sources
    LL_RCC_SetUSARTClockSource(LL_RCC_USART2_CLKSOURCE_PCLK1);

    // update CMSIS variable
    LL_SetSystemCoreClock(80000000UL);
}
