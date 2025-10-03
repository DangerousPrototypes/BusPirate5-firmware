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

#include "spd_rw.h"
#include <string.h>

void spd_rw_init(spd_rw_t* spd, uint8_t device_select) {
    memset(spd->memory, 0, SPD_MEMORY_SIZE);
    spd->device_select = device_select;
}

uint16_t spd_rw_crc16(spd_rw_t* spd, size_t start, int count) {
    uint16_t crc = 0;
    size_t ptr = start;

    while (--count >= 0) {
        if (ptr >= SPD_MEMORY_SIZE) {
            break; // Bounds check
        }
        
        crc = crc ^ spd->memory[ptr] << 8;
        ptr++;
        
        for (int i = 0; i < 8; i++) {
            if (crc & 0x8000) {
                crc = crc << 1 ^ 0x1021;
            } else {
                crc = crc << 1;
            }
        }
    }
    
    return (crc & 0xFFFF);
}

bool spd_rw_read(spd_rw_t* spd) {
    for (size_t addr = 0; SPD_MEMORY_SIZE > addr; addr += SPD_READ_PAGE_SIZE) {
        // Sets page address
        if (spd_rw_command(spd, 0b110 | ((addr >> 8) & 0x01)) < 0) {
            return false;
        }

        // Sets word address
        uint8_t device_addr = (SPD_DEVICE_TYPE << 3) | (spd->device_select & 0x07);
        uint8_t send_data = 0x00;
        if (i2c_write_blocking_stub(device_addr, &send_data, 1, true) < 0) {
            return false;
        }

        // Starts sequential read
        size_t size = SPD_MEMORY_SIZE;
        if (size <= addr) {
            break;
        } else if ((size - addr) < SPD_READ_PAGE_SIZE) {
            size = size - addr;
        } else {
            size = SPD_READ_PAGE_SIZE;
        }
        
        uint8_t* read_ptr = &spd->memory[addr];
        if (i2c_read_blocking_stub(device_addr, read_ptr, size, false) < 0) {
            return false;
        }
    }

    return true;
}

bool spd_rw_write(spd_rw_t* spd) {
    // Starts to write data
    for (size_t addr = 0; SPD_MEMORY_SIZE > addr; addr += SPD_WRITE_PAGE_SIZE) {
        // Sets page address
        if (spd_rw_command(spd, 0b110 | ((addr >> 8) & 0x01)) < 0) {
            return false;
        }

        // Starts page write
        uint8_t device_addr = (SPD_DEVICE_TYPE << 3) | (spd->device_select & 0x07);
        uint8_t send_data[1 + SPD_WRITE_PAGE_SIZE] = {0};
        send_data[0] = addr & 0xFF;
        
        for (size_t i = 0; SPD_WRITE_PAGE_SIZE > i; i++) {
            if (addr + i >= SPD_MEMORY_SIZE) {
                break;
            }
            send_data[1 + i] = spd->memory[addr + i];
        }
        
        if (i2c_write_blocking_stub(device_addr, send_data, sizeof(send_data), false) < 0) {
            return false;
        }
        
        // Write wait
        //TODO: Use address polling instead of fixed delay
        sleep_ms_stub(SPD_TWR_MS);
    }

    return true;
}

bool spd_rw_is_protected(spd_rw_t* spd) {
    if (spd_rw_read_status(spd, 0b001) < 0) {
        return true; // is protected
    }
    if (spd_rw_read_status(spd, 0b100) < 0) {
        return true; // is protected
    }
    if (spd_rw_read_status(spd, 0b101) < 0) {
        return true; // is protected
    }
    if (spd_rw_read_status(spd, 0b000) < 0) {
        return true; // is protected
    }
    
    return false; // is not protected
}

bool spd_rw_clear_protect(spd_rw_t* spd) {
    // Clears all write protect
    if (spd_rw_command(spd, 0b011) < 0) {
        return false;
    }
    return true;
}

bool spd_rw_set_protect(spd_rw_t* spd) {
    // Sets all write protects
    // SWP0
    if (spd_rw_command(spd, 0b001) < 0) {
        return false;
    }
    // SWP1
    if (spd_rw_command(spd, 0b100) < 0) {
        return false;
    }
    // SWP2
    if (spd_rw_command(spd, 0b101) < 0) {
        return false;
    }
    // SWP3
    if (spd_rw_command(spd, 0b000) < 0) {
        return false;
    }
    
    return true;
}

static int spd_rw_command(spd_rw_t* spd, uint8_t cmd) {
    uint8_t device_addr = (SPD_REGISTER_DEVICE_TYPE << 3) | (cmd & 0x07);
    uint8_t send_data[2] = {0};

    return i2c_write_blocking_stub(device_addr, send_data, sizeof(send_data), false);
}

static int spd_rw_read_status(spd_rw_t* spd, uint8_t cmd) {
    uint8_t device_addr = (SPD_REGISTER_DEVICE_TYPE << 3) | (cmd & 0x07);
    uint8_t received_data[2] = {0};

    return i2c_read_blocking_stub(device_addr, received_data, sizeof(received_data), false);
}

// I2C function stubs - IMPLEMENT THESE WITH YOUR LOCAL I2C FUNCTIONS
int i2c_write_blocking_stub(uint8_t device_addr, uint8_t* data, size_t len, bool nostop) {
    // TODO: Replace with your I2C write implementation
    // Parameters:
    //   device_addr: 7-bit I2C device address
    //   data: pointer to data buffer to write
    //   len: number of bytes to write
    //   nostop: true = don't send stop condition (for repeated start)
    // Return: negative on error, 0 or positive on success
    return -1; // Stub implementation
}

int i2c_read_blocking_stub(uint8_t device_addr, uint8_t* data, size_t len, bool nostop) {
    // TODO: Replace with your I2C read implementation
    // Parameters:
    //   device_addr: 7-bit I2C device address
    //   data: pointer to buffer to store read data
    //   len: number of bytes to read
    //   nostop: true = don't send stop condition
    // Return: negative on error, 0 or positive on success
    return -1; // Stub implementation
}

void sleep_ms_stub(uint32_t ms) {
    // TODO: Replace with your delay implementation
    // Parameters:
    //   ms: delay time in milliseconds
    (void)ms; // Suppress unused parameter warning
}

#include "spd_rw.h"

void example_usage(void) {
    spd_rw_t spd;
    
    // Initialize SPD with device select 0
    spd_rw_init(&spd, 0x00);
    
    // Read SPD data
    if (spd_rw_read(&spd)) {
        printf("SPD data read successfully\r\n");
        
        // Calculate CRC
        uint16_t crc = spd_rw_crc16(&spd, 0, 126);
        printf("CRC16: 0x%04X\r\n", crc);
        
        // Check protection status
        if (spd_rw_is_protected(&spd)) {
            printf("SPD is write protected\r\n");
            
            // Clear protection
            if (spd_rw_clear_protect(&spd)) {
                printf("Protection cleared\r\n");
            }
        }
        
        // Modify data and write back
        spd.memory[0] = 0xAA;
        if (spd_rw_write(&spd)) {
            printf("SPD data written successfully\r\n");
        }
        
    } else {
        printf("Failed to read SPD data\r\n");
    }
}