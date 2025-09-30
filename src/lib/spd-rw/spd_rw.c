/*
MIT License

Copyright (c) 2022 kitune-san

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.

https://github.com/kitune-san/SPD_RW/tree/main

Adapted from C++ for the Bus Pirate project by Ian Lesnet September 2025

*/

#ifndef SPD_RW_H
#define SPD_RW_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#define SPD_MEMORY_SIZE 512
#define SPD_READ_PAGE_SIZE 256
#define SPD_WRITE_PAGE_SIZE 16
#define SPD_DEVICE_TYPE 0x0A
#define SPD_REGISTER_DEVICE_TYPE 0x06
#define SPD_TWR_MS 6

typedef struct {
    uint8_t memory[SPD_MEMORY_SIZE];
    uint8_t device_select;
} spd_rw_t;

// Function prototypes
void spd_rw_init(spd_rw_t* spd, uint8_t device_select);
uint16_t spd_rw_crc16(spd_rw_t* spd, size_t start, int count);
bool spd_rw_read(spd_rw_t* spd);
bool spd_rw_write(spd_rw_t* spd);
bool spd_rw_is_protected(spd_rw_t* spd);
bool spd_rw_clear_protect(spd_rw_t* spd);
bool spd_rw_set_protect(spd_rw_t* spd);

// Private function prototypes
static int spd_rw_command(spd_rw_t* spd, uint8_t cmd);
static int spd_rw_read_status(spd_rw_t* spd, uint8_t cmd);

// I2C function stubs - implement these with your local I2C functions
int i2c_write_blocking_stub(uint8_t device_addr, uint8_t* data, size_t len, bool nostop);
int i2c_read_blocking_stub(uint8_t device_addr, uint8_t* data, size_t len, bool nostop);
void sleep_ms_stub(uint32_t ms);

#endif // SPD_RW_H