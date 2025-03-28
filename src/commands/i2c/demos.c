#include <stdio.h>
#include "pico/stdlib.h"
#include <stdint.h>
#include "pirate.h"
#include "pirate/hwi2c_pio.h"
#include "ui/ui_term.h"
#include "system_config.h"
#include "command_struct.h"
#include "bytecode.h"
#include "mode/hwi2c.h"
#include "ui/ui_help.h"
#include "ui/ui_cmdln.h"
#include "lib/ms5611/ms5611.h"
#include "lib/tsl2561/driver_tsl2561.h"
#include "binmode/fala.h"

void demo_tsl2561(struct command_result* res) {
    // select register [0b01110010 0b11100000]
    //  start device [0b01110010 3]
    // confirm start [0b01110011 r]
    //  select ID register [0b01110010 0b11101010]
    //  read ID register [0b01110011 r] 7:4 0101 = TSL2561T 3:0 0 = revision
    //  select ADC register [0b01110010 0b11101100]
    // 0b11011100
    uint16_t chan0, chan1;
    uint8_t data[4];
    printf("%s\r\n", GET_T(T_HELP_I2C_TSL2561));

    //we manually control any FALA capture
    fala_start_hook();

    // select register [0b01110010 0b11100000]
    data[0] = 0b11100000;
    if (pio_i2c_write_array_timeout(0b01110010u, data, 1u, 0xffffu)) {
        goto tsl2561_error;
    }
    // start device [0b01110010 3]
    data[0] = 3;
    if (pio_i2c_write_array_timeout(0b01110010u, data, 1u, 0xffffu)) {
        goto tsl2561_error;
    }
    busy_wait_ms(500);
    // select ID register [0b01110010 0b11101010]
    // read ID register [0b01110011 r] 7:4 0101 = TSL2561T 3:0 0 = revision
    data[0] = 0b11101010;
    if (pio_i2c_transaction_array_timeout(0b01110010u, data, 1u, data, 1u, 0xffffu)) {
        goto tsl2561_error;
    }
    printf("ID: %d REV: %d\r\n", data[0] >> 4, data[0] & 0b1111u);
    // select ADC register [0b01110010 0b11101100]
    data[0] = 0b11101100;
    if (pio_i2c_transaction_array_timeout(0b01110010u, data, 1u, data, 4u, 0xffffu)) {
        goto tsl2561_error;
    }
    fala_stop_hook();

    chan0 = data[1] << 8 | data[0];
    chan1 = data[3] << 8 | data[2];

    uint32_t lux1 = a_tsl2561_calculate_lux(0u, 2u, chan0, chan1);

    printf("Chan0: %d Chan1: %d LUX: %d\r\n", chan0, chan1, lux1);
    goto tsl2561_cleanup;

tsl2561_error:
    pio_i2c_stop_timeout(0xffff); //force both lines back high
    res->error = 1;
    fala_stop_hook();
    printf("Device not detected (no ACK)\r\n");
tsl2561_cleanup:
    //we manually control any FALA capture
    fala_notify_hook();
}

void demo_ms5611(struct command_result* res) {
    // PS high, CSB low
    // reset [0b11101110 0b00011110]
    // PROM read [0b11101110 0b10100110] [0b11101111 r:2]
    // start conversion [0b11101110 0b01001000]
    // ADC read [0b11101110 0] [0b11101111 r:3]
    float temperature;
    float pressure;
    printf("%s\r\n", GET_T(T_HELP_I2C_MS5611));

    //we manually control any FALA capture
    fala_start_hook();

    if (ms5611_read_temperature_and_pressure_simple(&temperature, &pressure)) {
        goto ms5611_error;
    }
    fala_stop_hook();

    printf("Temperature: %f\r\nPressure: %f\r\n", temperature, pressure);
    goto ms5611_cleanup;

ms5611_error:
    pio_i2c_stop_timeout(0xffff); //force both lines back high
    res->error = 1;
    fala_stop_hook();
    printf("Device not detected (no ACK)\r\n");
ms5611_cleanup:
    //we manually control any FALA capture
    fala_notify_hook();
}

void demo_si7021(struct command_result* res) {
    uint8_t data[8];
    printf("%s\r\n", GET_T(T_HELP_I2C_SI7021));

    //we manually control any FALA capture
    fala_start_hook();

    // humidity
    data[0] = 0xf5;
    if (pio_i2c_write_array_timeout(0x80, data, 1, 0xffff)) {
        printf("Error writing humidity register\r\n");
        goto si7021_error;
    }
    busy_wait_ms(23); // delay for max conversion time
    if (pio_i2c_read_array_timeout(0x81, data, 2, 0xffff)) {
        printf("Error reading humidity data\r\n");
        goto si7021_error;
    }

    float f = (float)((float)(125 * (data[0] << 8 | data[1])) / 65536) - 6;
    printf("Humidity: %.2f%% (%#04x %#04x)\r\n", f, data[0], data[1]);

    // temperature [0x80 0xe0] [0x81 r:2]
    data[0] = 0xf3;
    if (pio_i2c_write_array_timeout(0x80, data, 1, 0xffff)) {
        goto si7021_error;
    }
    busy_wait_ms(100); // delay for max conversion time
    if (pio_i2c_read_array_timeout(0x81, data, 2, 0xffff)) {
        goto si7021_error;
    }

    f = (float)((float)(175.72 * (data[0] << 8 | data[1])) / 65536) - 46.85;
    printf("Temperature: %.2fC (%#04x %#04x)\r\n", f, data[0], data[1]);

    // SN
    data[0] = 0xfa;
    data[1] = 0xf0;
    uint8_t sn[8];
    if (pio_i2c_transaction_array_timeout(0x80, data, 2, data, 8, 0xffff)) {
        goto si7021_error;
    }
    sn[2] = data[6];
    sn[3] = data[4];
    sn[4] = data[2];
    sn[5] = data[0];

    data[0] = 0xfc;
    data[1] = 0xc9;
    if (pio_i2c_transaction_array_timeout(0x80, data, 2, data, 6, 0xffff)) {
        goto si7021_error;
    }
    fala_stop_hook();

    sn[0] = data[1];
    sn[1] = data[0];
    sn[6] = data[4];
    sn[7] = data[3];
    printf("Serial Number: 0x%02x%02x%02x%02x%02x%02x%02x%02x\r\n",
           sn[7],
           sn[6],
           sn[5],
           sn[4],
           sn[3],
           sn[2],
           sn[1],
           sn[0]);
    
    goto si7021_cleanup;

si7021_error:    
    pio_i2c_stop_timeout(0xffff); //force both lines back high
    res->error = 1;
    fala_stop_hook();
    printf("Device not detected (no ACK)\r\n");       
si7021_cleanup:
    //we manually control any FALA capture
    fala_notify_hook();
}