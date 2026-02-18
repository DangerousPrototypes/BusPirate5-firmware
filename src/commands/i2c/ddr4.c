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

Some functions adapted from C++ for the Bus Pirate project by Ian Lesnet September 2025

*/


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
#include "lib/bp_args/bp_cmd.h"
#include "lib/ms5611/ms5611.h"
#include "lib/tsl2561/driver_tsl2561.h"
#include "binmode/fala.h"
#include "fatfs/ff.h"       // File system related
#include "pirate/file.h" // File handling related
//#include "pirate/storage.h" // File system related
#include "lib/jep106/jep106.h"
#include "ui/ui_hex.h"

#define DDR4_SPD_SIGNITURE 0x12
#define DDR4_SPD_I2C_ADDR_7BIT 0x50
#define DDR4_SPD_SIZE 512 // Size of DDR4 SPD data in bytes
#define DDR4_SPD_SPA0 0b01101100
#define DDR4_SPD_SPA1 0b01101110
#define DDR4_SPD_RPS0 0b01100011
#define DDR4_SPD_RPS1 0b01101001
#define DDR4_SPD_RPS2 0b01101011
#define DDR4_SPD_RPS3 0b01100001
#define DDR4_SPD_SWP0 0b01100010
#define DDR4_SPD_SWP1 0b01101000
#define DDR4_SPD_SWP2 0b01101010
#define DDR4_SPD_SWP3 0b01100000
#define DDR4_SPD_CWP 0b01100110

/**
 * DDR4 SPD (Serial Presence Detect) Structure
 * Decodes the first 16 bytes of DDR4 SPD information according to JEDEC Standard
 */
typedef struct __attribute__((packed)) {
    // Byte 0: Number of Serial PD Bytes Written / SPD Device Size
    union {
        uint8_t spd_bytes_used;
        struct {
            uint8_t spd_bytes_used_val:4;    // bits 0-3: SPD bytes used (encoded)
            uint8_t spd_bytes_total:3;       // bits 4-6: Total SPD bytes (encoded) 
            uint8_t crc_coverage:1;          // bit 7: CRC coverage
        };
    } byte_0;
    
    // Byte 1: SPD Revision
    union {
        uint8_t spd_revision;
        struct {
            uint8_t encoding_level:4;        // bits 0-3: Encoding level
            uint8_t additions_level:4;       // bits 4-7: Additions level
        };
    } byte_1;
    
    // Byte 2: Key Byte / DRAM Device Type
    uint8_t dram_device_type;               // 0x0C = DDR4 SDRAM
    
    // Byte 3: Key Byte / Module Type
    union {
        uint8_t module_type;
        struct {
            uint8_t module_type_val:4;       // bits 0-3: Module type
            uint8_t reserved_3_4_7:4;        // bits 4-7: Reserved
        };
    } byte_3;
    
    // Byte 4: SDRAM Density and Banks
    union {
        uint8_t sdram_density_banks;
        struct {
            uint8_t bank_address_bits:2;     // bits 0-1: Bank address bits
            uint8_t bank_group_bits:2;       // bits 2-3: Bank group bits  
            uint8_t sdram_capacity:4;        // bits 4-7: SDRAM capacity (encoded)
        };
    } byte_4;
    
    // Byte 5: SDRAM Addressing
    union {
        uint8_t sdram_addressing;
        struct {
            uint8_t column_address_bits:3;   // bits 0-2: Column address bits
            uint8_t row_address_bits:3;      // bits 3-5: Row address bits
            uint8_t reserved_5_6_7:2;        // bits 6-7: Reserved
        };
    } byte_5;
    
    // Byte 6: Primary SDRAM Package Type
    union {
        uint8_t primary_package_type;
        struct {
            uint8_t signal_loading:2;        // bits 0-1: Signal loading
            uint8_t reserved_6_2:1;          // bit 2: Reserved
            uint8_t die_count:3;             // bits 3-5: Die count
            uint8_t sdram_device_type:2;     // bits 6-7: SDRAM device type
        };
    } byte_6;
    
    // Byte 7: SDRAM Optional Features
    union {
        uint8_t sdram_optional_features;
        struct {
            uint8_t maximum_activate_count:4; // bits 0-3: Maximum activate count (MAC)
            uint8_t maximum_activate_window:2; // bits 4-5: Maximum activate window (MRAW)  
            uint8_t reserved_7_6_7:2;        // bits 6-7: Reserved
        };
    } byte_7;
    
    // Byte 8: SDRAM Thermal and Refresh Options
    union {
        uint8_t thermal_refresh_options;
        struct {
            uint8_t extended_temp_range:1;   // bit 0: Extended temperature range
            uint8_t extended_temp_refresh:1; // bit 1: Extended temperature refresh rate
            uint8_t auto_self_refresh:1;     // bit 2: Auto self refresh
            uint8_t on_die_thermal_sensor:1; // bit 3: On-die thermal sensor readout
            uint8_t reserved_8_4_6:3;        // bits 4-6: Reserved
            uint8_t partial_array_self_refresh:1; // bit 7: Partial array self refresh
        };
    } byte_8;
    
    // Byte 9: Other SDRAM Optional Features  
    union {
        uint8_t other_sdram_optional_features;
        struct {
            uint8_t post_package_repair:2;   // bits 0-1: Post package repair
            uint8_t read_dbi:1;              // bit 2: Read DBI
            uint8_t write_dbi:1;             // bit 3: Write DBI
            uint8_t reserved_9_4_7:4;        // bits 4-7: Reserved
        };
    } byte_9;
    
    // Byte 10: Secondary SDRAM Package Type
    union {
        uint8_t secondary_package_type;
        struct {
            uint8_t signal_loading:2;        // bits 0-1: Signal loading  
            uint8_t reserved_10_2:1;         // bit 2: Reserved
            uint8_t die_count:3;             // bits 3-5: Die count
            uint8_t sdram_device_type:2;     // bits 6-7: SDRAM device type
        };
    } byte_10;
    
    // Byte 11: Module Nominal Voltage, VDD
    union {
        uint8_t module_nominal_voltage;
        struct {
            uint8_t vdd_1_2v:1;              // bit 0: 1.2V operable
            uint8_t vdd_1_35v:1;             // bit 1: 1.35V operable  
            uint8_t vdd_1_5v:1;              // bit 2: 1.5V operable
            uint8_t reserved_11_3_7:5;       // bits 3-7: Reserved
        };
    } byte_11;
    
    // Byte 12: Module Organization
    union {
        uint8_t module_organization;
        struct {
            uint8_t device_width:3;          // bits 0-2: SDRAM device width
            uint8_t rank_count:3;            // bits 3-5: Number of package ranks per DIMM
            uint8_t rank_mix:1;              // bit 6: Rank mix
            uint8_t reserved_12_7:1;         // bit 7: Reserved
        };
    } byte_12;
    
    // Byte 13: Module Memory Bus Width
    union {
        uint8_t module_bus_width;
        struct {
            uint8_t primary_bus_width:3;     // bits 0-2: Primary bus width
            uint8_t bus_width_extension:2;   // bits 3-4: Bus width extension (ECC)
            uint8_t reserved_13_5_7:3;       // bits 5-7: Reserved
        };
    } byte_13;
    
    // Byte 14: Module Thermal Sensor
    union {
        uint8_t module_thermal_sensor;
        struct {
            uint8_t thermal_sensor:7;        // bits 0-6: Thermal sensor incorporated
            uint8_t reserved_14_7:1;         // bit 7: Reserved
        };
    } byte_14;
    
    // Byte 15: Extended Module Type
    union {
        uint8_t extended_module_type;
        struct {
            uint8_t extended_base_module_type:4; // bits 0-3: Extended base module type
            uint8_t reserved_15_4_7:4;       // bits 4-7: Reserved
        };    } byte_15;
    
    // Byte 16: Reserved (must be 0x00)
    uint8_t reserved_16;
    
} ddr4_spd_general_section_t;


/**
 * DDR4 SPD Manufacturing Information Structure
 * Decodes bytes 320-383 (0x140-0x17F) of DDR4 SPD information according to JEDEC Standard
 */
typedef struct __attribute__((packed)) {
    // Bytes 320-321: Module Manufacturer's ID Code
    union {
        uint16_t module_manufacturer_id;     // Complete 16-bit ID
        struct {
            uint8_t module_mfg_id_lsb;       // Byte 320: LSB
            uint8_t module_mfg_id_msb;       // Byte 321: MSB
        };
    };
    
    // Byte 322: Module Manufacturing Location
    uint8_t module_manufacturing_location;
    
    // Bytes 323-324: Module Manufacturing Date
    union {
        uint16_t module_manufacturing_date;  // Complete 16-bit date
        struct {
            uint8_t manufacturing_year;      // Byte 323: Year (BCD format, offset from 2000)
            uint8_t manufacturing_week;      // Byte 324: Week (BCD format, 1-53)
        };
    };
    
    // Bytes 325-328: Module Serial Number
    union {
        uint32_t module_serial_number;       // Complete 32-bit serial number
        struct {
            uint8_t serial_byte0;            // Byte 325: Serial number byte 0 (LSB)
            uint8_t serial_byte1;            // Byte 326: Serial number byte 1
            uint8_t serial_byte2;            // Byte 327: Serial number byte 2
            uint8_t serial_byte3;            // Byte 328: Serial number byte 3 (MSB)
        };
    };
    
    // Bytes 329-348: Module Part Number (20 ASCII characters)
    char module_part_number[20];
    
    // Byte 349: Module Revision Code
    union {
        uint8_t module_revision_code;
        struct {
            uint8_t revision_minor:4;        // bits 0-3: Minor revision
            uint8_t revision_major:4;        // bits 4-7: Major revision
        };
    };
    
    // Bytes 350-351: DRAM Manufacturer's ID Code
    union {
        uint16_t dram_manufacturer_id;       // Complete 16-bit ID
        struct {
            uint8_t dram_mfg_id_lsb;         // Byte 350: LSB
            uint8_t dram_mfg_id_msb;         // Byte 351: MSB
        };
    };
    
    // Byte 352: DRAM Stepping
    uint8_t dram_stepping;
    
    // Bytes 353-381: Module Manufacturer's Specific Data (29 bytes)
    uint8_t manufacturer_specific_data[29];
    
    // Bytes 382-383: Reserved (must be 0x00)
    uint8_t reserved[2];
    
} ddr4_spd_manufacturing_info_t;


// Helper macros for decoding encoded values
#define DDR4_SPD_BYTES_USED(x)    ((x) == 0 ? 0 : (128 << (x)))
#define DDR4_SPD_BYTES_TOTAL(x)   ((x) == 0 ? 256 : (256 << (x)))
#define DDR4_SDRAM_CAPACITY(x)    ((x) == 0 ? 256 : (256 << (x))) // Megabits
#define DDR4_DEVICE_WIDTH(x)      (4 << (x)) // bits
#define DDR4_BUS_WIDTH(x)         (8 << (x)) // bits

// Module type definitions
#define DDR4_MODULE_TYPE_EXTENDED    0x00
#define DDR4_MODULE_TYPE_RDIMM       0x01
#define DDR4_MODULE_TYPE_UDIMM       0x02
#define DDR4_MODULE_TYPE_SODIMM      0x03
#define DDR4_MODULE_TYPE_LRDIMM      0x04
#define DDR4_MODULE_TYPE_MINI_RDIMM  0x05
#define DDR4_MODULE_TYPE_MINI_UDIMM  0x06
#define DDR4_MODULE_TYPE_72B_SODIMM  0x08
#define DDR4_MODULE_TYPE_72B_UDIMM   0x09
#define DDR4_MODULE_TYPE_16B_SODIMM  0x0C
#define DDR4_MODULE_TYPE_32B_SODIMM  0x0D

// Function prototypes
void ddr4_spd_print_general_section(const ddr4_spd_general_section_t* spd);
const char* ddr4_spd_get_module_type_string(uint8_t module_type);

bool ddr4_poll_idle(void){
    uint32_t timeout = 0xffffu; 
    // wait for the write to complete
    hwi2c_status_t i2c_result;
    do {
        if(pio_i2c_start_timeout(0xffff)) return true;
        i2c_result = pio_i2c_write_timeout((DDR4_SPD_I2C_ADDR_7BIT<<1), 0xffffff);
        if(pio_i2c_stop_timeout(0xffff)) return true;
        if(i2c_result == HWI2C_OK) return false; //idle
        timeout --;
    } while(timeout); // wait until the write operation is complete
    return true; //failed
}

void ddr4_print_unknown(uint8_t value){
    // Print unknown value in hex format
    printf("Unknown (0x%02X)\r\n", value);
}

bool ddr4_write_command(uint8_t command, bool *ack){
    //write command to CWP
    hwi2c_status_t i2c_result;
    if(pio_i2c_start_timeout(0xffff)) return true;
    i2c_result = pio_i2c_write_timeout(command, 0xffff);
    pio_i2c_write_timeout(0x00, 0xffff);
    pio_i2c_write_timeout(0x00, 0xffff);
    if(pio_i2c_stop_timeout(0xffff)) return true;
    if(i2c_result >=HWI2C_TIMEOUT) return true; //error
    *ack = (i2c_result == HWI2C_OK) ? true : false;
    return false;
}

bool ddr4_set_page(bool page){
    const uint8_t page_addr[2]={DDR4_SPD_SPA0, DDR4_SPD_SPA1};
    bool ack;
    if(ddr4_write_command(page_addr[page], &ack)) return true; //write the page to the device
    if(!ack) { //ACK = set page failed
        printf("Error: Set page %d command failed\r\n", page);
        return true; // failed to set page
    }
    return false;
}

bool ddr4_get_lock_status(uint8_t block, bool *locked){
    const uint8_t block_addr[4]={DDR4_SPD_RPS0, DDR4_SPD_RPS1, DDR4_SPD_RPS2, DDR4_SPD_RPS3};
    //tricky: write first byte, NACK = locked, ACK = unlocked
    //then write two dummy bytes and ignore the result
    bool ack;
    if(ddr4_write_command(block_addr[block], &ack)) return true; // I2C error
    *locked = !ack; //invert ack to locked status
    return false;
}

bool ddr4_lock_block(uint8_t block_number) {
    const uint8_t block_addr[4]={DDR4_SPD_SWP0, DDR4_SPD_SWP1, DDR4_SPD_SWP2, DDR4_SPD_SWP3};
    //check lock status, fails if already locked
    //TODO: set and verify HV programming voltage
    bool lock_status;
    if(ddr4_get_lock_status(block_number, &lock_status)) {
        return true; // I2C error
    }
    if(lock_status) {
        printf("Block %d is already locked\r\n", block_number);
        return false; // already locked
    }
    if(ddr4_write_command(block_addr[block_number], &lock_status)) {
        return true; // I2C error
    }
    if(!lock_status) { //NACK = locked, ACK = failed
        printf("Error: Failed to lock block %d\r\n", block_number);
        return true; // failed to lock
    }
    if(ddr4_get_lock_status(block_number, &lock_status)) {
        return true; // I2C error
    }
    #if 0
    if(ddr4_poll_idle(0x6D)){
        printf("Error: Timeout waiting for lock operation to complete\r\n");
        return true; // timeout
    }
    #endif
    busy_wait_ms(100); //wait 100ms for the lock to take effect, datasheet suggests 4ms max
    if(!lock_status) {
        printf("Error: Verification failed, block %d is not locked\r\n", block_number);
        return true; // verification failed
    }
    printf("Block %d locked successfully\r\n", block_number);
    return false;
}

bool ddr4_unlock_blocks(void){
    bool ack;
    //printf("Unlocking all blocks (0-3)...\r\n");
    if(ddr4_write_command(DDR4_SPD_CWP, &ack)) return true; // I2C error
    if(!ack) { //ACK = unlock failed
        printf("Error: Unlock blocks command failed\r\n");
        return true; // failed to unlock
    }
    #if 0
    if(ddr4_poll_idle(0x6D)){
        printf("Error: Timeout waiting for unlock operation to complete\r\n");
        return true; // timeout
    }
    #endif
    busy_wait_ms(100); //wait 100ms for the unlock to take effect, datasheet suggests 4ms max
    //verify all blocks unlocked
    for(uint8_t block=0; block<4; block++){
        bool lock_status;
        if(ddr4_get_lock_status(block, &lock_status)) return true; // I2C error
        if(lock_status) {
            printf("Error: Verification failed, block %d is still locked\r\n", block);
            return true; // verification failed
        }
    }
    //printf("All blocks unlocked successfully\r\n");
    return false;
}

bool ddr4_read_pages(uint8_t* data) {
    //set page 0
    for(uint8_t i=0; i<2; i++){
        if(ddr4_set_page(i)) return true;
        //read 256 bytes from page i
        if(i2c_write(DDR4_SPD_I2C_ADDR_7BIT<<1, (uint8_t[]){0x00}, 1u)) return true;
        if(i2c_read((DDR4_SPD_I2C_ADDR_7BIT<<1)|1, &data[i*256], 256)) return true;
    }
    return false;
}

bool ddr4_show_lock_status(void){
    bool lock_status;
    printf("Block | Status\r\n");
    printf("--------------\r\n");
    for(uint8_t block=0; block<4; block++){
        if(ddr4_get_lock_status(block, &lock_status)) return true; // I2C error
        printf("  %d   | %s\r\n", block, lock_status ? "Locked" : "Unlocked");
    }
    return false;
}

bool ddr4_dump(uint8_t *buffer) {
    //detect if spd present
    //if(ddr5_detect_spd_quick()) return true; //check if the device is DDR5 SPD

    // align the start address to 16 bytes, and calculate the end address
    struct hex_config_t hex_config;
    hex_config.max_size_bytes= DDR4_SPD_SIZE; // maximum size of the device in bytes
    ui_hex_get_args_config(&hex_config);
    ui_hex_align_config(&hex_config);
    ui_hex_header_config(&hex_config);
    //read SPD
    if(ddr4_read_pages(buffer)) return true; //read EEPROM page 0-1
    for(uint32_t i=hex_config._aligned_start; i<(hex_config._aligned_end+1); i+=16) {
        if(ui_hex_row_config(&hex_config, i, &buffer[i], 16)){
            return true; // exit pager
        }
    }
    return false; // no error
}


bool ddr4_read_to_file(FIL *file_handle, uint8_t *buffer) {
    //read SPD
    if(ddr4_read_pages(buffer)) {
        file_close(file_handle); // close the file if there was an error
        return true; // if the read was unsuccessful
    }
    if(file_write(file_handle, buffer, DDR4_SPD_SIZE)) { 
        return true; // if the write was unsuccessful (file closed in lower layer)
    }
    // close the file
    f_close(file_handle); // close the file
    return false;
}

bool ddr4_verify(FIL *file_handle, uint8_t *buffer) {
 
    // check if the file size is 512 bytes        
    if(file_size_check(file_handle, DDR4_SPD_SIZE)) return true; 

    //read SPD
    if(ddr4_read_pages(buffer)) {
        file_close(file_handle); // close the file if there was an error
        return true; // if the read was unsuccessful
    }

    uint8_t verify_buffer[128];
    bool verror = false; // flag to indicate if there was a verification error
    for(uint32_t i=0; i<DDR4_SPD_SIZE/128; i++){
        if(file_read(file_handle, verify_buffer, 128, NULL)) return true; 
        //compare read data with buffer
        for(uint8_t j=0; j<128; j++){
            if(verify_buffer[j] != buffer[(i*128)+j]) {
                printf("Error: SPD NVM byte %d does not match file! (0x%02X != 0x%02X)\r\n", j+(i*128), verify_buffer[j], buffer[(i*128)+j]);
                verror = true; // set the verification error flag
            }
        }
    }

    // close the file
    file_close(file_handle); // close the file
    return verror; // return true if there was a verification error, false otherwise
}

uint16_t spd_rw_crc16(char* spd, size_t start, int count) {
    uint16_t crc = 0;
    size_t ptr = start;

    while (--count >= 0) {
        if (ptr >= DDR4_SPD_SIZE) {
            break; // Bounds check
        }
        
        crc = crc ^ spd[ptr] << 8;
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

bool ddr4_jedec_crc(uint8_t *data){
    printf("CRC verify\r\nStored CRC (bytes 126:127): 0x%02X 0x%02X\r\n", data[126], data[127]);
    uint16_t crc= spd_rw_crc16(data, 0, 126);
    printf("Calculated CRC: 0x%02X 0x%02X\r\n", crc&0xff, crc >> 8);
    if(crc != (data[127] << 8 | data[126])){
        printf("Error: CRC does not match!!!\r\n");
        return true;
    } else {
        printf("CRC okay :)\r\n");
        return false;
    }
}

bool ddr4_crc_file(FIL *file_handle, char *buffer, bool close_file){
    //get file size
    if(file_size_check(file_handle, DDR4_SPD_SIZE)) return true; // check if the file size is 1024 bytes
    if(file_read(file_handle, buffer, 512, NULL)) return true; // read the first 512 bytes from the file
    if(close_file){
        if(file_close(file_handle)) return true; // close the file
    }
    return ddr4_jedec_crc(buffer); 
}


//true for error, false for success
bool ddr4_write_from_file(FIL *file_handle, uint8_t *buffer) {
    uint32_t bytes_read;
    
    if(ddr4_crc_file(file_handle, buffer, false)) return true;
    
    //read current lock bits
    bool lock_status;
    for(uint8_t block=0; block<4; block++){
        if(ddr4_get_lock_status(block, &lock_status)) return true; // I2C error
        if(lock_status) {
            printf("Error: Block %d is locked, cannot write to DDR4 SPD NVM. Unlock all blocks before writing.\r\n", block);
            goto ddr4_write_error; // if any block is locked, we cannot write to the NVM
        }
    }

    //write 16 byte * 8 pages * 2 blocks to the DDR4 SPD
    printf("Writing page:");
    for(uint8_t i=0; i<2; i++){
        if(ddr4_set_page(i)) goto ddr4_write_error; // set the page to write to
        
        for(uint8_t j=0; j<16; j++){
            printf(" %d,", (i*16) + j);
            //get the bytes into an array so we don't have to use granular function
            uint8_t page_data[17];
            page_data[0] = (j*16); // address to write to
            for(uint8_t k=0; k<16; k++) {
                page_data[k+1] = buffer[(i*256)+(j*16)+k]; // data to write
            }

            if(i2c_write(DDR4_SPD_I2C_ADDR_7BIT<<1, page_data, 17u)){
                printf("\r\nError writing page %d, chunk %d\r\n", i, j);   
                goto ddr4_write_error; // write the access register and address to write to
            }
 
            // wait for the write to complete
            if(ddr4_poll_idle()) return true; // wait until the write operation is complete
        }
    }

    printf(" Done!\r\n");

    printf("Verify write\r\n");
    f_rewind(file_handle); // rewind the file to the beginning
    if(ddr4_verify(file_handle, buffer)){
        printf("Verify: Failed!\r\n");
        return true; // on fail file is closed in ddr5_verify
    }else{
        printf("Verify: OK\r\n"); //on success file is closed in ddr5_verify
    }

    return false;

ddr4_write_error:
    printf("Error writing to DDR4 SPD\r\n");
    file_close(file_handle); // close the file if there was an error
    system_config.error = true; // set the error flag
    return true;
}



const char* ddr4_spd_get_module_type_string(uint8_t module_type) {
    switch (module_type) {
        case DDR4_MODULE_TYPE_EXTENDED:    return "Extended";
        case DDR4_MODULE_TYPE_RDIMM:       return "RDIMM";
        case DDR4_MODULE_TYPE_UDIMM:       return "UDIMM"; 
        case DDR4_MODULE_TYPE_SODIMM:      return "SO-DIMM";
        case DDR4_MODULE_TYPE_LRDIMM:      return "LRDIMM";
        case DDR4_MODULE_TYPE_MINI_RDIMM:  return "Mini-RDIMM";
        case DDR4_MODULE_TYPE_MINI_UDIMM:  return "Mini-UDIMM";
        case DDR4_MODULE_TYPE_72B_SODIMM:  return "72b-SO-RDIMM";
        case DDR4_MODULE_TYPE_72B_UDIMM:   return "72b-SO-UDIMM";
        case DDR4_MODULE_TYPE_16B_SODIMM:  return "16b-SO-DIMM";
        case DDR4_MODULE_TYPE_32B_SODIMM:  return "32b-SO-DIMM";
        default:                           return "Reserved/Unknown";
    }
}

void ddr4_spd_print_general_section(const ddr4_spd_general_section_t* spd) {
    printf("DDR4 SPD General Section Information:\r\n");
    printf("=====================================\r\n");
    
    // Byte 0: SPD Bytes Used/Total
    printf("SPD Bytes Used: ");
    if (spd->byte_0.spd_bytes_used_val == 0) {
        printf("Undefined\r\n");
    } else {
        printf("%d\r\n", spd->byte_0.spd_bytes_used_val * 128);
    }
    
    printf("SPD Bytes Total: ");
    if (spd->byte_0.spd_bytes_total == 0) {
        printf("Undefined\r\n");
    } else {
        printf("%d\r\n", spd->byte_0.spd_bytes_total*256);
    }
    
    //printf("CRC Coverage: %s\r\n", spd->byte_0.crc_coverage ? "Bytes 0-125" : "Bytes 0-127");
    
    // Byte 1: SPD Revision
    printf("SPD Revision: %d.%d\r\n", spd->byte_1.additions_level, spd->byte_1.encoding_level);
    
    // Byte 2: DRAM Device Type
    printf("DRAM Device Type: ");
    if (spd->dram_device_type == 0x0C) {
        printf("DDR4 SDRAM\r\n");
    } else {
        printf("Unknown (0x%02X)\r\n", spd->dram_device_type);
    }
    
    // Byte 3: Module Type
    printf("Module Type: %s (0x%01X)\r\n", 
           ddr4_spd_get_module_type_string(spd->byte_3.module_type_val),
           spd->byte_3.module_type_val);
    
    // Byte 11: Module Nominal Voltage
    printf("Nominal Voltage Support:\r\n");
    printf("  1.2V: %s\r\n", spd->byte_11.vdd_1_2v ? "Yes" : "No");
    printf("  1.35V: %s\r\n", spd->byte_11.vdd_1_35v ? "Yes" : "No"); 
    printf("  1.5V: %s\r\n", spd->byte_11.vdd_1_5v ? "Yes" : "No");
           
    // Byte 14: Module Thermal Sensor  
    printf("Thermal Sensor: %s\r\n", spd->byte_14.thermal_sensor ? "Present" : "Not present");
    
    printf("\r\n");
}

// Helper function to decode BCD (Binary Coded Decimal) values
static uint8_t ddr4_bcd_to_decimal(uint8_t bcd) {
    return ((bcd >> 4) * 10) + (bcd & 0x0F);
}

static const char *ddr5_decode_jep106_jedec_id(uint8_t b0, uint8_t b1){
    int bank=(b0 & 0xf);
    int id=(b1 & 0x7f); //SPD JEDEC ID is lower 7 bits, no parity bit. Bit 7 = 1 = bank > 0
    return jep106_table_manufacturer(bank, id); //returns a string
}

// Function to print manufacturing information
void ddr4_spd_print_manufacturing_info(const ddr4_spd_manufacturing_info_t* mfg) {
    printf("DDR4 SPD Manufacturing Information:\r\n");
    printf("==================================\r\n");
    
    // Module Manufacturer ID
    printf("Module Manufacturer ID: 0x%04X", mfg->module_manufacturer_id);
    const char* module_mfg = ddr5_decode_jep106_jedec_id(mfg->module_mfg_id_lsb, mfg->module_mfg_id_msb);
    if (module_mfg) {
        printf(" (%s)", module_mfg);
    }
    printf("\r\n");
    
    // Manufacturing Location
    printf("Manufacturing Location: 0x%02X\r\n", mfg->module_manufacturing_location);
    
    // Manufacturing Date
    uint8_t year = ddr4_bcd_to_decimal(mfg->manufacturing_year);
    uint8_t week = ddr4_bcd_to_decimal(mfg->manufacturing_week);
    printf("Manufacturing Date: Year %d, Week %d (20%02d-W%02d)\r\n", 
           year, week, year, week);
    
    // Serial Number
    printf("Module Serial Number: 0x%08X (%u)\r\n", 
           mfg->module_serial_number, mfg->module_serial_number);
    
    // Part Number (ensure null termination for display)
    char part_number[21] = {0}; // 20 chars + null terminator
    memcpy(part_number, mfg->module_part_number, 20);
    // Trim trailing spaces
    for (int i = 19; i >= 0 && (part_number[i] == ' ' || part_number[i] == 0); i--) {
        part_number[i] = 0;
    }
    printf("Module Part Number: \"%s\"\r\n", part_number);
    
    // Revision Code
    printf("Module Revision: %d.%d\r\n", 
           mfg->revision_major, mfg->revision_minor);
    
    // DRAM Manufacturer ID
    printf("DRAM Manufacturer ID: 0x%04X", mfg->dram_manufacturer_id);
    const char* dram_mfg = ddr5_decode_jep106_jedec_id(mfg->dram_mfg_id_lsb, mfg->dram_mfg_id_msb);
    if (dram_mfg) {
        printf(" (%s)", dram_mfg);
    }
    printf("\r\n");
    
    // DRAM Stepping
    printf("DRAM Stepping: 0x%02X\r\n", mfg->dram_stepping);
    
    // Manufacturer Specific Data (show first few bytes)
    #if 0
    printf("Manufacturer Specific Data: ");
    for (int i = 0; i < 8 && i < 29; i++) {
        printf("0x%02X ", mfg->manufacturer_specific_data[i]);
    }
    if (29 > 8) {
        printf("... (%d more bytes)", 29 - 8);
    }
    printf("\r\n");
    #endif
    
    // Check reserved bytes
    if (mfg->reserved[0] != 0x00 || mfg->reserved[1] != 0x00) {
        printf("Warning: Reserved bytes are not 0x00 (0x%02X 0x%02X)\r\n", 
               mfg->reserved[0], mfg->reserved[1]);
    }
    
    printf("\r\n");
}

// Function to extract manufacturing info from SPD buffer
bool ddr4_decode_manufacturing_info(const uint8_t* spd_buffer) {
    // Manufacturing info starts at byte 320 (0x140)
    const ddr4_spd_manufacturing_info_t* mfg_info = 
        (const ddr4_spd_manufacturing_info_t*)(spd_buffer + 320);
    
    // Validate we're not reading beyond buffer bounds
    if (320 + sizeof(ddr4_spd_manufacturing_info_t) > DDR4_SPD_SIZE) {
        printf("Error: Manufacturing info extends beyond SPD buffer\r\n");
        return true;
    }
    
    ddr4_spd_print_manufacturing_info(mfg_info);
    return false;
}

bool ddr4_probe(uint8_t *buffer) {
    if(ddr4_read_pages(buffer)) return true; //read EEPROM page 0-1

    ddr4_spd_general_section_t* spd_data = (ddr4_spd_general_section_t*)buffer;

    // Validate it's DDR4
    if (spd_data->dram_device_type != 0x0C) {
        printf("Warning: Device type 0x%02X is not DDR4 (expected 0x0C)\r\n", spd_data->dram_device_type);
    }

    // Show lock status
    printf("DDR4 SPD Status:\r\n");
    printf("=====================\r\n");
    ddr4_jedec_crc(buffer); //check CRC
    printf("\r\n");
    if(ddr4_show_lock_status())return true;
    printf("\r\n");

    
    // Print decoded SPD information
    ddr4_spd_print_general_section(spd_data);

    // Print manufacturing information
    ddr4_decode_manufacturing_info(buffer);    
#if 0
    // read 128 bytes from the DDR5 SPD Volatile Memory
    // cast it to the ddr5_spd_volatile_t structure
    //detect if spd present
    if(ddr5_detect_spd_quick()) return true; //check if the device is DDR5 SPD

    if(ddr5_read_pages_128bytes(false, 0, 1, buffer)) return true; //read volatile memory, start at 0x00
    
    ddr5_decode_volatile_memory(buffer); //decode the volatile memory

    //read 1024 bytes from the DDR5 SPD NVM
    if(ddr5_read_pages_128bytes(true, 0, 8, buffer)) return true; //read EEPROM page 0-7, start at 0x00

    printf("\r\nSPD EEPROM JEDEC Data blocks 0-7:\r\n");
    //if(ddr5_read_pages_128bytes(true, 0, 4, buffer)) return true; //read EEPROM page 0-4, start at 0x00
    if(ddr5_nvm_jedec_crc(buffer)) return true; //check CRC of the first 512 bytes
    if(ddr5_nvm_jedec_decode_data(buffer)) return true; //decode the first 512 bytes of the EEPROM

    printf("\r\nSPD EEPROM JEDEC Manufacturing Information blocks 8-9:\r\n");
    //if(ddr5_read_pages_128bytes(true, 0b100, 4, buffer)) return true; //read EEPROM page 5-8, start at 0x00
    if(ddr5_nvm_jedec_decode_manuf(&buffer[0x200])) return true; //decode the manufacturing information
    if(ddr5_nvm_search(buffer)) return true; //search for the end user programmable area
#endif
    return false;
}

static const char* const usage[] = {
    "ddr4 [probe|dump|write|read|verify|lock|unlock|crc]\r\n\t[-f <file>] [-b <block number>|<bytes>] [-s <start address>] [-h(elp)]",
    "Probe DDR4 SPD:%s ddr4 probe",
    "Show DDR4 SPD contents:%s ddr4 dump",
    "Show 32 bytes starting at address 0x50:%s ddr4 dump -s 0x50 -b 32",
    "Write SPD from file, verify:%s ddr4 write -f example.bin",
    "Read SPD to file, verify:%s ddr4 read -f example.bin",
    "Verify against file:%s ddr4 verify -f example.bin",
    "Show block lock status:%s ddr4 lock",
    "Lock a block 0-3:%s ddr4 lock -b 0",
    "Unlock all blocks 0-3:%s ddr4 unlock",
    "Check/generate CRC for JEDEC bytes 0-125:%s ddr4 crc -f example.bin",
    "Patch/update CRC in file:%s ddr4 patch -f example.bin",
    "DDR4 write file **MUST** be exactly 512 bytes long"
};

enum ddr4_actions_enum {
    DDR4_PROBE=0,
    DDR4_DUMP,
    DDR4_READ,
    DDR4_WRITE,
    DDR4_VERIFY,
    DDR4_LOCK,
    DDR4_UNLOCK,
    DDR4_CRC,
    DDR4_PATCH
};

static const bp_command_action_t ddr4_action_defs[] = {
    { DDR4_PROBE,  "probe",  T_HELP_DDR4_PROBE },
    { DDR4_DUMP,   "dump",   T_HELP_DDR4_DUMP },
    { DDR4_READ,   "read",   T_HELP_DDR4_READ },
    { DDR4_WRITE,  "write",  T_HELP_DDR4_WRITE },
    { DDR4_VERIFY, "verify", T_HELP_DDR4_VERIFY },
    { DDR4_LOCK,   "lock",   T_HELP_DDR4_LOCK },
    { DDR4_UNLOCK, "unlock", T_HELP_DDR4_UNLOCK },
    { DDR4_CRC,    "crc",    T_HELP_DDR4_CRC },
    { DDR4_PATCH,  "patch",  T_HELP_DDR4_PATCH },
};

static const bp_command_opt_t ddr4_opts[] = {
    { "file",  'f', BP_ARG_REQUIRED, "file",  T_HELP_DDR4_FILE_FLAG },
    { "block", 'b', BP_ARG_REQUIRED, "block", T_HELP_DDR4_BLOCK_FLAG },
    { "start", 's', BP_ARG_REQUIRED, "addr",  UI_HEX_HELP_START },
    { "bytes", 'b', BP_ARG_REQUIRED, "count", UI_HEX_HELP_BYTES },
    { "quiet", 'q', BP_ARG_NONE,     NULL,      UI_HEX_HELP_QUIET },
    { 0 }
};

const bp_command_def_t ddr4_def = {
    .name         = "ddr4",
    .description  = T_HELP_DDR4,
    .actions      = ddr4_action_defs,
    .action_count = count_of(ddr4_action_defs),
    .opts         = ddr4_opts,
    .usage        = usage,
    .usage_count  = count_of(usage),
};

void ddr4_handler(struct command_result* res) {
    if (bp_cmd_help_check(&ddr4_def, res->help_flag)) {
        return;
    }
    if (!ui_help_sanity_check(true,0x00)) {
        return;
    }

    uint32_t action;
    if(!bp_cmd_get_action(&ddr4_def, &action)){
        bp_cmd_help_show(&ddr4_def);
        return;        
    }

    char file[13];
    FIL file_handle;                                                  // file handle
    if ((action == DDR4_WRITE || action == DDR4_READ || action== DDR4_VERIFY || action == DDR4_CRC || action == DDR4_PATCH)) {
        
        if(file_get_args(file, sizeof(file))){; // get the file name from the command line arguments
            return;
        }

        uint8_t file_status;
        if(action==DDR4_READ){
            file_status = FA_CREATE_ALWAYS | FA_WRITE;
        }else{
            file_status = FA_READ; // open the file for reading
        }
        if(file_open(&file_handle, file, file_status)) return; // create the file, overwrite if it exists
    }
    
    uint32_t block_flag;
    bool lock_update=false;
    if(action == DDR4_LOCK || action == DDR4_UNLOCK) {
        if(!bp_cmd_get_uint32(&ddr4_def, 'b', &block_flag)){ // block to lock/unlock
            lock_update = false; //we will not update the lock bits, just read them
        }else if(block_flag > 3) {
            printf("Block number must be between 0 and 3\r\n");
            return;
        }else{
            lock_update = true; //we will update the lock bits
        }
    }

    fala_start_hook();  
    uint8_t buffer[DDR4_SPD_SIZE]; // buffer to store the file data
    switch(action) {
        case DDR4_PROBE:
            ddr4_probe(buffer);
            break;
        case DDR4_DUMP:
            ddr4_dump(buffer);
            break;
        case DDR4_READ:
            printf("Read SPD to file: %s\r\n", file);
            if(ddr4_read_to_file(&file_handle, buffer)) {
                printf("Error!");
            }else{
                printf("Success :)");
            }
            break;
        case DDR4_WRITE:
            printf("Write SPD from file: %s\r\n", file);
            if(ddr4_write_from_file(&file_handle, buffer)) {
                printf("Error!\r\n");
            } else {
                printf("Success :)");
            }
            break;
        case DDR4_VERIFY:
            printf("Verifying SPD against file: %s\r\n", file);
            if(ddr4_verify(&file_handle, buffer)) {
                printf("Error!\r\n");
            } else {
                printf("Success :)\r\n");
            }
            break;
        case DDR4_LOCK:
            // show status before and after lock?
            if(lock_update){
                printf("Locking block %d\r\n", block_flag);
                ddr4_lock_block(block_flag);
            }//else just show lock status?
            ddr4_show_lock_status();
            break;
        case DDR4_UNLOCK:
            printf("Unlocking all blocks (0-3)...\r\n");
            if(!ddr4_unlock_blocks()){
                printf("All blocks unlocked successfully\r\n");
            }
            ddr4_show_lock_status();
            break;
        case DDR4_CRC:
            printf("Checking CRC for bytes 0-125, file: %s\r\n", file);
            ddr4_crc_file(&file_handle, buffer, true);
            break;
        case DDR4_PATCH:
            printf("Checking CRC for bytes 0-125, file: %s\r\n", file);
            //get file size
            if(file_size_check(&file_handle, DDR4_SPD_SIZE)) break; // check if the file size is 1024 bytes
            if(file_read(&file_handle, buffer, DDR4_SPD_SIZE, NULL)) break; // read the all bytes from the file
            if(file_close(&file_handle)) break; // close the file
            printf("Stored CRC (bytes 126:127): 0x%02X 0x%02X\r\n", buffer[126], buffer[127]);
            // check the CRC of the first 126 bytes
            uint16_t crc= spd_rw_crc16(buffer, 0, 126);
            printf("Calculated CRC: 0x%02X 0x%02X\r\n", crc&0xff, crc >> 8);
            if(crc == (buffer[127] << 8 | buffer[126])){
                printf("CRC is already correct, no need to patch\r\n");
                break;
            }
            // open the file for writing
            if(file_open(&file_handle, file, FA_WRITE | FA_READ)) break; // open the file for writing
            // patch the CRC
            buffer[126] = crc & 0xff;
            buffer[127] = crc >> 8;
            printf("Patching CRC in file to: 0x%02X 0x%02X\r\n", buffer[126], buffer[127]);
            // write the buffer to the file
            if(file_write(&file_handle, buffer, DDR4_SPD_SIZE)) break; // if the write was unsuccessful (file closed in lower layer)
            if(file_close(&file_handle)) break; // close the file
            printf("File patched successfully :)\r\n");
            //verify the patch
            printf("Verifying patched file CRC:\r\n");
            if(file_open(&file_handle, file, FA_READ)) break; // open the file for reading
            ddr4_crc_file(&file_handle, buffer, true);
            break;            
        default:
            printf("Unknown action\r\n");
            break;
    }
    //we manually control any FALA capture
    fala_stop_hook();
    fala_notify_hook();
}    