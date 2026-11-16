/**
 * @file shift.c
 * @brief 74HC595 shift register control implementation.
 * @details Controls 16-bit I/O expansion using two cascaded 74HC595 shift registers.
 *          Used on BP5 Rev8/Rev9 for hardware control that requires more GPIO pins
 *          than the RP2040 package provides:
 *          - Display backlight and reset
 *          - PSU current limit control and fuse reset
 *          - Analog multiplexer channel selection
 *          
 *          Data format: 2 bytes (16 bits) sent MSB first via SPI
 *          Latch timing: Pulse SHIFT_LATCH high to update outputs
 *          Speed: Runs at reduced SPI speed (BP_SPI_SHIFT_SPEED) for reliability
 */

#include <stdio.h>
#include "pico/stdlib.h"
#include "hardware/spi.h"
#include "hardware/clocks.h"
#include "pirate.h"
#include "command_struct.h"
#include "display/scope.h"

static uint8_t shift_out[2] = { 0, 0 };

// spi_set_baudrate() searches for the clock divider on every call, about 80 us
// on the RP2040. shift_clear_set() switches the shared SPI bus to the shift
// register speed and back on every call, four times per channel of the pin
// voltage sweep, so each rate keeps the divider registers it computed and
// reapplies them directly. They are recomputed if clk_peri changes.
typedef struct {
    uint baudrate;
    uint32_t freq_in; // clk_peri the registers were computed for, 0 = not yet
    uint32_t cpsr;
    uint32_t cr0_scr;
} spi_rate_t;

static spi_rate_t shift_rate = { .baudrate = BP_SPI_SHIFT_SPEED };
static spi_rate_t high_rate = { .baudrate = BP_SPI_HIGH_SPEED };

static void spi_apply_rate(spi_inst_t* spi, spi_rate_t* rate) {
    uint32_t freq_in = clock_get_hz(clk_peri);
    if (rate->freq_in != freq_in) {
        spi_set_baudrate(spi, rate->baudrate);
        rate->cpsr = spi_get_hw(spi)->cpsr;
        rate->cr0_scr = spi_get_hw(spi)->cr0 & SPI_SSPCR0_SCR_BITS;
        rate->freq_in = freq_in;
        return;
    }
    // Same register sequence as spi_set_baudrate(), without the divider search.
    uint32_t enable_mask = spi_get_hw(spi)->cr1 & SPI_SSPCR1_SSE_BITS;
    hw_clear_bits(&spi_get_hw(spi)->cr1, SPI_SSPCR1_SSE_BITS);
    spi_get_hw(spi)->cpsr = rate->cpsr;
    hw_write_masked(&spi_get_hw(spi)->cr0, rate->cr0_scr, SPI_SSPCR0_SCR_BITS);
    hw_set_bits(&spi_get_hw(spi)->cr1, enable_mask);
}

void shift_init(void) {
    gpio_set_function(SHIFT_EN, GPIO_FUNC_SIO);
    gpio_put(SHIFT_EN, 1); // active low
    gpio_set_dir(SHIFT_EN, GPIO_OUT);

    gpio_set_function(SHIFT_LATCH, GPIO_FUNC_SIO);
    gpio_put(SHIFT_LATCH, 0);
    gpio_set_dir(SHIFT_LATCH, GPIO_OUT);
}

void shift_output_enable(bool enable) {
    if (enable) {
        gpio_put(SHIFT_EN, 0); // active low, enable shift register outputs
    } else {
        gpio_put(SHIFT_EN, 1); // high, disable shift register outputs
    }
    busy_wait_us(5);
}

void shift_clear_set(uint16_t clear_bits, uint16_t set_bits, bool busy_wait) {
    extern uint8_t shift_out[2];
    // I inverted it to clear and then set for easier use with amux
    if (busy_wait) {
        spi_busy_wait(true);
    }
    spi_apply_rate(BP_SPI_PORT, &shift_rate); // 595s can't go full speed at low temperatures
    shift_out[1] &= ~((uint8_t)clear_bits);
    shift_out[0] &= ~((uint8_t)(clear_bits >> 8));
    shift_out[1] |= (uint8_t)set_bits;
    shift_out[0] |= (uint8_t)(set_bits >> 8);
    spi_write_blocking(BP_SPI_PORT, shift_out, 2);
    gpio_put(SHIFT_LATCH, 1);
    busy_wait_us(1);
    gpio_put(SHIFT_LATCH, 0);
    spi_apply_rate(BP_SPI_PORT, &high_rate);
    if (busy_wait) {
        spi_busy_wait(false);
    }
}

void shift_clear_set_wait(uint16_t clear_bits, uint16_t set_bits) {
    shift_clear_set(clear_bits, set_bits, true);
}
#if 0
void shift_adc_select(uint8_t channel)
{
    extern uint8_t shift_out[2];

    if (scope_running) // scope is using the analog subsystem
       return;

    shift_out[1]&=~((uint8_t)(0b1111<<1)); //clear the amux control bits      
    shift_out[1]|=(uint8_t)(channel<<1); //set the amux channel bits
      
    spi_busy_wait(true);
    
    //uint32_t baud=spi_get_baudrate(BP_SPI_PORT);
    //spi_set_baudrate(BP_SPI_PORT, 1000 * 1000 * 32); // max 10mhz?
    
    spi_write_blocking(BP_SPI_PORT, shift_out, 2);
    gpio_put(SHIFT_LATCH, 1);
    busy_wait_us(5);
    gpio_put(SHIFT_LATCH, 0); 
    
    //spi_set_baudrate(BP_SPI_PORT, baud);   
    spi_busy_wait(false);
}
#endif