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
#include "pirate/storage.h" // File system related

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

float ddr5_get_temperature(uint8_t t_high, uint8_t t_low, uint8_t t_resolution) {
   // Extract the sign bit from the high byte
   int8_t sign = (t_high & 0x10) ? -1 : 1;

   // Combine the high and low bytes into a 16-bit value
   uint16_t raw_value = ((t_high & 0x0F) << 8) | t_low;

   raw_value = raw_value >> (3-t_resolution); // Adjust for resolution
   // Determine the resolution multiplier
   float multiplier;
   switch (t_resolution) {
       case 0x00: // 9-bit resolution (0.5°C)
           multiplier = 0.5f;
           break;
       case 0x01: // 10-bit resolution (0.25°C)
           multiplier = 0.25f;
           break;
       case 0x02: // 11-bit resolution (0.125°C)
           multiplier = 0.125f;
           break;
       case 0x03: // 12-bit resolution (0.0625°C)
           multiplier = 0.0625f;
           break;
       default:
           multiplier = 0.0f; // Invalid resolution
           break;
   }

    //printf("Raw Value: 0x%04X, Sign: %d, Multiplier: %f\r\n", raw_value, sign, multiplier);
   // Calculate the temperature
   float temperature = sign * raw_value * multiplier;
   return temperature;
}

// 0 if ok, 1 if error
bool ddr5_sdram_info(uint8_t b1, uint8_t b2, uint8_t b3){

    
    static const uint8_t spd_sdram_density[]={0,4,8,12,16,24,32,48,64};
    if((b1 & 0b11111)==0){
        printf("  Not present\r\n");
        return false;
    }else if( (b1 & 0b11111) <= 0b1000) {
        printf("  Density: %d Gb\r\n", spd_sdram_density[b1 & 0b11111]);
    } else {
        printf("  Density: Unknown (0x%02X), aborting...\r\n", b1 & 0b11111);
    }

    static const uint8_t spd_sram_dies[]={1,2,2,4,8,16};
    if((b1>>5) <= 0b101) {
        printf("  Number of SRAM Dies: %d\r\n", spd_sram_dies[b1>>5]);
    } else {
        printf("  Number of SRAM Dies: Unknown (0x%02X), aborting...\r\n", b1>>5);
    }

    static const uint8_t spd_sdram_width[]={4,8,16,32};
    if(b2>>5 <=0b11) {
        printf("  Width: x%d\r\n", spd_sdram_width[b2>>5]);
    } else {
        printf("  Width: Unknown (0x%02X), aborting...\r\n",b2>>5);
    }

    static const uint8_t spd_sdram_groups[]={1,2,4,8};
    if(((b3 >> 5) & 0b111) <= 0b11) {
        printf("  Number of SDRAM Groups: %d\r\n", spd_sdram_groups[(b3 >> 5) & 0b111]);
    } else {
        printf("  Number of SDRAM Groups: Unknown (0x%02X), aborting...\r\n", (b3 >> 5) & 0b111);
    }

    static const uint8_t spd_sdram_banks[]={1,2,4};
    if((b3 & 0b111) <= 0b010) {
        printf("  Number of SDRAM Banks: %d\r\n", spd_sdram_banks[b3 & 0b111]);
    } else {
        printf("  Number of SDRAM Banks: Unknown (0x%02X), aborting...\r\n", b3 & 0b111);
    }

    return false;
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

//search in EEPROM bits of data, with up to X=10 trailing 0s
void ddr5_search_upa(uint8_t *data, uint32_t start, uint32_t end) {
    uint8_t data_found = 0;
    for(uint32_t i=start; i<end; i++){
        if(data[i] == 0x00 && !data_found){
            //data_found = 0;
            continue; //skip empty bytes
        }
        if(!data_found) {
            printf("\r\n\r\n@ 0x%03X:", i+512);
        }
        if(data[i]==0x00){
            data_found--;
        } else {
            data_found = 10; //reset counter
        }        
        printf(" 0x%02X", data[i]);

    }
}

bool ddr5_set_legacy_page(uint8_t page){
    //set the page for the legacy mode
    uint8_t data[2];
    data[0] = 0x0B; //MR11: I2C Legacy Mode Device Configuration
    data[1] = page & 0b111; //page 0-7
    if(pio_i2c_write_array_timeout(0xa0, data, 2u, 0xffffu)) {
        printf("Device not detected (no ACK)\r\n");
        return true;
    }
    //read to verify
    data[0] = 0x0B; //read EEPROM
    if(pio_i2c_transaction_array_repeat_start(0xa0, data, 1u, data, 1, 0xffffu)){
        printf("Device not detected (no ACK)\r\n");
        return true;
    }
    if(data[0] != page) {
        printf("Error: Page %d not set, read back %d\r\n", page, data[0]);
        return true;
    }
    return false;
}

bool ddr5_read_pages_128bytes(bool eeprom, uint8_t start_page, uint8_t num_pages, uint8_t* data) {
    if(!eeprom){
        data[0]=0x00;//read volatile memory, start at 0x00
    }else{
        data[0]=0x80;//read EEPROM page start at 0x00 
        if(ddr5_set_legacy_page(start_page)) return true; //set the page for the legacy mode
    }
    if (pio_i2c_transaction_array_repeat_start(0xa0, data, 1u, data, num_pages*128, 0xffffu)) {
        printf("Device not detected (no ACK)\r\n");
        return true;
    }
    return false;
}

bool ddr5_detect_spd(ddr5_spd_volatile_t* spd){
    printf("Device Type: 0x%02X%02X\r\n", spd->device_type_msb, spd->device_type_lsb);
    if((spd->device_type_msb != 0x51) || (spd->device_type_lsb != 0x18)){
        printf("Error: Device Type does not match expected values (0x51 0x18)\r\n");
        return true;
    }
    return false;
}

// include file from openocd/src/helper
static const char * const jep106[][126] = {
    #include "../../lib/bluetag/src/jep106.inc"
};

static const char *jep106_table_manufacturer(uint8_t b0, uint8_t b1){
    int bank=(b0 & 0xf);
    int id=(b1 & 0x7f); //SPD JEDEC ID is lower 7 bits, no parity bit. Bit 7 = 1 = bank > 0
	if (id < 1 || id > 126) {
		return "Unknown";
	}
	/* index is zero based */
	id--;
	if (bank >= sizeof(jep106) || jep106[bank][id] == 0)
		return "Unknown";
	return jep106[bank][id];
}

bool ddr5_decode_volatile_memory(ddr5_spd_volatile_t* spd) {
    printf("Device Revision: %d.%d\r\n", (spd->device_revision_major+1), spd->device_revision_minor);
    printf("Vendor ID: 0x%02X%02X (%s)\r\n", spd->vendor_id_byte0, spd->vendor_id_byte1, jep106_table_manufacturer(spd->vendor_id_byte0, spd->vendor_id_byte1));
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
    printf("Write Protection for NVM Blocks: 0x%02X%02X\r\n", spd->write_protection_nvm_blocks_high, spd->write_protection_nvm_blocks_low);
    for(uint8_t i=0; i<8; i++){
        printf("  Block %d: %s\r\n", i, (spd->write_protection_nvm_blocks_low & (1u << i)) ? "Protected" : "Unprotected");
    }
    for(uint8_t i=0; i<8; i++){
        printf("  Block %d: %s\r\n", i+8, (spd->write_protection_nvm_blocks_high & (1u << i)) ? "Protected" : "Unprotected");
    }    
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

bool ddr5_nvm_jedec_crc(uint8_t *data){
    printf("\r\nCRC verify\r\nStored CRC (bytes 510:511): 0x%02X 0x%02X\r\n", data[510], data[511]);
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

bool ddr5_nvm_jedec_decode_data(uint8_t *data) {
    //print the first few SPD bytes as hex first, then decode
    printf("\r\nSPD bytes 0-3: 0x%02X, 0x%02X, 0x%02X, 0x%02X\r\n", data[0], data[1], data[2], data[3]);
    //printf("\r\nSPD NVM byte 2: 0x%02X\r\n", data[2]);
    if(data[2] == 0x12){
        printf("  Host Bus Type: DDR5 SDRAM\r\n");
    }else{
        printf("  Host Bus Type: Unknown (0x%02X), aborting...\r\n", data[2]);
        return true;
    }

    //printf("SPD NVM byte 1: 0x%02X\r\n", data[1]);
    printf("  SPD Revision: %d.%d\r\n", (data[1] >> 4), data[1] & 0b1111);

    //printf("SPD NVM byte 0: 0x%02X\r\n", data[0]);   
    uint8_t cnt = (data[0]>>4)&0b111;
    const uint16_t spd_eeprom_size[] = { 0, 256, 512, 1024, 2048};
    if(cnt <= 0b100) {
        printf("  EEPROM size: %d bytes\r\n", spd_eeprom_size[cnt]);
    } else {
        printf("  EEPROM size: Unknown (0x%02X)\r\n", cnt);
    }
    cnt = data[0]>>7 | (data[0] & 0b1111);
    printf("  Beta Level: %d\r\n", cnt);

    //printf("SPD NVM byte 3: 0x%02X\r\n", data[3]);
    if(data[3] & 0b10000000) {
        printf("  Module Type: Hybrid, aborting...\r\n");
        return true;
    }

    printf("  Module Type: ");
    switch(data[3]&0xf){
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
            printf("Unknown (0x%02X), aborting...\r\n", data[3]);
            return true;
    }
    
    printf("First SDRAM: (0x%02X, 0x%02X, 0x%02X, 0x%02X)\r\n", data[4], data[5], data[6], data[7]);
    ddr5_sdram_info(data[4], data[6], data[7]);
    printf("Second SDRAM: (0x%02X, 0x%02X, 0x%02X, 0x%02X)\r\n", data[8], data[9], data[10], data[11]);
    ddr5_sdram_info(data[8], data[10], data[11]);
    return false;
}

bool ddr5_nvm_jedec_decode_manuf(uint8_t *data){
    printf("Module Manuf. Code: 0x%02X%02X (%s)\r\n", data[0], data[1], jep106_table_manufacturer(data[0], data[1]));
    printf("Module Manuf. Location: 0x%02X\r\n", data[2]);
    printf("Module Manuf. Date: %02XY/%02XW\r\n", data[3], data[4]);
    printf("Module Serial Number: 0x%02X%02X%02X%02X\r\n", data[5], data[6], data[7], data[8]);
    printf("Module Part Number: ");
    for(uint8_t i=0; i<30; i++){
        printf("%c", data[9+i]);
    }
    printf("\r\nModule Revision Code: 0x%02X\r\n", data[39]);
    printf("DRAM Manuf. Code: 0x%02X%02X (%s)\r\n", data[40], data[41], jep106_table_manufacturer(data[40], data[41]));
    printf("DRAM Stepping: 0x%02X\r\n", data[42]);
    return false;
}

bool ddr5_nvm_search(uint8_t *data) {
    printf("\r\nSearching Manuf. Specific Data area block 9:");
    ddr5_search_upa(data, 555-512, 0x27F-512); //search in 0x240-0x27F (576-639)
    printf("\r\n\r\nSearching End User Programmable Area blocks 10-15:");
    ddr5_search_upa(data, 0x280-512, 0x3FF-512); //search in 0x280-0x3FF (640-1023)
    printf("\r\n\r\n");
    return false;
}

bool ddr5_probe(void) {
    // read 128 bytes from the DDR5 SPD Volatile Memory
    // cast it to the ddr5_spd_volatile_t structure
    uint8_t data[512];

    if(ddr5_read_pages_128bytes(false, 0, 1, data)) return true; //read volatile memory, start at 0x00
    ddr5_spd_volatile_t* spd = (ddr5_spd_volatile_t*)data;
    
    if(ddr5_detect_spd(spd)) return true; //check if the device is DDR5 SPD
    
    ddr5_decode_volatile_memory(spd); //decode the volatile memory

    printf("\r\nSPD EEPROM JEDEC Data blocks 0-7:\r\n");
    if(ddr5_read_pages_128bytes(true, 0, 4, data)) return true; //read EEPROM page 0-4, start at 0x00
    if(ddr5_nvm_jedec_crc(data)) return true; //check CRC of the first 512 bytes
    if(ddr5_nvm_jedec_decode_data(data)) return true; //decode the first 512 bytes of the EEPROM

    printf("\r\nSPD EEPROM JEDEC Manufacturing Information blocks 8-9:\r\n");
    if(ddr5_read_pages_128bytes(true, 0b100, 4, data)) return true; //read EEPROM page 5-8, start at 0x00
    if(ddr5_nvm_jedec_decode_manuf(data)) return true; //decode the manufacturing information
    if(ddr5_nvm_search(data)) return true; //search for the end user programmable area
    return false;
}

bool ddr5_detect_spd_quick(void){
    uint8_t data[2];
    data[0] = 0x00; //read volatile memory, start at 0x00
    if(pio_i2c_transaction_array_repeat_start(0xa0, data, 1u, data, 2, 0xffffu)) {
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

bool ddr5_dump(void) {
    uint8_t data[128];

    //detect if spd present
    if(ddr5_detect_spd_quick()) return true; //check if the device is DDR5 SPD

    //loop and read out eeprom_bytes_count of bytes from the EEPROM
    for(uint32_t i=0; i<1024/128; i++){
        if(ddr5_read_pages_128bytes(true, i, 1, data)) return true; //read the EEPROM page

        // for convenience, we read 128 byte "pages" that each have two JEDEC blocks of 64 bytes each
        for(uint8_t block=0; block<2; block++){
            printf("\r\nEEPROM Block %d:\r\n", (i*2)+block);
            for(uint32_t j=0; j<64; j++){
                printf(" 0x%02X", data[(block*64)+j]);
            }
        }    
        //save to file...
    } 
}


/// @brief lock and unlock a NVM block
/// @param block NWM block number (0-15)
/// @param lock true to lock, false to unlock
/// @param update true to update the lock bits, false to just read them
/// @return true on error, false on success
bool ddr5_lock(uint8_t block, bool lock, bool update) {
    uint8_t old_data[2];
    uint8_t new_data[3];

    //detect if spd present
    if(ddr5_detect_spd_quick()) return true; //check if the device is DDR5 SPD
    
    //read current lock bits (0xc, 0xd)
    old_data[0]=0x0c;
    if(pio_i2c_transaction_array_repeat_start(0xa0, old_data, 1u, old_data, 2, 0xffffu)) {
        printf("Device not detected (no ACK)\r\n");
        return true;
    }

    if(update){
        if(block < 8) {
            if(lock){
                new_data[1] = old_data[0] | (1u << block); //set the lock bit for the block
            } else {
                new_data[1] = old_data[0] & ~(1u << block); //clear the lock bit for the block
            }
            new_data[2] = old_data[1]; //keep the high byte unchanged
        } else {
            new_data[1] = old_data[0]; //keep the low byte unchanged
            if(lock){
                new_data[2] = old_data[1] | (1u << (block - 8)); //set the lock bit for the block
            } else {
                new_data[2] = old_data[1] & ~(1u << (block - 8)); //clear the lock bit for the block
            }
        }

        //write the new lock bits
        new_data[0] = 0x0c; //register to write
        if(pio_i2c_write_array_timeout(0xa0, new_data, 3u, 0xffffu)) {
            printf("Device not detected (no ACK)\r\n");
            return true;
        }
    }

    //read back to verify
    uint8_t verify_data[2];//register to read
    verify_data[0] = 0x0c; //register to read
    if(pio_i2c_transaction_array_repeat_start(0xa0, verify_data, 1u, verify_data, 2, 0xffffu)) {
        printf("Device not detected (no ACK)\r\n");
        return true;
    }

    if(update){

        if(verify_data[0] != new_data[1] || verify_data[1] != new_data[2]) {
            printf("Error: Lock bits not set correctly, read back 0x%02X 0x%02X\r\n", verify_data[0], verify_data[1]);
            return true;
        } else {
            printf("Lock bits set successfully: 0x%02X 0x%02X\r\n", verify_data[0], verify_data[1]);
        }
    }

    printf(" Block| Old State | New State\r\n");
    printf("------|-----------|-----------\r\n");
    for(uint8_t i=0; i<8; i++){
        printf("  %d   | %s | %s\r\n", i, (old_data[0] & (1u << i)) ? "Locked" : "Unlock", (verify_data[0] & (1u << i)) ? "Locked" : "Unlock");
    }
    for(uint8_t i=0; i<8; i++){
        printf("  %d   | %s | %s\r\n", i+8, (old_data[1] & (1u << i)) ? "Locked" : "Unlock", (verify_data[1] & (1u << i)) ? "Locked" : "Unlock");
    }
    return false;
}




static const char* const usage[] = {
    "ddr5 [probe|dump|write|read|verify|lock|unlock|crc]\r\n\t[-f <file>] [-b <block number>] [-h(elp)]",
    "Probe DDR5 SPD: ddr5 probe",
    "Show DDR5 SPD NVM contents: ddr5 dump",
    "Write SPD NVM from file, verify: ddr5 write -f example.bin",
    "Read SPD NVM to file, verify: ddr5 read -f example.bin",
    "Verify against file: ddr5 verify -f example.bin",
    "Show NVM block lock status: ddr5 lock -or- ddr5 unlock",
    "Lock a NVM block 0-15: ddr5 lock -b 0",
    "Unlock a NVM block 0-15: ddr5 unlock -b 0",
    "Check/generate CRC for JEDEC blocks 0-7: ddr5 crc -f example.bin",
    "DDR5 write file **MUST** be exactly 1024 bytes long"
};

static const struct ui_help_options options[] = {
    { 1, "", T_HELP_FLASH },               // flash command help
    { 0, "probe", T_HELP_FLASH_PROBE },    // probe
    { 0, "dump", T_HELP_FLASH_PROBE },      // dump
    { 0, "write", T_HELP_FLASH_WRITE },    // write
    { 0, "read", T_HELP_FLASH_READ },      // read
    { 0, "verify", T_HELP_FLASH_VERIFY },  // verify
    { 0, "lock", T_HELP_FLASH_TEST },      // test
    { 0, "unlock", T_HELP_FLASH_TEST },    // test
    { 0, "-f", T_HELP_FLASH_FILE_FLAG },   // file to read/write/verify
    { 0, "-b", T_HELP_FLASH_ERASE_FLAG },  // with erase (before write)
    { 0, "-h", T_HELP_HELP },   // help flag
};

enum ddr5_actions {
    DDR5_PROBE=0,
    DDR5_DUMP,
    DDR5_READ,
    DDR5_WRITE,
    DDR5_VERIFY,
    DDR5_LOCK,
    DDR5_UNLOCK,
    DDR5_CRC
};

struct ddr5_actions_t {
    enum ddr5_actions action;
    const char verb[7];
};

static const struct ddr5_actions_t ddr5_actions[] = {
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
    if (!ui_help_check_vout_vref()) {
        return;
    }

    int8_t action=-1;
    char action_str[7];
    // action is the first argument (read/write/probe/erase/etc)
    if (cmdln_args_string_by_position(1, sizeof(action_str), action_str)) {
        for(uint8_t i = 0; i < count_of(ddr5_actions); i++) {
            if (strcmp(action_str, ddr5_actions[i].verb) == 0) {
                action = ddr5_actions[i].action;
                break;
            }
        }
    }

    if( action == -1) {
        ui_help_show(true, usage, count_of(usage), &options[0], count_of(options));
        return;
    }

    FIL file_handle;                                                  // file handle
    FRESULT result;  
    char file[13];
    command_var_t arg;
    bool file_flag = cmdln_args_find_flag_string('f' | 0x20, &arg, sizeof(file), file);
    if ((action == DDR5_WRITE || action == DDR5_READ || action== DDR5_VERIFY || action == DDR5_CRC)) {
        if(!file_flag){
            printf("Missing file name: -f <filename>\r\n");
            return;
        }else{
            BYTE file_status;
            if(action==DDR5_READ){
                file_status = FA_CREATE_ALWAYS | FA_WRITE;
            }else{
                file_status = FA_READ; // open the file for reading
            }
            result = f_open(&file_handle, file, file_status); // create the file, overwrite if it exists
            if (result != FR_OK) {                                            // error
                printf("Read write error file %s\r\n", file);
                system_config.error = true; // set the error flag
                return;
            }
        }
    }

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
    //BSS138 on breakout board
    //fala_start_hook();  
    //TODO: reusable file function
    //TODO: check if write is enabled for block lock and write operations
    UINT bytes_written; // somewhere to store the number of bytes written
    UINT bytes_read; // somewhere to store the number of bytes read
    uint8_t rbuffer[128];
    uint8_t buffer[1024]; // buffer to store the file data
    switch(action) {
        case DDR5_PROBE:
            ddr5_probe();
            return;
        case DDR5_DUMP:
            ddr5_dump();
            return;
        case DDR5_READ:
            printf("Read SPD NVM to file: %s\r\n", file);
            //detect if spd present
            if(ddr5_detect_spd_quick()){
                result = f_close(&file_handle); // close the file
                res->error = true; // set the error flag
                return; //check if the device is DDR5 SPD
            }

            //read 1024 bytes from the DDR5 SPD NVM
            for(uint32_t i=0; i<1024/128; i++){
                if(ddr5_read_pages_128bytes(true, i, 1, rbuffer)){
                    result = f_close(&file_handle); // close the file
                    return;
                }
                result = f_write(&file_handle, rbuffer, 128, &bytes_written); // write the data to the file
                if (result != FR_OK) {                                              // if the write was successful
                    printf("Error writing to file %s\r\n", file);
                    result = f_close(&file_handle); // close the file
                    system_config.error = true; // set the error flag
                    return;
                }
                printf("Wrote %d bytes to file %s\r\n", bytes_written, file);
            }
            // close the file
            result = f_close(&file_handle); // close the file
            printf("Successfully read SPD NVM to file: %s\r\n", file);
            break;
        case DDR5_WRITE:
            //TODO: CRC check!
            //TODO: single "write ready?" function to check write enabled, lock bits, etc
            //TODO: restore lock bits after write
            printf("Write SPD NVM from file: %s\r\n", file);
            //detect if spd present
            if(ddr5_detect_spd_quick()){
                result = f_close(&file_handle); // close the file
                res->error = true; // set the error flag
                return; //check if the device is DDR5 SPD
            }
            #define SPD_DEVICE_STATUS_REG 0x30 // Device Status Register
            #define WP_OVERRIDE_STATUP 1u << 2
            #define WP_OP_STATUS 1u << 3
            if(pio_i2c_transaction_array_repeat_start(0xa0, (uint8_t[]){SPD_DEVICE_STATUS_REG}, 1u, rbuffer, 1, 0xffffu)) {
                printf("Device not detected (no ACK)\r\n");
                result = f_close(&file_handle); // close the file
                system_config.error = true; // set the error flag
                return;
            }
            // is write enabled? check 0x30 bit 2
            if(rbuffer[0] & WP_OVERRIDE_STATUP) {
                printf("Write Protect Override is enabled, proceeding with write\r\n");
            } else {
                printf("Write Protect Override is not enabled, cannot write to SPD NVM. Is HSA pin grounded?\r\n");
                result = f_close(&file_handle); // close the file
                system_config.error = true; // set the error flag
                return;
            }
            // is file size 1024 bytes?
            if(f_size(&file_handle)!=1024){ // get the file size
                printf("Error: File %s must be exactly 1024 bytes long\r\n", file);
                system_config.error = true; // set the error flag
                result = f_close(&file_handle); // close the file
                return;
            }
            // unlock and verify the block lock bits
            if(pio_i2c_write_array_timeout(0xa0, (uint8_t[]){0x0c, 0x00, 0x00}, 3u, 0xffffu)) {
                printf("Error: Could not unlock NVM blocks\r\n");
                result = f_close(&file_handle); // close the file
                system_config.error = true; // set the error flag
                return;
            }
            //verify lock bits
            uint8_t lock_bits[2];
            if(pio_i2c_transaction_array_repeat_start(0xa0, (uint8_t[]){0x0c}, 1u, lock_bits, 2, 0xffffu)) {
                printf("Error: Could not read NVM block lock bits\r\n");
                result = f_close(&file_handle); // close the file
                system_config.error = true; // set the error flag
                return;
            }
            if(lock_bits[0] != 0x00 || lock_bits[1] != 0x00) {
                printf("Error: NVM block lock bits are not cleared, cannot write to SPD NVM\r\n");
                printf("Current lock bits: 0x%02X 0x%02X\r\n", lock_bits[0], lock_bits[1]);
                result = f_close(&file_handle); // close the file
                system_config.error = true; // set the error flag
                return;
            } else {
                printf("NVM block lock bits cleared, proceeding with write\r\n");
            }
            //write 16 byte pages to the DDR5 SPD NVM
            // poll 0x30 bit 3 for 0
            //8 pages of 128 bytes
            //page needs to be updated every 128 bytes in MR11
            for(uint8_t i=0; i<8; i++){
                result = f_read(&file_handle, rbuffer, 128, &bytes_read); // read the data from the file
                if (result != FR_OK) {                                              // if the read was successful
                    printf("Error reading file %s\r\n", file);
                    system_config.error = true; // set the error flag
                    result = f_close(&file_handle); // close the file
                    return;
                }
                if(bytes_read != 128) { // check if we read exactly 128 bytes
                    printf("Error reading 128 bytes from file %d\r\n", i);
                    system_config.error = true; // set the error flag
                    result = f_close(&file_handle); // close the file
                    return;
                }
                if(ddr5_set_legacy_page(i)) goto ddr5_write_error; // set the legacy page to write to
                for(uint8_t j=0; j<128/16; j++){
                    // need to send the address to write to, then the data
                    if(pio_i2c_start_timeout(0xfffff)) goto ddr5_write_error; // start the I2C transaction
                    if(pio_i2c_write_timeout(0xa0, 0xffff)) goto ddr5_write_error; // write the device address
                    if(pio_i2c_write_timeout(0x80 + (j*16), 0xffff))goto ddr5_write_error; // write the address to write to
                    for(uint8_t k=0; k<16; k++) {
                        if(pio_i2c_write_timeout(rbuffer[j*16+k], 0xffff))goto ddr5_write_error; // write the data byte
                    }
                    if(pio_i2c_stop_timeout(0xffff))goto ddr5_write_error; // stop the I2C transaction
                    // wait for the write to complete
                    uint8_t status;
                    do {
                        // read the status register 0x30
                        if(pio_i2c_transaction_array_repeat_start(0xa0, (uint8_t[]){SPD_DEVICE_STATUS_REG}, 1u, &status, 1, 0xffffu)) {
                            printf("Error reading SPD Device Status Register\r\n");
                            system_config.error = true; // set the error flag
                            result = f_close(&file_handle); // close the file
                            return;
                        }
                    } while(status & WP_OP_STATUS); // wait until the write operation is complete
                }
            }
            result = f_close(&file_handle); // close the file
            break;
ddr5_write_error:
            printf("Error writing to DDR5 SPD NVM\r\n");
            result = f_close(&file_handle); // close the file
            system_config.error = true; // set the error flag
            break;
        case DDR5_VERIFY:
            printf("Verifying SPD NVM against file: %s\r\n", file);
            //detect if spd present
            if(ddr5_detect_spd_quick()){
                result = f_close(&file_handle); // close the file
                res->error = true; // set the error flag
                return; //check if the device is DDR5 SPD
            }            
            //open file for reading, then varify against the SPD NVM page by page
            //get file size
            if(f_size(&file_handle)!=1024){ // get the file size
                printf("Error: File %s must be exactly 1024 bytes long\r\n", file);
                system_config.error = true; // set the error flag
                result = f_close(&file_handle); // close the file
                return;
            }
            bool verror = false; // flag to indicate if there was a verification error
            for(uint32_t i=0; i<1024/128; i++){
                result = f_read(&file_handle, rbuffer, 128, &bytes_read); // read the data from the file
                if (result != FR_OK) {                                              // if the read was successful
                    printf("Error reading file %s\r\n", file);
                    system_config.error = true; // set the error flag
                    result = f_close(&file_handle); // close the file
                    return;
                }
                if(bytes_read != 128) { // check if we read exactly 128 bytes
                    printf("Error reading 128 bytes from file %d\r\n", i);
                    system_config.error = true; // set the error flag
                    result = f_close(&file_handle); // close the file
                    return;
                }
                if(ddr5_read_pages_128bytes(true, i, 1, buffer)){
                    result = f_close(&file_handle); // close the file
                    return;
                }
                for(uint8_t j=0; j<128; j++){
                    if(rbuffer[j] != buffer[j]) {
                        printf("Error: SPD NVM byte %d does not match file! (0x%02X != 0x%02X)\r\n", j+(i*128), rbuffer[j], buffer[j]);
                        verror = true; // set the verification error flag
                    }
                }
            }

            // close the file
            result = f_close(&file_handle); // close the file
            if (verror) {
                printf("Verification failed, SPD NVM does not match file %s\r\n", file);
            } else {
                printf("Verification successful, SPD NVM matches file %s\r\n", file);
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
            // crc in the file
            // read the file
            //get file size
            if(f_size(&file_handle)!=1024){ // get the file size
                printf("Error: File %s must be exactly 1024 bytes long\r\n", file);
                system_config.error = true; // set the error flag
                result = f_close(&file_handle); // close the file
                return;
            }
            result = f_read(&file_handle, buffer, 512, &bytes_read); // read the data from the file
            if (result != FR_OK) {                                              // if the read was successful
                printf("Error reading file %s\r\n", file);
                system_config.error = true; // set the error flag
                result = f_close(&file_handle); // close the file
                return;
            }
            
            // close the file
            result = f_close(&file_handle); // close the file
            if (result != FR_OK) {
                printf("Error closing file %s\r\n", file);
                system_config.error = true; // set the error flag
                return;
            }    

            if(bytes_read != 512) { // check if we read exactly 512 bytes
                printf("Error reading 512 byte JEDEC header\r\n", file);
                system_config.error = true; // set the error flag
                return;
            }

            ddr5_nvm_jedec_crc(buffer); // check the CRC of the first 512 bytes

            break;
        default:
            printf("Unknown action\r\n");
            return;
    }

#if 0
flash_cleanup:
    //we manually control any FALA capture
    fala_stop_hook();
    fala_notify_hook();
#endif

}

//maybe useful to evaluate before writing?
#if 0
    printf("Device Status - Write Protect Override: %s\r\n", (spd->device_status & 0x04) ? "Enabled" : "Disabled");
    if(!(spd->device_status & 0x04)) {
        printf("Error: Write Protect Override is disabled, ensure HSA pin is connected to ground, aborting...\r\n");
        return true;
    }
    printf("Error Status: 0x%02X\r\n", spd->error_status);
    printf("Program Abort Register: %s\r\n", spd->program_abort_register?"Error":"Normal");  


    printf("\r\nEEPROM SPD Data blocks 0-7:\r\n");
    
    if(ddr5_set_legacy_page(0)) return true; //set the page for the legacy mode
    
    data[0] = 0x80;
    if (pio_i2c_transaction_array_repeat_start(0xa0, data, 1u, data, 512, 0xffffu)) {
        printf("Device not detected (no ACK)\r\n");
        return true;
    }

    //print the first few SPD bytes as hex first, then decode
    printf("\r\nSPD bytes 0-3: 0x%02X, 0x%02X, 0x%02X, 0x%02X\r\n", data[0], data[1], data[2], data[3]);
    //printf("\r\nSPD NVM byte 2: 0x%02X\r\n", data[2]);
    if(data[2] == 0x12){
        printf("  Host Bus Type: DDR5 SDRAM\r\n");
    }else{
        printf("  Host Bus Type: Unknown (0x%02X), aborting...\r\n", data[2]);
        return true;
    }

    //printf("SPD NVM byte 0: 0x%02X\r\n", data[0]);   
    uint8_t eeprom_bytes_cnt = (data[0]>>4)&0b111;
    const uint16_t spd_eeprom_size[] = { 0, 256, 512, 1024, 2048};
    if(eeprom_bytes_cnt <= 0b100) {
        printf("  EEPROM size: %d bytes\r\n", spd_eeprom_size[eeprom_bytes_cnt]);
        if(eeprom_bytes_cnt !=3) {
            printf("  Warning: EEPROM size is not 1024 bytes!\r\n");
        }
    } else {
        printf("  EEPROM size: Unknown (0x%02X)\r\n", eeprom_bytes_cnt);
    }
#endif