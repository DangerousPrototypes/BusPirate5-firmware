#include "mpu6050_light.h"
#include <math.h>

// Global I2C function pointer
const mpu6050_i2c_t* g_mpu6050_i2c = NULL;

/* Wrap an angle in the range [-limit,+limit] (special thanks to Edgar Bonet!) */
static float wrap(float angle, float limit) {
    while (angle > limit) angle -= 2 * limit;
    while (angle < -limit) angle += 2 * limit;
    return angle;
}

/* INIT and BASIC FUNCTIONS */

void mpu6050_init(mpu6050_t* mpu, const mpu6050_i2c_t* i2c_funcs) {
    g_mpu6050_i2c = i2c_funcs;
    mpu->address = MPU6050_ADDR;
    mpu6050_set_filter_gyro_coef(mpu, DEFAULT_GYRO_COEFF);
    mpu6050_set_gyro_offsets(mpu, 0, 0, 0);
    mpu6050_set_acc_offsets(mpu, 0, 0, 0);
    mpu->upsideDownMounting = false;
}

uint8_t mpu6050_begin(mpu6050_t* mpu, int gyro_config_num, int acc_config_num) {
    // changed calling register sequence [https://github.com/rfetick/MPU6050_light/issues/1] -> thanks to augustosc
    uint8_t status = mpu6050_write_data(mpu, MPU6050_PWR_MGMT_1_REGISTER, 0x01); // check only the first connection with status
    mpu6050_write_data(mpu, MPU6050_SMPLRT_DIV_REGISTER, 0x00);
    mpu6050_write_data(mpu, MPU6050_CONFIG_REGISTER, 0x00);
    mpu6050_set_gyro_config(mpu, gyro_config_num);
    mpu6050_set_acc_config(mpu, acc_config_num);

    mpu6050_update(mpu);
    mpu->angleX = mpu6050_get_acc_angle_x(mpu);
    mpu->angleY = mpu6050_get_acc_angle_y(mpu);
    mpu->preInterval = g_mpu6050_i2c->get_time_ms(); // may cause lack of angular accuracy if begin() is much before the first update()
    return status;
}

uint8_t mpu6050_write_data(mpu6050_t* mpu, uint8_t reg, uint8_t data) {
    uint8_t buffer[2] = {reg, data};
    return g_mpu6050_i2c->write(mpu->address, buffer, 2);
}

// This method is not used internally, maybe by user...
uint8_t mpu6050_read_data(mpu6050_t* mpu, uint8_t reg) {
    uint8_t data;
    
    // Write register address
    if (g_mpu6050_i2c->write(mpu->address, &reg, 1) != 0) {
        return 0; // Error
    }
    
    // Read data
    if (g_mpu6050_i2c->read(mpu->address, &data, 1) != 0) {
        return 0; // Error
    }
    
    return data;
}

/* SETTER */

uint8_t mpu6050_set_gyro_config(mpu6050_t* mpu, int config_num) {
    uint8_t status;
    switch (config_num) {
        case 0: // range = +- 250 deg/s
            mpu->gyro_lsb_to_degsec = 131.0f;
            status = mpu6050_write_data(mpu, MPU6050_GYRO_CONFIG_REGISTER, 0x00);
            break;
        case 1: // range = +- 500 deg/s
            mpu->gyro_lsb_to_degsec = 65.5f;
            status = mpu6050_write_data(mpu, MPU6050_GYRO_CONFIG_REGISTER, 0x08);
            break;
        case 2: // range = +- 1000 deg/s
            mpu->gyro_lsb_to_degsec = 32.8f;
            status = mpu6050_write_data(mpu, MPU6050_GYRO_CONFIG_REGISTER, 0x10);
            break;
        case 3: // range = +- 2000 deg/s
            mpu->gyro_lsb_to_degsec = 16.4f;
            status = mpu6050_write_data(mpu, MPU6050_GYRO_CONFIG_REGISTER, 0x18);
            break;
        default: // error
            status = 1;
            break;
    }
    return status;
}

uint8_t mpu6050_set_acc_config(mpu6050_t* mpu, int config_num) {
    uint8_t status;
    switch (config_num) {
        case 0: // range = +- 2 g
            mpu->acc_lsb_to_g = 16384.0f;
            status = mpu6050_write_data(mpu, MPU6050_ACCEL_CONFIG_REGISTER, 0x00);
            break;
        case 1: // range = +- 4 g
            mpu->acc_lsb_to_g = 8192.0f;
            status = mpu6050_write_data(mpu, MPU6050_ACCEL_CONFIG_REGISTER, 0x08);
            break;
        case 2: // range = +- 8 g
            mpu->acc_lsb_to_g = 4096.0f;
            status = mpu6050_write_data(mpu, MPU6050_ACCEL_CONFIG_REGISTER, 0x10);
            break;
        case 3: // range = +- 16 g
            mpu->acc_lsb_to_g = 2048.0f;
            status = mpu6050_write_data(mpu, MPU6050_ACCEL_CONFIG_REGISTER, 0x18);
            break;
        default: // error
            status = 1;
            break;
    }
    return status;
}

void mpu6050_set_gyro_offsets(mpu6050_t* mpu, float x, float y, float z) {
    mpu->gyroXoffset = x;
    mpu->gyroYoffset = y;
    mpu->gyroZoffset = z;
}

void mpu6050_set_acc_offsets(mpu6050_t* mpu, float x, float y, float z) {
    mpu->accXoffset = x;
    mpu->accYoffset = y;
    mpu->accZoffset = z;
}

void mpu6050_set_filter_gyro_coef(mpu6050_t* mpu, float gyro_coeff) {
    if ((gyro_coeff < 0) || (gyro_coeff > 1)) { 
        gyro_coeff = DEFAULT_GYRO_COEFF; // prevent bad gyro coeff, should throw an error...
    }
    mpu->filterGyroCoef = gyro_coeff;
}

void mpu6050_set_filter_acc_coef(mpu6050_t* mpu, float acc_coeff) {
    mpu6050_set_filter_gyro_coef(mpu, 1.0f - acc_coeff);
}

/* CALC OFFSET */

void mpu6050_calc_offsets(mpu6050_t* mpu, bool is_calc_gyro, bool is_calc_acc) {
    if (is_calc_gyro) { 
        mpu6050_set_gyro_offsets(mpu, 0, 0, 0); 
    }
    if (is_calc_acc) { 
        mpu6050_set_acc_offsets(mpu, 0, 0, 0); 
    }
    
    float ag[6] = {0, 0, 0, 0, 0, 0}; // 3*acc, 3*gyro

    for (int i = 0; i < CALIB_OFFSET_NB_MES; i++) {
        mpu6050_fetch_data(mpu);
        ag[0] += mpu->accX;
        ag[1] += mpu->accY;
        ag[2] += (mpu->accZ - 1.0f);
        ag[3] += mpu->gyroX;
        ag[4] += mpu->gyroY;
        ag[5] += mpu->gyroZ;
        g_mpu6050_i2c->delay_ms(1); // wait a little bit between 2 measurements
    }

    if (is_calc_acc) {
        mpu->accXoffset = ag[0] / CALIB_OFFSET_NB_MES;
        mpu->accYoffset = ag[1] / CALIB_OFFSET_NB_MES;
        mpu->accZoffset = ag[2] / CALIB_OFFSET_NB_MES;
    }

    if (is_calc_gyro) {
        mpu->gyroXoffset = ag[3] / CALIB_OFFSET_NB_MES;
        mpu->gyroYoffset = ag[4] / CALIB_OFFSET_NB_MES;
        mpu->gyroZoffset = ag[5] / CALIB_OFFSET_NB_MES;
    }
}

void mpu6050_calc_gyro_offsets(mpu6050_t* mpu) {
    mpu6050_calc_offsets(mpu, true, false);
}

void mpu6050_calc_acc_offsets(mpu6050_t* mpu) {
    mpu6050_calc_offsets(mpu, false, true);
}

/* ADDRESS */

void mpu6050_set_address(mpu6050_t* mpu, uint8_t addr) {
    mpu->address = addr;
}

uint8_t mpu6050_get_address(mpu6050_t* mpu) {
    return mpu->address;
}

/* GETTERS */

float mpu6050_get_gyro_x_offset(mpu6050_t* mpu) { return mpu->gyroXoffset; }
float mpu6050_get_gyro_y_offset(mpu6050_t* mpu) { return mpu->gyroYoffset; }
float mpu6050_get_gyro_z_offset(mpu6050_t* mpu) { return mpu->gyroZoffset; }

float mpu6050_get_acc_x_offset(mpu6050_t* mpu) { return mpu->accXoffset; }
float mpu6050_get_acc_y_offset(mpu6050_t* mpu) { return mpu->accYoffset; }
float mpu6050_get_acc_z_offset(mpu6050_t* mpu) { return mpu->accZoffset; }

float mpu6050_get_filter_gyro_coef(mpu6050_t* mpu) { return mpu->filterGyroCoef; }
float mpu6050_get_filter_acc_coef(mpu6050_t* mpu) { return 1.0f - mpu->filterGyroCoef; }

float mpu6050_get_temp(mpu6050_t* mpu) { return mpu->temp; }

float mpu6050_get_acc_x(mpu6050_t* mpu) { return mpu->accX; }
float mpu6050_get_acc_y(mpu6050_t* mpu) { return mpu->accY; }
float mpu6050_get_acc_z(mpu6050_t* mpu) { return mpu->accZ; }

float mpu6050_get_gyro_x(mpu6050_t* mpu) { return mpu->gyroX; }
float mpu6050_get_gyro_y(mpu6050_t* mpu) { return mpu->gyroY; }
float mpu6050_get_gyro_z(mpu6050_t* mpu) { return mpu->gyroZ; }

float mpu6050_get_acc_angle_x(mpu6050_t* mpu) { return mpu->angleAccX; }
float mpu6050_get_acc_angle_y(mpu6050_t* mpu) { return mpu->angleAccY; }

float mpu6050_get_angle_x(mpu6050_t* mpu) { return mpu->angleX; }
float mpu6050_get_angle_y(mpu6050_t* mpu) { return mpu->angleY; }
float mpu6050_get_angle_z(mpu6050_t* mpu) { return mpu->angleZ; }

/* UPDATE */

void mpu6050_fetch_data(mpu6050_t* mpu) {
    uint8_t reg = MPU6050_ACCEL_OUT_REGISTER;
    uint8_t raw_data[14];
    
    // Write register address
    if (g_mpu6050_i2c->write(mpu->address, &reg, 1) != 0) {
        return; // Error
    }
    
    // Read 14 bytes of data
    if (g_mpu6050_i2c->read(mpu->address, raw_data, 14) != 0) {
        return; // Error
    }

    int16_t rawData[7]; // [ax,ay,az,temp,gx,gy,gz]

    for (int i = 0; i < 7; i++) {
        rawData[i] = (raw_data[i * 2] << 8) | raw_data[i * 2 + 1];
    }

    mpu->accX = ((float)rawData[0]) / mpu->acc_lsb_to_g - mpu->accXoffset;
    mpu->accY = ((float)rawData[1]) / mpu->acc_lsb_to_g - mpu->accYoffset;
    mpu->accZ = (!mpu->upsideDownMounting - mpu->upsideDownMounting) * ((float)rawData[2]) / mpu->acc_lsb_to_g - mpu->accZoffset;
    mpu->temp = (rawData[3] + TEMP_LSB_OFFSET) / TEMP_LSB_2_DEGREE;
    mpu->gyroX = ((float)rawData[4]) / mpu->gyro_lsb_to_degsec - mpu->gyroXoffset;
    mpu->gyroY = ((float)rawData[5]) / mpu->gyro_lsb_to_degsec - mpu->gyroYoffset;
    mpu->gyroZ = ((float)rawData[6]) / mpu->gyro_lsb_to_degsec - mpu->gyroZoffset;
}

void mpu6050_update(mpu6050_t* mpu) {
    // retrieve raw data
    mpu6050_fetch_data(mpu);

    // estimate tilt angles: this is an approximation for small angles!
    float sgZ = mpu->accZ < 0 ? -1.0f : 1.0f; // allow one angle to go from -180 to +180 degrees
    mpu->angleAccX = atan2f(mpu->accY, sgZ * sqrtf(mpu->accZ * mpu->accZ + mpu->accX * mpu->accX)) * RAD_2_DEG; // [-180,+180] deg
    mpu->angleAccY = -atan2f(mpu->accX, sqrtf(mpu->accZ * mpu->accZ + mpu->accY * mpu->accY)) * RAD_2_DEG; // [- 90,+ 90] deg

    uint32_t Tnew = g_mpu6050_i2c->get_time_ms();
    float dt = (Tnew - mpu->preInterval) * 1e-3f;
    mpu->preInterval = Tnew;

    // Correctly wrap X and Y angles (special thanks to Edgar Bonet!)
    // https://github.com/gabriel-milan/TinyMPU6050/issues/6
    mpu->angleX = wrap(mpu->filterGyroCoef * (mpu->angleAccX + wrap(mpu->angleX + mpu->gyroX * dt - mpu->angleAccX, 180.0f)) + (1.0f - mpu->filterGyroCoef) * mpu->angleAccX, 180.0f);
    mpu->angleY = wrap(mpu->filterGyroCoef * (mpu->angleAccY + wrap(mpu->angleY + sgZ * mpu->gyroY * dt - mpu->angleAccY, 90.0f)) + (1.0f - mpu->filterGyroCoef) * mpu->angleAccY, 90.0f);
    mpu->angleZ += mpu->gyroZ * dt; // not wrapped
}