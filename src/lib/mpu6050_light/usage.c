#include "mpu6050_light.h"
#include "your_i2c_driver.h" // Your I2C implementation

// I2C function implementations
static int my_i2c_write(uint8_t addr, const uint8_t* data, size_t len) {
    // Your I2C write implementation
    return 0; // 0 = success
}

static int my_i2c_read(uint8_t addr, uint8_t* data, size_t len) {
    // Your I2C read implementation
    return 0; // 0 = success
}

static void my_delay_ms(uint32_t ms) {
    // Your delay implementation
}

static uint32_t my_get_time_ms(void) {
    // Your time implementation
    return 0;
}

// I2C interface structure
static const mpu6050_i2c_t i2c_interface = {
    .write = my_i2c_write,
    .read = my_i2c_read,
    .delay_ms = my_delay_ms,
    .get_time_ms = my_get_time_ms
};

int main() {
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
            
            printf("Temp: %.2f°C, Acc X: %.2fg, Gyro X: %.2f°/s, Angle X: %.2f°\n",
                   temp, acc_x, gyro_x, angle_x);
            
            my_delay_ms(100);
        }
    } else {
        printf("MPU6050 initialization failed\n");
    }
    
    return 0;
}