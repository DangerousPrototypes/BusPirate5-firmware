#include <stdio.h>
#include "pico/stdlib.h"
#include "hardware/spi.h"
#include "pirate.h"
#include "command_struct.h"
#include "display/scope.h"

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

void shift_write_wait(uint8_t *level, uint8_t *direction) {
    // I inverted it to clear and then set for easier use with amux
    spi_busy_wait(true);
    
    spi_set_baudrate(BP_SPI_PORT, BP_SPI_SHIFT_SPEED); // 595s can't go full speed at low temperatures
    spi_write_blocking(BP_SPI_PORT, level, 2);
    gpio_put(SHIFT_LATCH, 1);
    busy_wait_us(1);
    gpio_put(SHIFT_LATCH, 0);
    spi_set_baudrate(BP_SPI_PORT, BP_SPI_HIGH_SPEED);

    spi_busy_wait(false);

}
