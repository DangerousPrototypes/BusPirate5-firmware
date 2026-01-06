#include <stdio.h>
#include "pico/stdlib.h"
#include <stdint.h>
#include <string.h>
#include "pirate.h"
#include "ui/ui_term.h"
#include "command_struct.h"
#include "ui/ui_help.h"
#include "ui/ui_cmdln.h"
#include "binmode/fala.h"
#include "pirate/hwi2c_pio.h"
#include "lib/mpu6050_light/mpu6050_light.h"

// I2C function implementations
static int my_i2c_write(uint8_t addr, const uint8_t* data, size_t len) {
    // Your I2C write implementation
    return i2c_write(addr<<1, (uint8_t *)data, (uint16_t)len);
    //return 0; // 0 = success
}

static int my_i2c_read(uint8_t addr, uint8_t* data, size_t len) {
    // Your I2C read implementation
    return i2c_read((addr<<1)|1, data, (uint16_t)len);
    //return 0; // 0 = success
}

static void my_delay_ms(uint32_t ms) {
    // Your delay implementation
    busy_wait_ms(ms);
}

static uint32_t my_get_time_ms(void) {
    // Your time implementation
    return to_ms_since_boot(get_absolute_time());
}

// I2C interface structure
static const mpu6050_i2c_t i2c_interface = {
    .write = my_i2c_write,
    .read = my_i2c_read,
    .delay_ms = my_delay_ms,
    .get_time_ms = my_get_time_ms
};

static const char* const usage[] = {
    "mpu6050 read:%s mpu6050",
};

static const struct ui_help_options options[] = {
    /*{ 1, "", T_HELP_I2C_MPU6050 },*/
    { 0, "-h", T_HELP_FLAG },   // help
};  


void mpu6050_handler(struct command_result* res) {
    if(res->help_flag) {
        //eeprom_display_devices(eeprom_devices, count_of(eeprom_devices)); // display the available EEPROM devices
        ui_help_show(true, usage, count_of(usage), &options[0], count_of(options)); // show help if requested
        return; // if help was shown, exit
    }
    mpu6050_t mpu;

    // Initialize MPU6050
    mpu6050_init(&mpu, &i2c_interface);

    // Begin with gyro config 1 (±500°/s) and acc config 0 (±2g)
    if (mpu6050_begin(&mpu, 1, 0) == 0) {
        printf("MPU6050 initialized successfully\n");
        
        // Calibrate offsets
        mpu6050_calc_offsets(&mpu, true, true);
        
        while (1) {
            // Update sensor data
            mpu6050_update(&mpu);
            
            // Read values
            float temp = mpu6050_get_temp(&mpu);
            float acc_x = mpu6050_get_acc_x(&mpu);
            float gyro_x = mpu6050_get_gyro_x(&mpu);
            float angle_x = mpu6050_get_angle_x(&mpu);
            
            printf("\rTemp: %.2f°C, Acc X: %.2fg, Gyro X: %.2f°/s, Angle X: %.2f°  ",
                temp, acc_x, gyro_x, angle_x);
            
            my_delay_ms(100);
        }
    } else {
        printf("MPU6050 initialization failed\n");
    }    

}
