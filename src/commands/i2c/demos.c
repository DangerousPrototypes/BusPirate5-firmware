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
#include "usb_rx.h"
#include "pio_config.h"

static bool i2c_transaction(uint8_t addr, uint8_t *write_data, uint8_t write_len, uint8_t *read_data, uint8_t read_len) {
    if (pio_i2c_transaction_array_repeat_start(addr, write_data, write_len, read_data, read_len, 0xffffu)) {
        printf("Device not detected (no ACK)\r\n");
        return true;
    }
    return false;
}

static bool i2c_write(uint8_t addr, uint8_t *data, uint8_t len) {
    hwi2c_status_t i2c_result = pio_i2c_write_array_timeout(addr, data, len, 0xfffffu);
    if(i2c_result != HWI2C_OK) {
        if(i2c_result == HWI2C_TIMEOUT) {
            printf("I2C Timeout\r\n");
        } else if(i2c_result == HWI2C_NACK) {
            printf("Device not detected (no ACK)\r\n");
        } else {
            printf("I2C Error: %d\r\n", i2c_result);
        }
        return true;
    }
    return false;
}

static bool i2c_read(uint8_t addr, uint8_t *data, uint8_t len) {
    hwi2c_status_t i2c_result = pio_i2c_read_array_timeout(addr | 1u, data, len, 0xfffffu);
    if(i2c_result != HWI2C_OK) {
        if(i2c_result == HWI2C_TIMEOUT) {
            printf("I2C Timeout\r\n");
        } else if(i2c_result == HWI2C_NACK) {
            printf("Device not detected (no ACK)\r\n");
        } else {
            printf("I2C Error: %d\r\n", i2c_result);
        }
        return true;
    }
    return false;
}

static const char* const sht4x_usage[] = {
    "sht4x [-h(elp)]",
    "- read SHT4x series temperature and humidity sensors",
    "- 1.08-3.3 volt device, pull-up resistors required",
    "Read SHT4x: sht4x",
};

static const struct ui_help_options sht4x_options[] = {0};

void demo_sht4x(struct command_result* res){
    printf("SHT40/41/43/45 Temperature and Humidity Sensor Demo\r\n");
    if (ui_help_show(res->help_flag||!ui_help_sanity_check(true,0x00), sht4x_usage, count_of(sht4x_usage), &sht4x_options[0], count_of(sht4x_options))) {
        return;
    }

    fala_start_hook();

    #define SHT4X_ADDRESS 0x44 << 1u // SHT4x default I2C address
    static uint8_t HIGH_REPEATABILITY_MEASURE[] = { 0xFD };
    if(i2c_write(SHT4X_ADDRESS, HIGH_REPEATABILITY_MEASURE, sizeof(HIGH_REPEATABILITY_MEASURE))) {
        goto sht4x_cleanup; // Error writing to the sensor
    }
    busy_wait_ms(9); // Wait for measurement to complete
    uint8_t data[6];
    if(i2c_read(SHT4X_ADDRESS, data, 6)) {
       goto sht4x_cleanup; // Error reading from the sensor
    }

    // Process the data
    uint16_t temp_raw = (data[0] << 8) | data[1];
    uint16_t hum_raw = (data[3] << 8) | data[4];

    float temperature = -45 + (175 * (temp_raw / 65535.0f));
    float humidity = -6 + 125 * (hum_raw / 65535.0f);

    printf("Temperature: %.2f °C (0x%02X 0x%02X)\r\n", temperature, data[0], data[1]);
    printf("Humidity: %.2f %% (0x%02X 0x%02X)\r\n", humidity, data[3], data[4]);

sht4x_cleanup:
    pio_i2c_stop_timeout(0xffff); //force both lines back high
    fala_stop_hook();
    //we manually control any FALA capture
    fala_notify_hook();       

}

static const char* const sht3x_usage[] = {
    "sht3x [-h(elp)]",
    "- read SHT3x series temperature and humidity sensors",
    "- 2.15-5 volt device, pull-up resistors required",
    "Read SHT3x: sht3x",
};

static const struct ui_help_options sht3x_options[] = {0};

void demo_sht3x(struct command_result* res) {
    printf("SHT30/31/35 Temperature and Humidity Sensor Demo\r\n");

    if(ui_help_show(res->help_flag||!ui_help_sanity_check(true,0x00), sht3x_usage, count_of(sht3x_usage), &sht3x_options[0], count_of(sht3x_options))) {
        return;
    }
    /*if (!ui_help_sanity_check(true,0x00)) {
        ui_help_show(true, tcs34725_usage, count_of(tcs34725_usage), &tcs34725_options[0], count_of(tcs34725_options));
        return;
    }*/
    fala_start_hook();

    #define SHT3X_ADDRESS 0x44 << 1u // SHT3x default I2C address
    static uint8_t CMD_SINGLE_SHOT_MEASURE[] = { 0x24, 0x00 };
    if(i2c_write(SHT3X_ADDRESS, CMD_SINGLE_SHOT_MEASURE, sizeof(CMD_SINGLE_SHOT_MEASURE))) {
        goto sht3x_cleanup; // Error writing to the sensor
    }
    busy_wait_ms(15); // Wait for measurement to complete
    uint8_t data[6];
    if(i2c_read(SHT3X_ADDRESS, data, 6)) {
        goto sht3x_cleanup; // Error reading from the sensor
    }

    // Process the data
    uint16_t temp_raw = (data[0] << 8) | data[1];
    uint16_t hum_raw = (data[3] << 8) | data[4];

    float temperature = -45 + (175 * (temp_raw / 65535.0f));
    float humidity = 100 * (hum_raw / 65535.0f);

    printf("Temperature: %.2f °C (0x%02X 0x%02X)\r\n", temperature, data[0], data[1]);
    printf("Humidity: %.2f %% (0x%02X 0x%02X)\r\n", humidity, data[3], data[4]);

sht3x_cleanup:
    pio_i2c_stop_timeout(0xffff); //force both lines back high
    fala_stop_hook();
    //we manually control any FALA capture
    fala_notify_hook();    
}

static const char* const tcs34725_usage[] = {
    "tcs3472 [-g <gain:1,4,16*,60x>] [-i <integration cycles:1-256*>] [-h(elp)]",
    "- read tcs3472x color sensor, show colors in terminal and on Bus Pirate LEDs",
    "- 3.3volt device, pull-up resistors required",
    "Read with default* 16x gain, 256 integration cycles: tcs3472",
    "Read with 60x gain, 10 integration cycles: tcs3472 -g 60 -i 10",
};

static const struct ui_help_options tcs34725_options[] = {0};

// based on https://github.com/ControlEverythingCommunity/TCS34725/blob/master/C/TCS34725.c
void demo_tcs34725(struct command_result* res) {
    if (ui_help_show(res->help_flag, tcs34725_usage, count_of(tcs34725_usage), &tcs34725_options[0], count_of(tcs34725_options))) {
        return;
    }
    if (!ui_help_sanity_check(true,0x00)) {
        ui_help_show(true, tcs34725_usage, count_of(tcs34725_usage), &tcs34725_options[0], count_of(tcs34725_options));
        return;
    }
    
    
    #define TCS34725_ADDRESS 0x29<<1u // I2C address of TCS34725
    uint16_t red, green, blue;
    uint8_t data[4];

    printf("TCS3472x Ambient Light Sensor Demo\r\n");
    printf("Press SPACE to exit\r\n");

    //get gain and integration time
    command_var_t arg;
    uint8_t gain; // default gain 16x
    uint32_t user_gain=16;
    cmdln_args_find_flag_uint32('g', &arg, &user_gain);
    switch(user_gain) {
        case 1: gain = 0b00; break; // 1x
        case 4: gain = 0b01; break; // 4x
        case 16: gain = 0b10; break; // 16x
        case 60: gain = 0b11; break; // 60x
        default:
            printf("Invalid gain value: %d\r\n\r\n", user_gain);
            ui_help_show(true, tcs34725_usage, count_of(tcs34725_usage), &tcs34725_options[0], count_of(tcs34725_options));
            return;
    }

    uint32_t integration_cycles = 256; // default integration time 700ms
    if(cmdln_args_find_flag_uint32('i', &arg, &integration_cycles)){
        if(integration_cycles < 1 || integration_cycles > 256){
            printf("Invalid integration cycles: %d\r\n\r\n", integration_cycles);
            //show help
            ui_help_show(true, tcs34725_usage, count_of(tcs34725_usage), &tcs34725_options[0], count_of(tcs34725_options));
            return;
        }
    }

    printf("Gain: %d, Integration Cycles: %d\r\n", user_gain, integration_cycles);

    fala_start_hook();
    // Select enable register(0x80)
	// Power ON, RGBC enable, wait time disable(0x03)
    if(i2c_write(TCS34725_ADDRESS, (uint8_t[]){0x80, 0x03}, 2u)){
        goto tcs34725_cleanup;
    }

    // Select control register(0x8F)
	// AGAIN = 16x(0b10)
    if(i2c_write(TCS34725_ADDRESS, (uint8_t[]){0x8f,gain}, 2u)){
        goto tcs34725_cleanup;
    }

	// Select Wait Time register(0x83)
	// WTIME : 2.4ms(0xFF)
    if(i2c_write(TCS34725_ADDRESS, (uint8_t[]){0x83, 0xff}, 2u)){
        goto tcs34725_cleanup;
    }
	
    // Select ALS time register(0x81)
	// Atime = 700 ms(0x00)
    if(i2c_write(TCS34725_ADDRESS, (uint8_t[]){0x81, (uint8_t)(256-integration_cycles)}, 2u)){
        goto tcs34725_cleanup;
    }

	// Read 8 bytes of data from register(0x94)
	// cData lsb, cData msb, red lsb, red msb, green lsb, green msb, blue lsb, blue msb
    rgb_irq_enable(false);
    rgb_set_all(0, 0, 0);
    while(true){
        char data[8] = {0};
        if(i2c_transaction(TCS34725_ADDRESS, (uint8_t[]){0x94}, 1u, data, 8u)) {
            goto tcs34725_cleanup;
        }

        // Convert the data
        int cData = (data[1] * 256 + data[0]);
        int red = (data[3] * 256 + data[2]);
        int green = (data[5] * 256 + data[4]);
        int blue = (data[7] * 256 + data[6]);

        // Calculate luminance
        float luminance = (-0.32466) * (red) + (1.57837) * (green) + (-0.73191) * (blue);
        if(luminance < 0){
            luminance = 0;
        }
        // upper byte of each
        //rgb_put(data[3] << 16 | data[5] << 8 | data[7]); // RGB format
        rgb_set_all(data[3], data[5], data[7]); // RGB format
        printf("\rR: 0x%04X G: 0x%04X B: 0x%04X C: 0x%04X #RGB: #%02X%02X%02X Lum: %.2f", red, green, blue, cData, data[3], data[5], data[7], luminance);
        //press key to exit
        uint32_t i = 1;
        char c;
        while(i){
            if (rx_fifo_try_get(&c) && (c==' ')) {
                goto tcs34725_cleanup;
            }
            i--; 
            busy_wait_ms(1);
        }
    }

tcs34725_cleanup:
    rgb_irq_enable(true);
    pio_i2c_stop_timeout(0xffff); //force both lines back high
    fala_stop_hook();
    //we manually control any FALA capture
    fala_notify_hook();

}

static const char* const tsl2561_usage[] = {
    "tsl2561 [-h(elp)]",
    "- 3.3volt device, pull-up resistors required",
    "Show LUX: tsl2561",
};

static const struct ui_help_options tsl2561_options[] = {
    { 1, "", T_HELP_I2C_TSL2561 },               // flash command help  
    { 0, "-h", T_HELP_HELP }               // help flag   
};

void demo_tsl2561(struct command_result* res) {
    if (ui_help_show(res->help_flag, tsl2561_usage, count_of(tsl2561_usage), &tsl2561_options[0], count_of(tsl2561_options))) {
        return;
    }
    if (!ui_help_sanity_check(true,0x00)) {
        ui_help_show(true, tsl2561_usage, count_of(tsl2561_usage), &tsl2561_options[0], count_of(tsl2561_options));
        return;
    }


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

static const char* const ms5611_usage[] = {
    "ms5611 [-h(elp)]",
    "- 3.3volt device, pull-up resistors required",
    "Show temperature and pressure: ms5611",
};

static const struct ui_help_options ms5611_options[] = {
    { 1, "", T_HELP_I2C_MS5611},               // flash command help  
    { 0, "-h", T_HELP_HELP }               // help flag   
};

void demo_ms5611(struct command_result* res) {
    if (ui_help_show(res->help_flag, ms5611_usage, count_of(ms5611_usage), &ms5611_options[0], count_of(ms5611_options))) {
        return;
    }
    if (!ui_help_sanity_check(true,0x00)) {
        ui_help_show(true, ms5611_usage, count_of(ms5611_usage), &ms5611_options[0], count_of(ms5611_options));
        return;
    }    
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

static const char* const si7021_usage[] = {
    "si7021 [-h(elp)]",
    "- 3.3volt device, pull-up resistors required",
    "Show temperature and humidity: si7021",
};

static const struct ui_help_options si7021_options[] = {
    { 1, "", T_HELP_I2C_SI7021},               // flash command help  
    { 0, "-h", T_HELP_HELP }               // help flag   
};

void demo_si7021(struct command_result* res) {
    if (ui_help_show(res->help_flag, si7021_usage, count_of(si7021_usage), &si7021_options[0], count_of(si7021_options))) {
        return;
    }
    if (!ui_help_sanity_check(true,0x00)) {
        ui_help_show(true, si7021_usage, count_of(si7021_usage), &si7021_options[0], count_of(si7021_options));
        return;
    }    

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
/*
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
*/
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