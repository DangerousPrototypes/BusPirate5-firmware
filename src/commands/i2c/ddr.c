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
#include "fatfs/ff.h"       // File system related
#include "pirate/file.h" // File handling related
//#include "pirate/storage.h" // File system related
#include "lib/jep106/jep106.h"
#include "ui/ui_hex.h"

// DDR5 SPD Volatile Memory structure
typedef struct __attribute__((packed)) {
    // Register MR0 and MR1: Device Type
    uint8_t device_type_msb; // MR0: Device Type MSB (Default: 0x51)
    uint8_t device_type_lsb; // MR1: Device Type LSB (Default: 0x18)

    // Register MR2: Device Revision
    uint8_t device_revision_0_rv:1; // MR2: Reserved (Default: 0x00)
    uint8_t device_revision_minor:3;
    uint8_t device_revision_major:2;
    uint8_t device_revision_76_rv:2; // MR2: Device Revision (Default: 0x20)

    // Register MR3 and MR4: Vendor ID
    uint8_t vendor_id_byte0; // MR3: Vendor ID Byte 0 (Default: 0x80)
    uint8_t vendor_id_byte1; // MR4: Vendor ID Byte 1 (Default: 0xCD)

    // Register MR5: Device Capability
    //uint8_t device_capability; // MR5: Device Capability (Default: 0x03)
    bool device_capability_hub_support:1; 
    bool device_capability_ts_suport:1; 
    uint8_t device_capability_rv:6;

    // Register MR6: Device Write Recovery Time Capability
    //uint8_t write_recovery_time_capability; // MR6: Write Recovery Time Capability (Default: 0x52)
    uint8_t write_recovery_time_capability_time:2; 
    uint8_t write_recovery_time_capability_rv:2; // Reserved bit
    uint8_t write_recovery_time_capability_unit:4; 

    // Register MR7 to MR10: Reserved
    uint8_t reserved_7_10[4]; // MR7 to MR10: Reserved (Default: 0x00)

    // Register MR11: I2C Legacy Mode Device Configuration
    //uint8_t i2c_legacy_mode_config; // MR11: I2C Legacy Mode Device Configuration (Default: 0x00)
    uint8_t i2c_legacy_mode_config_addr_pointer:3; // Address Pointer Mode (Default: 0x00)
    uint8_t i2c_legacy_mode_config_addr_mode:1; // I2C Legacy Mode (Default: 0x00)
    uint8_t i2c_legacy_mode_config_rv:4; // Reserved bits


    // Register MR12 and MR13: Write Protection for NVM Blocks
    uint8_t write_protection_nvm_blocks_low;  // MR12: Write Protection for NVM Blocks [7:0] (Default: 0x00)
    uint8_t write_protection_nvm_blocks_high; // MR13: Write Protection for NVM Blocks [15:8] (Default: 0x00)

    // Register MR14: Device Configuration - Host & Local Interface IO
    uint8_t device_configuration_io; // MR14: Device Configuration - Host & Local Interface IO (Default: 0x00)

    // Register MR15 to MR17: Reserved
    uint8_t reserved_15_17[3]; // MR15 to MR17: Reserved (Default: 0x00)

    // Register MR18: Device Configuration
    uint8_t device_configuration; // MR18: Device Configuration (Default: 0x00)

    // Register MR19 and MR20: Clear Registers
    uint8_t clear_register_temperature_status; // MR19: Clear Register MR51 Temperature Status Command (Default: 0x00)
    uint8_t clear_register_error_status;       // MR20: Clear Register MR52 Error Status Command (Default: 0x00)

    // Register MR21 to MR25: Reserved
    uint8_t reserved_21_25[5]; // MR21 to MR25: Reserved (Default: 0x00)

    // Register MR26: TS Configuration
    uint8_t ts_configuration; // MR26: TS Configuration (Default: 0x00)

    // Register MR27: Interrupt Configurations
    uint8_t interrupt_configurations; // MR27: Interrupt Configurations (Default: 0x00)

    // Register MR28 to MR35: TS Temperature Limits
    uint8_t ts_temp_high_limit_low;       // MR28 & MR29: TS Temperature High Limit Configuration (Default: 0x7003)
    uint8_t ts_temp_high_limit_high;      // MR30 & MR31: TS Temperature High Limit Configuration (Default: 0x0000)
    
    uint8_t ts_temp_low_limit_low;        // MR30 & MR31: TS Temperature Low Limit Configuration (Default: 0x0000)
    uint8_t ts_temp_low_limit_high;       // MR32 & MR33: TS Temperature Low Limit Configuration (Default: 0x0000)
    
    uint8_t ts_critical_temp_high_limit_low; // MR32 & MR33: TS Critical Temperature High Limit Configuration (Default: 0x5005)
    uint8_t ts_critical_temp_high_limit_high; // MR34 & MR35: TS Critical Temperature High Limit Configuration (Default: 0x0000)
    
    uint8_t ts_critical_temp_low_limit_low;  // MR34 & MR35: TS Critical Temperature Low Limit Configuration (Default: 0x0000)
    uint8_t ts_critical_temp_low_limit_high; // MR36 & MR37: TS Critical Temperature Low Limit Configuration (Default: 0x0000)

    // Register MR36: TS Resolution
    uint8_t ts_resolution; // MR36: TS Resolution register (Default: 0x01)

    // Register MR37: TS Hysteresis Width
    uint8_t ts_hysteresis_width; // MR37: TS Hysteresis width register (Default: 0x01)

    uint8_t reserved_38_47[10]; // MR38 to MR47: Reserved (Default: 0x00)

    // Register MR48: Device Status
    uint8_t device_status; // MR48: Device Status (Default: 0x00)

    // Register MR49 and MR50: TS Current Sensed Temperature
    uint8_t ts_current_sensed_temperature_low; // MR49 & MR50: TS Current Sensed Temperature (Default: 0x0000)
    uint8_t ts_current_sensed_temperature_high; // MR49 & MR50: TS Current Sensed Temperature (Default: 0x0000)

    // Register MR51: TS Temperature Status
    uint8_t ts_temperature_status; // MR51: TS Temperature Status (Default: 0x00)

    // Register MR52: Hub, Thermal, and NVM Error Status
    uint8_t error_status; // MR52: Hub, Thermal, and NVM Error Status (Default: 0x00)

    // Register MR53: Program Abort Register
    uint8_t program_abort_register; // MR53: Program abort register (Default: 0x00)

    // Reserved Registers
    uint8_t reserved_54_127[74]; // MR54 to MR127: Reserved (Default: 0x00)
} ddr5_spd_volatile_t;

// DDR5 NVM JEDEC SDRAM Information structure
typedef struct __attribute__((packed)) {
    uint8_t density_per_die:5; // 0-31
    uint8_t dies_per_package:3;
    uint8_t row_address_bits:5; // 0-7
    uint8_t column_address_bits:3; // 0-7
    uint8_t rev_1:5;
    uint8_t io_width:3; // 0-3
    uint8_t banks_per_group:3; // 0-3
    uint8_t rev_2:2;
    uint8_t bank_groups:3; // 0-7
} ddr5_nvm_jedec_sdram_info_t;

//struct for parsing the NVM 5118 JEDEC data
typedef struct __attribute__((packed)) {
    uint8_t beta_level_b03:4; // Beta Level 0-3 (bits 0-3)
    uint8_t spd_bytes_total:3;
    uint8_t beta_level_b4:1;
    uint8_t spd_revision_minor:4; // SPD Revision Minor (bits 0-3)
    uint8_t spd_revision_major:4; // SPD Revision Major (bits 4-7)
    uint8_t bus_protocol_type; // SDRAM Module Type
    uint8_t module_type:4; // Module Type (bits 0-3)
    uint8_t hybrid_media:3; // Hybrid Media (bits 4-6)
    uint8_t hybrid_module:1; // Hybrid Module (bit 7)
    ddr5_nvm_jedec_sdram_info_t first_sdram; // First SDRAM Info (bytes 4-7)
    ddr5_nvm_jedec_sdram_info_t second_sdram; // Second SDRAM Info (bytes 8-11)
} ddr5_nvm_jedec_info_t;

//struct for parsing the NVM manufacturing information
typedef struct __attribute__((packed)) {
    uint8_t manuf_code[2]; // Manufacturer Code (JEP106)
    uint8_t manuf_location; // Manufacturer Location
    uint8_t manuf_date[2]; // Manufacturing Date (Y/W)
    uint8_t serial_number[4]; // Serial Number
    char part_number[30]; // Part Number
    uint8_t revision_code; // Revision Code
    uint8_t dram_manuf_code[2]; // DRAM Manufacturer Code (JEP106)
    uint8_t dram_stepping; // DRAM Stepping
    uint8_t manufacturer_specific[128-42]; // Manufacturer Specific Data (remaining bytes)
} ddr5_nvm_jedec_manuf_info_t;

#define DDR5_SPD_SIGNITURE 0x12
#define DDR5_SPD_I2C_WRITE_ADDR 0xA0
#define DDR5_SPD_MR11 0x0B // MR11: I2C Legacy Mode Device Configuration
#define DDR5_SPD_MR12 0x0C // MR12: Write Protection for NVM Blocks [7:0]
#define DDR5_SPD_MR13 0x0D // MR13: Write Protection for NVM Blocks [15:8]
#define DDR5_SPD_MR48 0x30 // MR48: Device Status
#define DDR5_SPD_MR48_OVERRIDE_STATUP 1u << 2 // MR48: Write Protect Override Status Update
#define DDR5_SPD_MR48_OP_STATUS 1u << 3 // MR48: Operation Status (1= write in progress)

#define DDR5_SPD_ACCESS_REG 0x00 // Access register space
#define DDR5_SPD_ACCESS_NVM 0x80 // Access NVM space

#define DDR5_SPD_SIZE 1024 // Size of DDR5 SPD data in bytes

bool ddr5_set_legacy_page(uint8_t page){
    //set the page for the legacy mode
    uint8_t data[2]={DDR5_SPD_MR11, 0x00 | (page & 0b111)}; //MR11: I2C Legacy Mode Device Configuration
    //data[0] = DDR5_SPD_MR11; //MR11: I2C Legacy Mode Device Configuration
    //data[1] = page & 0b111; //page 0-7
    if(i2c_write(DDR5_SPD_I2C_WRITE_ADDR, data, 2u)) return true; //write the page to the device
    //read to verify
    if(i2c_transaction(DDR5_SPD_I2C_WRITE_ADDR, (uint8_t[]){DDR5_SPD_MR11}, 1u, data, 1)) return true;
    
    if(data[0] != page) {
        printf("Error: Page %d not set, read back %d\r\n", page, data[0]);
        return true;
    }

    return false;
}

bool ddr5_detect_spd_quick(void){
    uint8_t data[2];
    //data[0] = DDR5_SPD_ACCESS_REG; //read volatile memory, start at 0x00
    if(pio_i2c_transaction_array_repeat_start(DDR5_SPD_I2C_WRITE_ADDR, (uint8_t[]){DDR5_SPD_ACCESS_REG}, 1u, data, 2, 0xffffu)) {
        printf("Device not detected (no ACK)\r\n");
        return true;
    }
    ddr5_spd_volatile_t* spd = (ddr5_spd_volatile_t*)data;
    printf("Device Type: 0x%02X%02X\r\n", spd->device_type_msb, spd->device_type_lsb);
    if((spd->device_type_msb != 0x51) || (spd->device_type_lsb != 0x18)){
        printf("Error: Device Type does not match expected values (0x51 0x18)\r\n");
        return true;
    }
    if(ddr5_set_legacy_page(0)) {
        printf("Error: I2C Legacy Mode Address Pointer Mode is not 1 byte\r\n");
        return true;
    }
    return false;
}

bool ddr5_read_pages_128bytes(bool eeprom, uint8_t start_page, uint8_t num_pages, uint8_t* data) {
    if(!eeprom){
        data[0]=DDR5_SPD_ACCESS_REG;//read volatile memory, start at 0x00
    }else{
        data[0]=DDR5_SPD_ACCESS_NVM;//read EEPROM page start at 0x00 
        if(ddr5_set_legacy_page(start_page)) return true; //set the page for the legacy mode
    }
    if (pio_i2c_transaction_array_repeat_start(DDR5_SPD_I2C_WRITE_ADDR, data, 1u, data, num_pages*128, 0xffffu)) {
        printf("Device not detected (no ACK)\r\n");
        return true;
    }
    return false;
}

static const char *ddr5_decode_jep106_jedec_id(uint8_t b0, uint8_t b1){
    int bank=(b0 & 0xf);
    int id=(b1 & 0x7f); //SPD JEDEC ID is lower 7 bits, no parity bit. Bit 7 = 1 = bank > 0
    return jep106_table_manufacturer(bank, id); //returns a string
}

uint16_t ddr5_crc16(const uint8_t *spd, uint32_t cnt) {
    if (spd == NULL) {
        printf("Error: SPD pointer is NULL.\n");
        return 0xFFFF; // Return an error code
    }

    uint16_t crc = 0;
    for (uint32_t index = 0; index < cnt; ++index) {
        crc ^= (uint16_t)(spd[index] << 8); // Process each byte
        for (uint8_t bit = 0; bit < 8; ++bit) {
            if (crc & 0x8000) {
                crc = (crc << 1) ^ 0x1021;
            } else {
                crc = crc << 1;
            }
        }
    }
    return crc & 0xFFFF;
}

bool ddr5_wait_idle(void){
    uint32_t timeout = 0xfffu; 
    // wait for the write to complete
    uint8_t status;
    do {
        // read the status register 0x30
        if(i2c_transaction(DDR5_SPD_I2C_WRITE_ADDR, (uint8_t[]){DDR5_SPD_MR48}, 1u, &status, 1)) {
            printf("Error reading SPD Device Status Register\r\n");
            return true;
        }
        timeout --;
    } while((status & DDR5_SPD_MR48_OP_STATUS) || (timeout==0)); // wait until the write operation is complete
    return false;
}

void ddr5_protection_block_table(uint8_t *old_data, uint8_t *verify_data, bool show_old) {
    // lock table header
    printf("Block   |");
    for(uint8_t i=0; i<16; i++){
        printf("%02X|", i);
    }

    if(show_old){
        printf("\r\nPrevious|");
        for(uint8_t j=0; j<2; j++){
            for(uint8_t i=0; i<8; i++){
                printf("%s |", (old_data[j] & (1u << i)) ? "L" : "U");
            }
        }
    }

    printf("\r\nCurrent |");
    for(uint8_t j=0; j<2; j++){
        for(uint8_t i=0; i<8; i++){
            printf("%s |", (verify_data[j] & (1u << i)) ? "L" : "U");
        }
    }
    printf("\r\n");
}

/*
*
* DDR5 SPD register and configuration functions
*
*/
float ddr5_get_temperature(uint8_t t_high, uint8_t t_low, uint8_t t_resolution) {
   // Extract the sign bit from the high byte
   int8_t sign = (t_high & 0x10) ? -1 : 1;

   // Combine the high and low bytes into a 16-bit value
   uint16_t raw_value = ((t_high & 0x0F) << 8) | t_low;

   raw_value = raw_value >> (3-t_resolution); // Adjust for resolution
   // Determine the resolution multiplier
   float multiplier;
   //replace switch with a struct
   struct {
       uint8_t resolution;
       float multiplier;
   } resolution_map[] = {
       {0x00, 0.5f},   // 9-bit resolution (0.5°C)
       {0x01, 0.25f},  // 10-bit resolution (0.25°C)
       {0x02, 0.125f}, // 11-bit resolution (0.125°C)
       {0x03, 0.0625f} // 12-bit resolution (0.0625°C)
   };
   if(t_resolution > 3) {
       multiplier = 0.0f; // Invalid resolution
   }else{
       multiplier = resolution_map[t_resolution].multiplier;
   }

    //printf("Raw Value: 0x%04X, Sign: %d, Multiplier: %f\r\n", raw_value, sign, multiplier);
   // Calculate the temperature
   float temperature = sign * raw_value * multiplier;
   return temperature;
}

bool ddr5_decode_volatile_memory(uint8_t* data) {
    
    ddr5_spd_volatile_t* spd = (ddr5_spd_volatile_t*)data;

    printf("Device Revision: %d.%d\r\n", (spd->device_revision_major+1), spd->device_revision_minor);
    printf("Vendor ID: 0x%02X%02X (%s)\r\n", spd->vendor_id_byte0, spd->vendor_id_byte1, ddr5_decode_jep106_jedec_id(spd->vendor_id_byte0, spd->vendor_id_byte1));
    printf("Device Capability - Temperature Sensor Support: %d\r\n", spd->device_capability_ts_suport);
    printf("Device Capability - Hub Support: %d\r\n", spd->device_capability_hub_support);

    uint16_t write_recovery_time;
    if(spd->write_recovery_time_capability_unit<=10){
        write_recovery_time = spd->write_recovery_time_capability_unit;
    } else if (spd->write_recovery_time_capability_unit==11){
        write_recovery_time = 50;
    } else if (spd->write_recovery_time_capability_unit==12){
        write_recovery_time = 100;
    } else if (spd->write_recovery_time_capability_unit==13){
        write_recovery_time = 200;
    } else if (spd->write_recovery_time_capability_unit==14){
        write_recovery_time = 500;
    } else {
        write_recovery_time = 0xffff;
    }

    printf("Write Recovery Time Capability: %d%s\r\n", write_recovery_time, spd->write_recovery_time_capability_time==0?"ns": spd->write_recovery_time_capability_time==1 ? "us" : spd->write_recovery_time_capability_time==2?"ms": "error");
    printf("I2C Legacy Mode Address Pointer Mode: %d byte address\r\n", (spd->i2c_legacy_mode_config_addr_mode+1));
    printf("I2C Legacy Mode Address Pointer: page %d\r\n", spd->i2c_legacy_mode_config_addr_pointer);
    printf("Write Protection for NVM Blocks: 0x%02X 0x%02X\r\n", spd->write_protection_nvm_blocks_low, spd->write_protection_nvm_blocks_high);
    ddr5_protection_block_table((uint8_t[]){0x00,0x00}, (uint8_t[]){spd->write_protection_nvm_blocks_low, spd->write_protection_nvm_blocks_high}, false); 
    #if 0
    for(uint8_t i=0; i<8; i++){
        printf("  Block %d: %s\r\n", i, (spd->write_protection_nvm_blocks_low & (1u << i)) ? "Protected" : "Unprotected");
    }
    for(uint8_t i=0; i<8; i++){
        printf("  Block %d: %s\r\n", i+8, (spd->write_protection_nvm_blocks_high & (1u << i)) ? "Protected" : "Unprotected");
    }  
    #endif  
    printf("Device Configuration - Host & Local Interface IO: 0x%02X\r\n", spd->device_configuration_io);
    printf("Device Configuration: 0x%02X\r\n", spd->device_configuration);
    printf("Interrupt Configurations: 0x%02X\r\n", spd->interrupt_configurations);

    printf("Device Status: 0x%02X\r\n", spd->device_status);
    printf("Device Status - Write Protect Override: %s\r\n", (spd->device_status & 0x04) ? "Enabled" : "Disabled");

    printf("Temp Sensor: %s\r\n", spd->ts_configuration?"Disabled":"Enabled");
    printf("TS Resolution: %d bits\r\n", spd->ts_resolution+9);
    printf("TS Hysteresis Width: 0x%02X\r\n", spd->ts_hysteresis_width);
    printf("TS Current Sensed Temperature: 0x%02X%02X (%2.1fc)\r\n", 
        spd->ts_current_sensed_temperature_high, 
        spd->ts_current_sensed_temperature_low, 
        ddr5_get_temperature(spd->ts_current_sensed_temperature_high, spd->ts_current_sensed_temperature_low, spd->ts_resolution));
    printf("TS Temperature High Limit: 0x%02X%02X (%2.1fc)\r\n", 
        spd->ts_temp_high_limit_high, 
        spd->ts_temp_high_limit_low, 
        ddr5_get_temperature(spd->ts_temp_high_limit_high, spd->ts_temp_high_limit_low, spd->ts_resolution));
    printf("TS Temperature Low Limit: 0x%02X%02X (%2.1fc)\r\n", 
        spd->ts_temp_low_limit_high, 
        spd->ts_temp_low_limit_low, 
        ddr5_get_temperature(spd->ts_temp_low_limit_high, spd->ts_temp_low_limit_low, spd->ts_resolution));
    printf("TS Critical Temperature High Limit: 0x%02X%02X (%2.1fc)\r\n", spd->ts_critical_temp_high_limit_high, spd->ts_critical_temp_high_limit_low, 
        ddr5_get_temperature(spd->ts_critical_temp_high_limit_high, spd->ts_critical_temp_high_limit_low, spd->ts_resolution)); 
    printf("TS Critical Temperature Low Limit: 0x%02X%02X (%2.1fc)\r\n", 
        spd->ts_critical_temp_low_limit_high, 
        spd->ts_critical_temp_low_limit_low,
        ddr5_get_temperature(spd->ts_critical_temp_low_limit_high, spd->ts_critical_temp_low_limit_low, spd->ts_resolution));
    printf("TS Temperature Status: 0x%02X\r\n", spd->ts_temperature_status);
    
    printf("Error Status: 0x%02X\r\n", spd->error_status);
    printf("Program Abort Register: %s\r\n", spd->program_abort_register?"Error":"Normal");  
    return false;
}

/*
*
* Non Volatile Memory (NVM) JEDEC data EEPROM structure
*
*/
void ddr5_print_unknown(uint8_t value){
    // Print unknown value in hex format
    printf("Unknown (0x%02X)\r\n", value);
}

bool ddr5_nvm_jedec_crc(uint8_t *data){
    printf("CRC verify\r\nStored CRC (bytes 510:511): 0x%02X 0x%02X\r\n", data[510], data[511]);
    uint16_t crc= ddr5_crc16(data, 510);
    printf("Calculated CRC: 0x%02X 0x%02X\r\n", crc&0xff, crc >> 8);
    if(crc != (data[511] << 8 | data[510])){
        printf("Error: CRC does not match!!!\r\n");
        return true;
    } else {
        printf("CRC okay :)\r\n");
        return false;
    }
}

// 0 if ok, 1 if error
bool ddr5_sdram_info(ddr5_nvm_jedec_sdram_info_t *sdram_info) {   
    static const uint8_t spd_sdram_density[]={0,4,8,12,16,24,32,48,64};
    if(sdram_info->density_per_die==0){
        printf("  Not present\r\n");
        return false;
    }
    
    printf("  Density: ");
    if(sdram_info->density_per_die <= 0b1000) {
        printf("%d Gb\r\n", spd_sdram_density[sdram_info->density_per_die]);
    } else {
        ddr5_print_unknown(sdram_info->density_per_die);
    }

    static const uint8_t spd_sram_dies[]={1,2,2,4,8,16};
    printf("  SRAM Die Count: ");
    if(sdram_info->dies_per_package <= 0b101) {
        printf("%d\r\n", spd_sram_dies[sdram_info->dies_per_package]);
    } else {
        ddr5_print_unknown(sdram_info->dies_per_package);
    }

    static const uint8_t spd_sdram_width[]={4,8,16,32};
    printf("  SDRAM Width: ");
    if(sdram_info->io_width <=0b11) {
        printf("x%d\r\n", spd_sdram_width[sdram_info->io_width]);
    } else {
        ddr5_print_unknown(sdram_info->io_width);
    }

    static const uint8_t spd_sdram_groups[]={1,2,4,8};
    printf("  SDRAM Bank Groups: ");
    if(sdram_info->bank_groups <= 0b11) {
        printf("%d\r\n", spd_sdram_groups[sdram_info->bank_groups]);
    } else {
        ddr5_print_unknown(sdram_info->bank_groups);
    }

    static const uint8_t spd_sdram_banks[]={1,2,4};
    printf("  SDRAM Banks per Bank Group: ");
    if(sdram_info->banks_per_group <= 0b010) {
        printf("%d\r\n", spd_sdram_banks[sdram_info->banks_per_group]);
    } else {
        ddr5_print_unknown(sdram_info->banks_per_group);
    }

    return false;
}

bool ddr5_nvm_jedec_decode_data(uint8_t *data) {
    //print the first few SPD bytes as hex first, then decode
    printf("\r\nSPD bytes 0-3: 0x%02X, 0x%02X, 0x%02X, 0x%02X\r\n", data[0], data[1], data[2], data[3]);
    //printf("\r\nSPD NVM byte 2: 0x%02X\r\n", data[2]);
    ddr5_nvm_jedec_info_t* spd_info = (ddr5_nvm_jedec_info_t*)data;
    //check the signature
    if(spd_info->bus_protocol_type == DDR5_SPD_SIGNITURE){
        printf("  Host Bus Type: DDR5 SDRAM (0x%02X)\r\n", spd_info->bus_protocol_type);
    }else{
        printf("  Host Bus Type: Unknown (0x%02X), aborting...\r\n", spd_info->bus_protocol_type);
        return true;
    }

    //printf("SPD NVM byte 1: 0x%02X\r\n", data[1]);
    printf("  SPD Revision: %d.%d\r\n", spd_info->spd_revision_major, spd_info->spd_revision_minor);

    //printf("SPD NVM byte 0: 0x%02X\r\n", data[0]);   
    const uint16_t spd_eeprom_size[] = { 0, 256, 512, 1024, 2048};
    if(spd_info->spd_bytes_total <= 0b100) {
        printf("  EEPROM size: %d bytes\r\n", spd_eeprom_size[spd_info->spd_bytes_total]);
    } else {
        printf("  EEPROM size: Unknown (0x%02X)\r\n", spd_info->spd_bytes_total);
    }

    printf("  Beta Level: %d\r\n", (spd_info->beta_level_b4<<4)|spd_info->beta_level_b03);

    //printf("SPD NVM byte 3: 0x%02X\r\n", data[3]);
    if(spd_info->hybrid_media) {
        printf("  Module Type: Hybrid, aborting...\r\n");
        return true;
    }
    printf("  Module Type: ");
    switch(spd_info->module_type){
        case 0b0001:printf("RDIMM\r\n");break;
        case 0b0010:printf("UDIMM\r\n");break;
        case 0b0011:printf("SODIMM\r\n");break;
        case 0b0100:printf("LRDIMM\r\n");break;
        case 0b0101:printf("CUDIMM\r\n");break;
        case 0b0110:printf("CSODIMM\r\n");break;
        case 0b0111:printf("MRDIMM\r\n");break;
        case 0b1000:printf("CAMM2\r\n");break;
        case 0b1010:printf("DDIMM\r\n");break;
        case 0b1011:printf("Solder down\r\n");break;
        default:
            printf("Unknown (0x%02X), aborting...\r\n", spd_info->module_type);
            return true;
    }
    
    printf("First SDRAM: 0x%02X, 0x%02X, 0x%02X, 0x%02X\r\n", data[4], data[5], data[6], data[7]);
    ddr5_sdram_info(&spd_info->first_sdram);
    printf("Second SDRAM: 0x%02X, 0x%02X, 0x%02X, 0x%02X\r\n", data[8], data[9], data[10], data[11]);
    ddr5_sdram_info(&spd_info->second_sdram);
    return false;
}

bool ddr5_nvm_jedec_decode_manuf(uint8_t *data){
    //use the struct to extract the manufacturing information
    ddr5_nvm_jedec_manuf_info_t* manuf_info = (ddr5_nvm_jedec_manuf_info_t*)data;

    printf("  Module Manuf. Code: 0x%02X%02X (%s)\r\n", manuf_info->manuf_code[0], manuf_info->manuf_code[1], ddr5_decode_jep106_jedec_id(manuf_info->manuf_code[0], manuf_info->manuf_code[1]));
    printf("  Module Manuf. Location: 0x%02X\r\n", manuf_info->manuf_location);
    printf("  Module Manuf. Date: %02XY/%02XW\r\n", manuf_info->manuf_date[0], manuf_info->manuf_date[1]);
    printf("  Module Serial Number: 0x%02X%02X%02X%02X\r\n", manuf_info->serial_number[0], manuf_info->serial_number[1], manuf_info->serial_number[2], manuf_info->serial_number[3]);
    printf("  Module Part Number: "); 
    for(uint8_t i=0; i<30; i++){
        printf("%c",  manuf_info->part_number[i]); //this is not null-terminated, so we print all 30 bytes
    }
    printf("\r\n  Module Revision Code: 0x%02X\r\n", manuf_info->revision_code);
    printf("  DRAM Manuf. Code: 0x%02X%02X (%s)\r\n", manuf_info->dram_manuf_code[0], manuf_info->dram_manuf_code[1], ddr5_decode_jep106_jedec_id(manuf_info->dram_manuf_code[0], manuf_info->dram_manuf_code[1]));
    printf("  DRAM Stepping: 0x%02X\r\n", manuf_info->dram_stepping);
    return false;
}

//search in EEPROM bits of data, with up to X=10 trailing 0s
void ddr5_search_upa(uint8_t *data, uint32_t start, uint32_t end) {
    uint8_t data_found = 0;
    uint32_t start_address, total_bytes=0;

    for(uint32_t i=start; i<end; i++){
        if(data[i] == 0x00 && !data_found) continue;
        if(!data_found){
            //printf("\r\n\r\n@ 0x%03X:", i);
            start_address = i; 
        }

        if(data[i]==0x00){
            data_found--;
            if(!data_found){
                //show the found data
                printf("\r\nFound data at 0x%03X: %d bytes\r\n", start_address, total_bytes);
                // align the start address to 16 bytes, and calculate the end address
                struct hex_config_t hex_config;
                //ui_hex_init_config(&hex_config);
                ui_hex_get_args_config(&hex_config); //get the quiet flag, everything else is overridden
                hex_config.max_size_bytes= DDR5_SPD_SIZE; // maximum size of the device in bytes
                //ui_hex_get_args_config(&hex_config);
                hex_config.start_address=start_address; //set the start address
                hex_config.requested_bytes=total_bytes; //set the requested bytes
                ui_hex_align_config(&hex_config);
                ui_hex_header_config(&hex_config);
                //read 1024 bytes from the DDR5 SPD NVM
                for(uint32_t i=hex_config._aligned_start; i<(hex_config._aligned_end+1); i+=16) {
                    ui_hex_row_config(&hex_config, i, &data[i], 16);
                }
                total_bytes = 0; //reset total bytes
            }else{
                total_bytes++;
            }
        } else {
            total_bytes++;
            data_found = 10; //reset counter
        }        
        //printf(" 0x%02X", data[i]);
    }
}

bool ddr5_nvm_search(uint8_t *data) {
    printf("\r\nSearching Manuf. Specific Data area block 9:");
    ddr5_search_upa(data, 0x22B, 0x27F); //search in 0x22B-0x27F
    printf("\r\n\r\nSearching End User Programmable Area blocks 10-15:");
    ddr5_search_upa(data, 0x280, 0x3FF); //search in 0x280-0x3FF
    printf("\r\n\r\n");
    return false;
}

/*
*
* DDR5 SPD command functions
*
*/

bool ddr5_probe(uint8_t *buffer) {
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
    return false;
}

bool ddr5_dump(uint8_t *buffer) {
    //detect if spd present
    if(ddr5_detect_spd_quick()) return true; //check if the device is DDR5 SPD

    // align the start address to 16 bytes, and calculate the end address
    struct hex_config_t hex_config;
    hex_config.max_size_bytes= DDR5_SPD_SIZE; // maximum size of the device in bytes
    ui_hex_get_args_config(&hex_config);
    ui_hex_align_config(&hex_config);
    ui_hex_header_config(&hex_config);
    //read 1024 bytes from the DDR5 SPD NVM
    if(ddr5_read_pages_128bytes(true, 0, 8, buffer)) return true; //read EEPROM page 0-7, start at 0x00
    for(uint32_t i=hex_config._aligned_start; i<(hex_config._aligned_end+1); i+=16) {
        ui_hex_row_config(&hex_config, i, &buffer[i], 16);
    }

}

bool ddr5_read_to_file(FIL *file_handle, uint8_t *buffer) {
    //detect if spd present
    if(ddr5_detect_spd_quick()){
        file_close(file_handle); // close the file if there was an error
        return true; // check if the device is DDR5 SPD
    }
    //read 1024 bytes from the DDR5 SPD NVM
    if(ddr5_read_pages_128bytes(true, 0, 8, buffer)) {
        file_close(file_handle); // close the file if there was an error
        return true; // if the read was unsuccessful
    }
    if(file_write(file_handle, buffer, DDR5_SPD_SIZE)) { 
        return true; // if the write was unsuccessful (file closed in lower layer)
    }
    // close the file
    f_close(file_handle); // close the file
    return false;
}

bool ddr5_lock_bits_write_verify(uint8_t lb0, uint8_t lb1) {
    // unlock the block lock bits
    uint8_t lock_bits[3] = {DDR5_SPD_MR12, lb0, lb1}; // 0x0c is the lock bits register
    if(i2c_write(DDR5_SPD_I2C_WRITE_ADDR, lock_bits, 3u)) {
        return true;
    }

    if(ddr5_wait_idle()) return true;

    //verify lock bits
    if(i2c_transaction(DDR5_SPD_I2C_WRITE_ADDR, (uint8_t[]){DDR5_SPD_MR12}, 1u, lock_bits, 2)) {
        return true;
    }

    if(lock_bits[0] != lb0 || lock_bits[1] != lb1) {
        printf("Error: Lock bits not written successfully, expected 0x%02X 0x%02X, got 0x%02X 0x%02X\r\n", lb0, lb1, lock_bits[0], lock_bits[1]);
        printf("Is HSA pin grounded to enable Write Protect Override?\r\n");
        return true; // if not, we cannot write to the NVM
    }
    return false; // if the lock bits were written successfully
}

bool ddr5_verify(FIL *file_handle, uint8_t *buffer) {
    if(ddr5_detect_spd_quick()){
        file_close(file_handle); // close the file
        return true;
    }    
    // check if the file size is 1024 bytes        
    if(file_size_check(file_handle, DDR5_SPD_SIZE)) return true; 

    uint8_t verify_buffer[128];
    bool verror = false; // flag to indicate if there was a verification error
    for(uint32_t i=0; i<DDR5_SPD_SIZE/128; i++){
        if(file_read(file_handle, buffer, 128, NULL)) return true; 
        if(ddr5_read_pages_128bytes(true, i, 1, verify_buffer)){
            file_close(file_handle); // close the file
            return true;
        }
        for(uint8_t j=0; j<128; j++){
            if(verify_buffer[j] != buffer[j]) {
                printf("Error: SPD NVM byte %d does not match file! (0x%02X != 0x%02X)\r\n", j+(i*128), verify_buffer[j], buffer[j]);
                verror = true; // set the verification error flag
            }
        }
    }

    // close the file
    file_close(file_handle); // close the file
    return verror; // return true if there was a verification error, false otherwise
}

//true for error, false for success
bool ddr5_write_from_file(FIL *file_handle, uint8_t *buffer) {
    uint32_t bytes_read;
    
    // is file size 1024 bytes?
    if(file_size_check(file_handle, DDR5_SPD_SIZE)) return true; // if not, we cannot write to the NVM

    //check file CRC
    if(file_read(file_handle, buffer, 512, NULL)) return true;
    if(f_rewind(file_handle)) { // rewind the file to the beginning
        printf("Error rewinding file\r\n");
        goto ddr5_write_error; 
    }
    if(ddr5_nvm_jedec_crc(buffer)) return true; // check CRC of the first 512 bytes

    //detect if spd present
    if(ddr5_detect_spd_quick()) goto ddr5_write_error; //check if the device is DDR5 SPD

    //check if write enabled (HSA pin grounded)
    if(i2c_transaction(DDR5_SPD_I2C_WRITE_ADDR, (uint8_t[]){DDR5_SPD_MR48}, 1u, buffer, 1)) goto ddr5_write_error; // read the Device Status Register (MR48)
    // is write enabled? check 0x30 bit 2
    if(buffer[0] & DDR5_SPD_MR48_OVERRIDE_STATUP) {
        printf("Write Protect Override is enabled: OK\r\n");
    } else {
        printf("Write Protect Override is not enabled, cannot write to SPD NVM. Is HSA pin grounded?\r\n");
        goto ddr5_write_error; // if not, we cannot write to the NVM
    }

    //read current lock bits
    uint8_t original_lock_bits[2];
    if(i2c_transaction(DDR5_SPD_I2C_WRITE_ADDR, (uint8_t[]){DDR5_SPD_MR12}, 1u, original_lock_bits, 2)) {
        goto ddr5_write_error; // read the NVM block lock bits
    }
    printf("Saving NVM block lock bits: 0x%02X 0x%02X\r\n", original_lock_bits[0], original_lock_bits[1]);

    // unlock the block lock bits
    if(ddr5_lock_bits_write_verify(0x00, 0x00)) {
        goto ddr5_write_error; // if the lock bits were not written successfully
    }
    printf("NVM block lock bits cleared: OK\r\n");

    //write 16 byte pages to the DDR5 SPD NVM
    // poll 0x30 bit 3 for 0
    //8 pages of 128 bytes
    //page needs to be updated every 128 bytes in MR11
    printf("Writing page:");
    for(uint8_t i=0; i<8; i++){
        printf(" %d,", i);
        if(file_read(file_handle, buffer, 128, NULL)) return true; // read 128 bytes from the file

        if(ddr5_set_legacy_page(i)) goto ddr5_write_error; // set the page for the legacy mode  

        for(uint8_t j=0; j<128/16; j++){
            //get the bytes into an array so we don't have to use granular function
            uint8_t page_data[17];
            page_data[0] = DDR5_SPD_ACCESS_NVM | (j*16); // address to write to
            for(uint8_t k=0; k<16; k++) {
                page_data[k+1] = buffer[(j*16)+k]; // data to write
            }

            if(i2c_write(DDR5_SPD_I2C_WRITE_ADDR, page_data, 17u)){
                printf("\r\nError writing page %d, chunk %d\r\n", i, j);   
                goto ddr5_write_error; // write the access register and address to write to
            }
 
            // wait for the write to complete
            if(ddr5_wait_idle()) return true; // wait until the write operation is complete
        }
    }
    printf(" Done!\r\n");

    //restore NVM lock bits
    if(ddr5_lock_bits_write_verify(original_lock_bits[0], original_lock_bits[1])) {
        goto ddr5_write_error; // if the lock bits were not written successfully
    }
    printf("NVM block lock bits restored: 0x%02X 0x%02X\r\n", original_lock_bits[0], original_lock_bits[1]);

    printf("Verify write\r\n");
    f_rewind(file_handle); // rewind the file to the beginning
    if(ddr5_verify(file_handle, buffer)){
        printf("Verify: Failed!\r\n");
        return true; // on fail file is closed in ddr5_verify
    }else{
        printf("Verify: OK\r\n"); //on success file is closed in ddr5_verify
    }

    //file_close(file_handle); // close the file
    return false;

ddr5_write_error:
    printf("Error writing to DDR5 SPD NVM\r\n");
    file_close(file_handle); // close the file
    system_config.error = true; // set the error flag
    return true;
}

bool ddr5_crc_file(FIL *file_handle, char *buffer){
    //get file size
    if(file_size_check(file_handle, DDR5_SPD_SIZE)) return true; // check if the file size is 1024 bytes
    if(file_read(file_handle, buffer, 512, NULL)) return true; // read the first 512 bytes from the file
    if(file_close(file_handle)) return true; // close the file
    // check the CRC of the first 512 bytes
    return ddr5_nvm_jedec_crc(buffer); 
}

/// @brief lock and unlock a NVM block
/// @param block NWM block number (0-15)
/// @param lock true to lock, false to unlock
/// @param update true to update the lock bits, false to just read them
/// @return true on error, false on success
bool ddr5_lock(uint8_t block, bool lock, bool update) {
    uint8_t old_data[2];

    //detect if spd present
    if(ddr5_detect_spd_quick()) return true; //check if the device is DDR5 SPD
    
    //read current lock bits (0xc, 0xd)
    //old_data[0]=DDR5_SPD_MR12;
    if(i2c_transaction(DDR5_SPD_I2C_WRITE_ADDR, (uint8_t[]){DDR5_SPD_MR12}, 1u, old_data, 2)) {
        return true;
    }

    uint8_t new_data[3]={DDR5_SPD_MR12, old_data[0], old_data[1]}; // prepare new data for writing
    if(update){
        // Update lock bits
        if (block < 8) {
            new_data[1] = lock ? (old_data[0] | (1u << block)) : (old_data[0] & ~(1u << block));
        } else {
            new_data[2] = lock ? (old_data[1] | (1u << (block - 8))) : (old_data[1] & ~(1u << (block - 8)));
        }
        //write the new lock bits
        new_data[0] = DDR5_SPD_MR12; //register to write
        if(i2c_write(DDR5_SPD_I2C_WRITE_ADDR, new_data, 3u)) {
            return true;
        }
    }

    uint8_t verify_data[2];
    if(i2c_transaction(DDR5_SPD_I2C_WRITE_ADDR, (uint8_t[]){DDR5_SPD_MR12}, 1u, verify_data, 2)) {
        return true; // read the NVM block lock bits
    }

    if(update){
        if(new_data[1] != verify_data[0] || new_data[2] != verify_data[1]) {
            printf("Error: Lock bits not written successfully, expected 0x%02X 0x%02X, got 0x%02X 0x%02X\r\n", verify_data[0], verify_data[1], new_data[1], new_data[2]);
            printf("Is HSA pin grounded to enable Write Protect Override?\r\n");
            return true; // if not, we cannot write to the NVM
        }
        printf("Lock bits set successfully: 0x%02X 0x%02X\r\n", new_data[1], new_data[2]);
    }

    ddr5_protection_block_table(old_data, verify_data, true); // print the lock bits table
    return false;
}

static const char* const usage[] = {
    "ddr5 [probe|dump|write|read|verify|lock|unlock|crc]\r\n\t[-f <file>] [-b <block number>|<bytes>] [-s <start address>] [-h(elp)]",
    "Probe DDR5 SPD:%s ddr5 probe",
    "Show DDR5 SPD NVM contents:%s ddr5 dump",
    "Show 32 bytes starting at address 0x50:%s ddr5 dump -s 0x50 -b 32",
    "Write SPD NVM from file, verify:%s ddr5 write -f example.bin",
    "Read SPD NVM to file, verify:%s ddr5 read -f example.bin",
    "Verify against file:%s ddr5 verify -f example.bin",
    "Show NVM block lock status:%s ddr5 lock -or- ddr5 unlock",
    "Lock a NVM block 0-15:%s ddr5 lock -b 0",
    "Unlock a NVM block 0-15:%s ddr5 unlock -b 0",
    "Check/generate CRC for JEDEC blocks 0-7:%s ddr5 crc -f example.bin",
    "DDR5 write file **MUST** be exactly 1024 bytes long"
};

static const struct ui_help_options options[] = {
    { 1, "", T_HELP_DDR5 },               // flash command help  
    { 0, "probe", T_HELP_DDR5_PROBE },    // probe
    { 0, "dump", T_HELP_DDR5_DUMP },      // dump
    { 0, "write", T_HELP_DDR5_WRITE },    // write
    { 0, "read", T_HELP_DDR5_READ },      // read
    { 0, "verify", T_HELP_DDR5_VERIFY },  // verify
    { 0, "lock", T_HELP_DDR5_LOCK },      // lock
    { 0, "unlock", T_HELP_DDR5_UNLOCK },  // unlock
    { 0, "crc", T_HELP_DDR5_CRC },        // crc
    { 0, "-f", T_HELP_DDR5_FILE_FLAG },   // file to read/write/verify
    { 0, "-s", UI_HEX_HELP_START }, // start address for dump
    { 0, "-b", UI_HEX_HELP_BYTES }, // bytes to dump
    { 0, "-q", UI_HEX_HELP_QUIET}, // quiet mode, disable address and ASCII columns
    { 0, "-b", T_HELP_DDR5_BLOCK_FLAG },  
    { 0, "-h", T_HELP_HELP }               // help flag
};

enum ddr5_actions_enum {
    DDR5_PROBE=0,
    DDR5_DUMP,
    DDR5_READ,
    DDR5_WRITE,
    DDR5_VERIFY,
    DDR5_LOCK,
    DDR5_UNLOCK,
    DDR5_CRC
};

static const struct cmdln_action_t ddr5_actions[] = {
    { DDR5_PROBE, "probe" },
    { DDR5_DUMP, "dump" },
    { DDR5_READ, "read" },
    { DDR5_WRITE, "write" },
    { DDR5_VERIFY, "verify" },
    { DDR5_LOCK, "lock" },
    { DDR5_UNLOCK, "unlock" },
    { DDR5_CRC, "crc" }
};

void ddr5_handler(struct command_result* res) {
    if (ui_help_show(res->help_flag, usage, count_of(usage), &options[0], count_of(options))) {
        return;
    }
    if (!ui_help_sanity_check(true,0x00)) {
        return;
    }

    uint32_t action;
    if(cmdln_args_get_action(ddr5_actions, count_of(ddr5_actions), &action)){
        ui_help_show(true, usage, count_of(usage), &options[0], count_of(options));
        return;        
    }

    char file[13];
    FIL file_handle;                                                  // file handle
    if ((action == DDR5_WRITE || action == DDR5_READ || action== DDR5_VERIFY || action == DDR5_CRC)) {
        
        if(file_get_args(file, sizeof(file))){; // get the file name from the command line arguments
            return;
        }

        uint8_t file_status;
        if(action==DDR5_READ){
            file_status = FA_CREATE_ALWAYS | FA_WRITE;
        }else{
            file_status = FA_READ; // open the file for reading
        }
        if(file_open(&file_handle, file, file_status)) return; // create the file, overwrite if it exists
    }
    
    command_var_t arg;
    uint32_t block_flag;
    bool lock_update=false;
    if(action == DDR5_LOCK || action == DDR5_UNLOCK) {
        if(!cmdln_args_find_flag_uint32('b', &arg, &block_flag)){ // block to lock/unlock
            if(arg.has_arg){
                printf("Missing block number: -b <block number>\r\n");
                return;
            }else{ //no block, just show current status
                lock_update = false; //we will not update the lock bits, just read them
            }
        }else if(block_flag > 15) {
            printf("Block number must be between 0 and 15\r\n");
            return;
        }else{
            lock_update = true; //we will update the lock bits
        }
    }

    fala_start_hook();  
    uint8_t buffer[DDR5_SPD_SIZE]; // buffer to store the file data
    switch(action) {
        case DDR5_PROBE:
            ddr5_probe(buffer);
            break;
        case DDR5_DUMP:
            ddr5_dump(buffer);
            break;
        case DDR5_READ:
            printf("Read SPD NVM to file: %s\r\n", file);
            if(ddr5_read_to_file(&file_handle, buffer)) {
                printf("Error!");
            }else{
                printf("Success :)");
            }
            break;
        case DDR5_WRITE:
            //TODO: CRC check!
            //TODO: single "write ready?" function to check write enabled, lock bits, etc
            //TODO: restore lock bits after write
            printf("Write SPD NVM from file: %s\r\n", file);
            if(ddr5_write_from_file(&file_handle, buffer)) {
                printf("Error!\r\n");
            } else {
                printf("Success :)");
            }
            break;
        case DDR5_VERIFY:
            printf("Verifying SPD NVM against file: %s\r\n", file);
            if(ddr5_verify(&file_handle, buffer)) {
                printf("Error!\r\n");
            } else {
                printf("Success :)\r\n");
            }
            break;
        case DDR5_LOCK:
            // show status before and after lock?
            if(lock_update) printf("Locking NVM block %d\r\n", block_flag);
            ddr5_lock(block_flag, true, lock_update);
            break;
        case DDR5_UNLOCK:
            if(lock_update) printf("Unlocking NVM block %d\r\n", block_flag);
            ddr5_lock(block_flag, false, lock_update);
            break;
        case DDR5_CRC:
            printf("Checking CRC for JEDEC blocks 0-7, file: %s\r\n", file);
            ddr5_crc_file(&file_handle, buffer);
            break;
        default:
            printf("Unknown action\r\n");
            break;
    }
    //we manually control any FALA capture
    fala_stop_hook();
    fala_notify_hook();
}    