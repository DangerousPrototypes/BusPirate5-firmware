#ifndef MPU6050_LIGHT_H
#define MPU6050_LIGHT_H

#include <stdint.h>
#include <stdbool.h>
#include <math.h>

/* The register map is provided at
 * https://invensense.tdk.com/wp-content/uploads/2015/02/MPU-6000-Register-Map1.pdf
 *
 * Mapping of the different gyro and accelero configurations:
 *
 * GYRO_CONFIG_[0,1,2,3] range = +- [250, 500,1000,2000] deg/s
 *                       sensi =    [131,65.5,32.8,16.4] bit/(deg/s)
 *
 * ACC_CONFIG_[0,1,2,3] range = +- [    2,   4,   8,  16] times the gravity (9.81 m/s^2)
 *                      sensi =    [16384,8192,4096,2048] bit/gravity
*/

#define MPU6050_ADDR                  0x68
#define MPU6050_SMPLRT_DIV_REGISTER   0x19
#define MPU6050_CONFIG_REGISTER       0x1a
#define MPU6050_GYRO_CONFIG_REGISTER  0x1b
#define MPU6050_ACCEL_CONFIG_REGISTER 0x1c
#define MPU6050_PWR_MGMT_1_REGISTER   0x6b

#define MPU6050_GYRO_OUT_REGISTER     0x43
#define MPU6050_ACCEL_OUT_REGISTER    0x3B

#define RAD_2_DEG             57.29578f // [deg/rad]
#define CALIB_OFFSET_NB_MES   500
#define TEMP_LSB_2_DEGREE     340.0f    // [bit/celsius]
#define TEMP_LSB_OFFSET       12412.0f

#define DEFAULT_GYRO_COEFF    0.98f

typedef struct {
    uint8_t address;
    float gyro_lsb_to_degsec;
    float acc_lsb_to_g;
    float gyroXoffset, gyroYoffset, gyroZoffset;
    float accXoffset, accYoffset, accZoffset;
    float temp, accX, accY, accZ, gyroX, gyroY, gyroZ;
    float angleAccX, angleAccY;
    float angleX, angleY, angleZ;
    uint32_t preInterval;
    float filterGyroCoef; // complementary filter coefficient to balance gyro vs accelero data to get angle
    bool upsideDownMounting;
} mpu6050_t;

// I2C function prototypes - implement these with your I2C library
typedef struct {
    int (*write)(uint8_t addr, const uint8_t* data, size_t len);
    int (*read)(uint8_t addr, uint8_t* data, size_t len);
    void (*delay_ms)(uint32_t ms);
    uint32_t (*get_time_ms)(void);
} mpu6050_i2c_t;

// Function prototypes
void mpu6050_init(mpu6050_t* mpu, const mpu6050_i2c_t* i2c_funcs);
uint8_t mpu6050_begin(mpu6050_t* mpu, int gyro_config_num, int acc_config_num);

uint8_t mpu6050_write_data(mpu6050_t* mpu, uint8_t reg, uint8_t data);
uint8_t mpu6050_read_data(mpu6050_t* mpu, uint8_t reg);

void mpu6050_calc_offsets(mpu6050_t* mpu, bool is_calc_gyro, bool is_calc_acc);
void mpu6050_calc_gyro_offsets(mpu6050_t* mpu);
void mpu6050_calc_acc_offsets(mpu6050_t* mpu);

void mpu6050_set_address(mpu6050_t* mpu, uint8_t addr);
uint8_t mpu6050_get_address(mpu6050_t* mpu);

uint8_t mpu6050_set_gyro_config(mpu6050_t* mpu, int config_num);
uint8_t mpu6050_set_acc_config(mpu6050_t* mpu, int config_num);

void mpu6050_set_gyro_offsets(mpu6050_t* mpu, float x, float y, float z);
void mpu6050_set_acc_offsets(mpu6050_t* mpu, float x, float y, float z);

void mpu6050_set_filter_gyro_coef(mpu6050_t* mpu, float gyro_coeff);
void mpu6050_set_filter_acc_coef(mpu6050_t* mpu, float acc_coeff);

// Getters
float mpu6050_get_gyro_x_offset(mpu6050_t* mpu);
float mpu6050_get_gyro_y_offset(mpu6050_t* mpu);
float mpu6050_get_gyro_z_offset(mpu6050_t* mpu);

float mpu6050_get_acc_x_offset(mpu6050_t* mpu);
float mpu6050_get_acc_y_offset(mpu6050_t* mpu);
float mpu6050_get_acc_z_offset(mpu6050_t* mpu);

float mpu6050_get_filter_gyro_coef(mpu6050_t* mpu);
float mpu6050_get_filter_acc_coef(mpu6050_t* mpu);

float mpu6050_get_temp(mpu6050_t* mpu);

float mpu6050_get_acc_x(mpu6050_t* mpu);
float mpu6050_get_acc_y(mpu6050_t* mpu);
float mpu6050_get_acc_z(mpu6050_t* mpu);

float mpu6050_get_gyro_x(mpu6050_t* mpu);
float mpu6050_get_gyro_y(mpu6050_t* mpu);
float mpu6050_get_gyro_z(mpu6050_t* mpu);

float mpu6050_get_acc_angle_x(mpu6050_t* mpu);
float mpu6050_get_acc_angle_y(mpu6050_t* mpu);

float mpu6050_get_angle_x(mpu6050_t* mpu);
float mpu6050_get_angle_y(mpu6050_t* mpu);
float mpu6050_get_angle_z(mpu6050_t* mpu);

void mpu6050_fetch_data(mpu6050_t* mpu);
void mpu6050_update(mpu6050_t* mpu);

// Global I2C function pointer - set this before using the library
extern const mpu6050_i2c_t* g_mpu6050_i2c;

#endif