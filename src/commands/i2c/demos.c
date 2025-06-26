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

/**************************************************** */

struct eeprom_device_t {
    char name[9];
    uint32_t size_bytes;
    uint8_t address_bytes; 
    uint8_t block_select_bits;
    uint8_t block_select_offset;
    uint16_t page_bytes; 
};

const struct eeprom_device_t eeprom_devices[] = {
    { "24xM02", 262144, 2, 2, 0, 256 },
    { "24xM01", 131072, 2, 1, 0, 256 },    
    { "24x1026", 131072, 2, 1, 0, 128 },
    { "24x1025", 131072, 2, 1, 3, 128 },
    { "24x512",  65536,  2, 0, 0, 128 },
    { "24x256",  32768,  2, 0, 0, 64  },
    { "24x128",  16384,  2, 0, 0, 64  },
    { "24x64",   8192,   2, 0, 0, 32  },
    { "24x32",   4096,   2, 0, 0, 32  },
    { "24x16",   2048,   1, 3, 0, 16  },
    { "24x08",   1024,   1, 2, 0, 16  },
    { "24x04",   512,    1, 1, 0, 16  },
    { "24x02",   256,    1, 0, 0, 8   },
    { "24x01",   128,    1, 0, 0, 8   }
};

// a struct to hold the current EEPROM info and address, etc
struct eeprom_info {
    const struct eeprom_device_t* device;
    uint8_t device_address; // 7-bit address for the device
    uint8_t rw_ptr[2]; // 1 or 2 bytes depending on the device
    uint8_t block_select; // 0-15 for 24LC1025, 0-7 for others
    uint8_t page; // page address for writing
};


static void printProgress(size_t count, size_t max) {
    const int bar_width = 50;

    float progress = (float) count / max;
    int bar_length = progress * bar_width;

    printf("\rProgress: [");
    for (int i = 0; i < bar_length; ++i) {
        printf("#");
    }
    for (int i = bar_length; i < bar_width; ++i) {
        printf(" ");
    }
    printf("] %.2f%%", progress * 100);

    fflush(stdout);
}

// read the EEPROM and write to file
void eeprom_read_handler(struct command_result* res) {
    #define EEPROM_READ_SIZE 256
    
    //get eeprom type from command line
    command_var_t arg;
    char device_name[9];
    if(!cmdln_args_find_flag_string('d', &arg, sizeof(device_name), device_name)){
        printf("Missing EEPROM device name: -d <device name>\r\n");
        for(uint8_t i = 0; i < count_of(eeprom_devices); i++) {
            printf("  %s\r\n", eeprom_devices[i].name);
        }
        return;
    }

    // we have a device name, find it in the list
    uint8_t eeprom_type = 0xFF; // invalid by default
    strupr(device_name);
    for(uint8_t i = 0; i < count_of(eeprom_devices); i++) {
        if(strcmp(device_name, eeprom_devices[i].name) == 0) {
            eeprom_type = i; // found the device
            break;
        }
    }

    if(eeprom_type == 0xFF) {
        printf("Invalid EEPROM device name: %s\r\n", device_name);
        for(uint8_t i = 0; i < count_of(eeprom_devices); i++) {
            printf("  %s\r\n", eeprom_devices[i].name);
        }
        return; // error
    }
 
    static struct eeprom_info eeprom;
    eeprom.device = &eeprom_devices[eeprom_type];
    eeprom.device_address = 0x50; // default I2C address for EEPROMs

    //read in 256 bytes pages for convinience
    uint32_t addr_range_256b= eeprom.device->size_bytes / EEPROM_READ_SIZE;

    if(addr_range_256b ==0) addr_range_256b = 1;

    printf("Reading EEPROM %s (%d bytes) %d * 256 bytes\r\n", eeprom.device->name, eeprom.device->size_bytes, addr_range_256b);

    eeprom.rw_ptr[0] = eeprom.rw_ptr[1] = 0x00;

    // TODO: just pull high byte of address, lower byte is always 0x00;
    for(uint32_t i = 0; i < addr_range_256b; i++) {
       
        uint32_t read_size = EEPROM_READ_SIZE; // read 256 bytes at a time
        if(eeprom.device->size_bytes < EEPROM_READ_SIZE){
            read_size = eeprom.device->size_bytes; // use the page size for reading        
        }

        // calculate the address for the page
        uint32_t page_address = i * EEPROM_READ_SIZE;       
        //if(eeprom.device->address_bytes == 1) { //<256 bytes is a single read, not needed
            //eeprom.rw_ptr[0] = page_address & 0xFF; // low byte only
        //}else{
            eeprom.rw_ptr[0] = (page_address >> 8) & 0xFF; // high byte
            //eeprom.rw_ptr[1] = page_address & 0xFF; // low byte
        //}

        // if the device has block select bits, we need to adjust the address
        // if the device doen't have block select bits, then page_address is always 0 in the upper bits and this has no effect
        uint8_t block_select_bits = (page_address>>(8*eeprom.device->address_bytes));
        // adjust to correct location in address (ie 1025)
        uint8_t i2caddr_7bit = eeprom.device_address | (block_select_bits << eeprom.device->block_select_offset);        

        //printf("Reading EEPROM page %d at address 0x%04X (I2C address 0x%02X)\r\n", i, page_address, i2caddr_7bit);
        printProgress(i, addr_range_256b);
        #if 0
        // read the page from the EEPROM
        if (i2c_transaction(eeprom.device_address<<1, eeprom.rw_ptr, eeprom.device->address_bytes, buffer, EEPROM_READ_SIZE)) {
            printf("Error reading EEPROM at %d\r\n", i*256);
            return true; // error
        }
        #endif
    }

    printProgress(addr_range_256b, addr_range_256b);
    printf("\r\nRead complete\r\n");

}


// write the EEPROM from a file
// 8, 16, 32, 64, 128 byte pages are possible
void eeprom_handler(struct command_result* res) {
    #define EEPROM_READ_SIZE 256
    
    //get eeprom type from command line
    command_var_t arg;
    char device_name[9];
    if(!cmdln_args_find_flag_string('d', &arg, sizeof(device_name), device_name)){
        printf("Missing EEPROM device name: -d <device name>\r\n");
        for(uint8_t i = 0; i < count_of(eeprom_devices); i++) {
            printf("  %s\r\n", eeprom_devices[i].name);
        }
        return;
    }

    // we have a device name, find it in the list
    //TODO: case insensitive
    uint8_t eeprom_type = 0xFF; // invalid by default
    strupr(device_name);
    for(uint8_t i = 0; i < count_of(eeprom_devices); i++) {
        if(strcmp(device_name, eeprom_devices[i].name) == 0) {
            eeprom_type = i; // found the device
            break;
        }
    }

    if(eeprom_type == 0xFF) {
        printf("Invalid EEPROM device name: %s\r\n", device_name);
        for(uint8_t i = 0; i < count_of(eeprom_devices); i++) {
            printf("  %s\r\n", eeprom_devices[i].name);
        }
        return; // error
    }
 
    static struct eeprom_info eeprom;
    eeprom.device = &eeprom_devices[eeprom_type];
    eeprom.device_address = 0x50; // default I2C address for EEPROMs

    //read in 256 bytes pages for convinience
    uint32_t addr_range_256b= eeprom.device->size_bytes / EEPROM_READ_SIZE;

    if(addr_range_256b ==0) addr_range_256b = 1;

    //nice fill description of chip and capabilities
    printf("%s: %d bytes,  %d block select bits, %d byte address, %d byte pages\r\n", eeprom.device->name, eeprom.device->size_bytes, eeprom.device->block_select_bits, eeprom.device->address_bytes, eeprom.device->page_bytes);

    eeprom.rw_ptr[0] = eeprom.rw_ptr[1] = 0x00;

    // TODO: just pull high byte of address, lower byte is always 0x00;
    for(uint32_t i = 0; i < addr_range_256b; i++) {
       
        uint32_t read_size = EEPROM_READ_SIZE; // read 256 bytes at a time
        if(eeprom.device->size_bytes < EEPROM_READ_SIZE){
            read_size = eeprom.device->size_bytes; // use the page size for reading        
        }

        // calculate the address for the page
        uint32_t page_address = i * EEPROM_READ_SIZE;       
        //if(eeprom.device->address_bytes == 1) { //<256 bytes is a single read, not needed
            //eeprom.rw_ptr[0] = page_address & 0xFF; // low byte only
        //}else{
            eeprom.rw_ptr[0] = (page_address >> 8) & 0xFF; // high byte
            //eeprom.rw_ptr[1] = page_address & 0xFF; // low byte
        //}

        // if the device has block select bits, we need to adjust the address
        // if the device doen't have block select bits, then page_address is always 0 in the upper bits and this has no effect
        uint8_t block_select_bits = (page_address>>(8*eeprom.device->address_bytes));
        // adjust to correct location in address (ie 1025)
        uint8_t i2caddr_7bit = eeprom.device_address | (block_select_bits << eeprom.device->block_select_offset);        

        //printf("Reading EEPROM page %d at address 0x%04X (I2C address 0x%02X)\r\n", i, page_address, i2caddr_7bit);
        printProgress(i, addr_range_256b);

        // loop over the page size
        uint32_t write_pages = read_size / eeprom.device->page_bytes;
        uint8_t buffer[128];
        memset(buffer, 0x88, sizeof(buffer)); // clear the buffer

        //printf("Writing EEPROM page %d at address 0x%04X, %d write pages (I2C address 0x%02X):", i, page_address, write_pages, i2caddr_7bit);

        for(uint32_t j = 0; j < write_pages; j++) {
            // read the page from the EEPROM
            /*if (i2c_transaction(i2caddr_7bit, eeprom.rw_ptr, eeprom.device->address_bytes, buffer, eeprom.device->page_bytes)) {
                printf("Error reading EEPROM at %d\r\n", i*256 + j);
                return; // error
            }*/
            //printf(" %d", j);
        }
        //printf("\r\n");
        #if 0
        // read the page from the EEPROM
        if (i2c_transaction(eeprom.device_address<<1, eeprom.rw_ptr, eeprom.device->address_bytes, buffer, EEPROM_READ_SIZE)) {
            printf("Error reading EEPROM at %d\r\n", i*256);
            return true; // error
        }
        #endif
    }

    printProgress(addr_range_256b, addr_range_256b);
    printf("\r\nWrite complete\r\n");

}


enum eeprom_actions_enum {
    FLASH_DUMP=0,
    FLASH_ERASE,
    FLASH_WRITE,
    FLASH_READ,
    FLASH_VERIFY,
    FLASH_TEST
};

struct eeprom_action_t {
    enum eeprom_actions action;
    const char name[7];
};

const struct eeprom_action_t eeprom_actions[] = {
    { FLASH_DUMP, "dump" },
    { FLASH_ERASE, "erase" },
    { FLASH_WRITE, "write" },
    { FLASH_READ, "read" },
    { FLASH_VERIFY, "verify" },
    { FLASH_TEST, "test" }
};

struct eeprom_args{
    int action;
    struct eeprom_info device;
    uint8_t device_address; // 7-bit address for the device
    char file[13]; // file to read/write/verify
    bool verify_flag; // verify flag
    uint32_t start_address; // start address for read/write
    uint32_t end_address; // end address for read/write
}

bool eeprom_get_args(struct eeprom_args *args) {
    command_var_t arg;
    char arg_str[9];
    
    args->action = -1; // invalid by default
    // action is the first argument (read/write/probe/erase/etc)
    if (cmdln_args_string_by_position(1, sizeof(arg_str), arg_str)) {
        // get action from struct
        for (uint8_t i = 0; i < count_of(eeprom_actions); i++) {
            if (strcmp(arg_str, eeprom_actions[i].name) == 0) {
                args->action = eeprom_actions[i].action;
                break;
            }
        }
    }
    if (args->action == -1) {
        printf("Invalid action: %s\r\n\r\n", arg_str);
        return true; // invalid action
    }
    
    if(!cmdln_args_find_flag_string('d', &arg, sizeof(arg_str), arg_str)){
        printf("Missing EEPROM device name: -d <device name>\r\n");
        for(uint8_t i = 0; i < count_of(eeprom_devices); i++) {
            printf("  %s\r\n", eeprom_devices[i].name);
        }
        return true;
    }

    // we have a device name, find it in the list
    uint8_t eeprom_type = 0xFF; // invalid by default
    strupr(arg_str);
    for(uint8_t i = 0; i < count_of(eeprom_devices); i++) {
        if(strcmp(arg_str, eeprom_devices[i].name) == 0) {
            eeprom_type = i; // found the device
            break;
        }
    }

    if(eeprom_type == 0xFF) {
        printf("Invalid EEPROM device name: %s\r\n", arg_str);
        for(uint8_t i = 0; i < count_of(eeprom_devices); i++) {
            printf("  %s\r\n", eeprom_devices[i].name);
        }
        return true; // error
    }
 
    //static struct eeprom_info eeprom;
    //eeprom.device = &eeprom_devices[eeprom_type];
    args->device.device = &eeprom_devices[eeprom_type];
    args->device_address = 0x50; // default I2C address for EEPROMs

    // verify_flag
    args->verify_flag = cmdln_args_find_flag('v' | 0x20);

    // file to read/write/verify
    bool file_flag = cmdln_args_find_flag_string('f' | 0x20, &arg, sizeof(args->file), args->file);
    if ((args->action == FLASH_READ || args->action == FLASH_WRITE || args->action==FLASH_VERIFY) && !file_flag) {
        printf("Missing file name: -f <file name>\r\n");
        return true;
    }
    //bool override_flag = cmdln_args_find_flag('o' | 0x20);
    return false;
}

bool eeprom_write(struct eeprom_args *args){
    struct eeprom_info *eeprom;
    eeprom = &args->device;
    
    //Each EEPROM 8 bit address covers 256 bytes
    uint32_t addr_range_256b= eeprom->device->size_bytes / EEPROM_READ_SIZE;
    if(addr_range_256b ==0) addr_range_256b = 1; //for devices smaller than 256 bytes

    //nice full description of chip and capabilities
    printf("%s: %d bytes,  %d block select bits, %d byte address, %d byte pages\r\n", eeprom->device->name, eeprom->device->size_bytes, eeprom->device->block_select_bits, eeprom->device->address_bytes, eeprom->device->page_bytes);

    eeprom->rw_ptr[0] = eeprom->rw_ptr[1] = 0x00;

    for(uint32_t i = 0; i < addr_range_256b; i++) {
       
        uint32_t read_size = EEPROM_READ_SIZE; // read 256 bytes at a time
        if(eeprom->device->size_bytes < EEPROM_READ_SIZE){
            read_size = eeprom->device->size_bytes; // use the page size for reading        
        }

        // calculate the address for the 256 byte address range
        uint32_t page_address = i * EEPROM_READ_SIZE;       
        eeprom->rw_ptr[0] = (page_address >> 8) & 0xFF; // high byte

        // if the device has block select bits, we need to adjust the address
        // if the device doen't have block select bits, then page_address is always 0 in the upper bits and this has no effect
        uint8_t block_select_bits = (page_address>>(8*eeprom->device->address_bytes));
        // adjust to correct location in address (ie 1025)
        uint8_t i2caddr_7bit = eeprom->device_address | (block_select_bits << eeprom->device->block_select_offset);        

        //printf("Reading EEPROM page %d at address 0x%04X (I2C address 0x%02X)\r\n", i, page_address, i2caddr_7bit);
        printProgress(i, addr_range_256b);

        // loop over the page size
        uint32_t write_pages = read_size / eeprom->device->page_bytes;
        uint8_t buffer[256];
        memset(buffer, 0x88, sizeof(buffer)); // clear the buffer

        //printf("Writing EEPROM page %d at address 0x%04X, %d write pages (I2C address 0x%02X):", i, page_address, write_pages, i2caddr_7bit);

        for(uint32_t j = 0; j < write_pages; j++) {
            // write page to the EEPROM
            /*if (i2c_transaction(i2caddr_7bit, eeprom->rw_ptr, eeprom->device->address_bytes, buffer, eeprom->device->page_bytes)) {
                printf("Error reading EEPROM at %d\r\n", i*256 + j);
                return; // error
            }*/
            //printf(" %d", j);
        }
        //printf("\r\n");
        #if 0
        // read the page from the EEPROM
        if (i2c_transaction(eeprom->device_address<<1, eeprom->rw_ptr, eeprom->device->address_bytes, buffer, EEPROM_READ_SIZE)) {
            printf("Error reading EEPROM at %d\r\n", i*256);
            return true; // error
        }
        #endif
    }

    printProgress(addr_range_256b, addr_range_256b);
    printf("\r\nWrite complete\r\n");
}


void eeprom_handler_high_level(struct command_result* res) {
    //help

    struct eeprom_args args;
    if(eeprom_get_args(&args)) {
        //ui_help_show(true, usage, count_of(usage), &options[0], count_of(options));
        return;
    }

    //we manually control any FALA capture
    fala_start_hook();    
    if (args.action == FLASH_ERASE || args.action == FLASH_TEST) {
        if (!spiflash_erase(&flash_info)) {
            goto flash_cleanup;
        }
        if (args.verify_flag || args.action == FLASH_TEST) {
            if (!spiflash_erase_verify(start_address, end_address, sizeof(data), data, &flash_info)) {
                goto flash_cleanup;
            }
        }
    }

    if (args.action == FLASH_TEST) {
        if (!spiflash_write_test(start_address, end_address, sizeof(data), data, &flash_info)) {
            goto flash_cleanup;
        }
        if (!spiflash_write_verify(start_address, end_address, sizeof(data), data, &flash_info)) {
            goto flash_cleanup;
        }
    }

    if (args.action==FLASH_WRITE) {
        if (!spiflash_load(start_address, end_address, sizeof(data), data, &flash_info, file)) {
            goto flash_cleanup;
        }
        if (args.verify_flag) {
            if (!spiflash_verify(start_address, end_address, sizeof(data), data, data2, &flash_info, file)) {
                goto flash_cleanup;
            }
        }
    }

    if (args.action==FLASH_READ) {
        if (!spiflash_dump(start_address, end_address, sizeof(data), data, &flash_info, file)) {
            goto flash_cleanup;
        }
    }

    if (args.action==FLASH_VERIFY) {
        if (!spiflash_verify(start_address, end_address, sizeof(data), data, data2, &flash_info, file)) {
            goto flash_cleanup;
        }
    }

flash_cleanup:
    //we manually control any FALA capture
    fala_stop_hook();
    fala_notify_hook();

}


// write the EEPROM from a file
// 8, 16, 32, 64, 128 byte pages are possible
void eeprom_handler_high_level(struct command_result* res) {
    #define EEPROM_READ_SIZE 256
    
    //get eeprom type from command line
    command_var_t arg;
    char device_name[9];
    if(!cmdln_args_find_flag_string('d', &arg, sizeof(device_name), device_name)){
        printf("Missing EEPROM device name: -d <device name>\r\n");
        for(uint8_t i = 0; i < count_of(eeprom_devices); i++) {
            printf("  %s\r\n", eeprom_devices[i].name);
        }
        return;
    }

    // we have a device name, find it in the list
    //TODO: case insensitive
    uint8_t eeprom_type = 0xFF; // invalid by default
    strupr(device_name);
    for(uint8_t i = 0; i < count_of(eeprom_devices); i++) {
        if(strcmp(device_name, eeprom_devices[i].name) == 0) {
            eeprom_type = i; // found the device
            break;
        }
    }

    if(eeprom_type == 0xFF) {
        printf("Invalid EEPROM device name: %s\r\n", device_name);
        for(uint8_t i = 0; i < count_of(eeprom_devices); i++) {
            printf("  %s\r\n", eeprom_devices[i].name);
        }
        return; // error
    }
 
    static struct eeprom_info eeprom;
    eeprom.device = &eeprom_devices[eeprom_type];
    eeprom.device_address = 0x50; // default I2C address for EEPROMs

    //read in 256 bytes pages for convinience
    uint32_t addr_range_256b= eeprom.device->size_bytes / EEPROM_READ_SIZE;

    if(addr_range_256b ==0) addr_range_256b = 1;

    //nice fill description of chip and capabilities
    printf("%s: %d bytes,  %d block select bits, %d byte address, %d byte pages\r\n", eeprom.device->name, eeprom.device->size_bytes, eeprom.device->block_select_bits, eeprom.device->address_bytes, eeprom.device->page_bytes);

    eeprom.rw_ptr[0] = eeprom.rw_ptr[1] = 0x00;

    // TODO: just pull high byte of address, lower byte is always 0x00;
    for(uint32_t i = 0; i < addr_range_256b; i++) {
       
        uint32_t read_size = EEPROM_READ_SIZE; // read 256 bytes at a time
        if(eeprom.device->size_bytes < EEPROM_READ_SIZE){
            read_size = eeprom.device->size_bytes; // use the page size for reading        
        }

        // calculate the address for the page
        uint32_t page_address = i * EEPROM_READ_SIZE;       
        //if(eeprom.device->address_bytes == 1) { //<256 bytes is a single read, not needed
            //eeprom.rw_ptr[0] = page_address & 0xFF; // low byte only
        //}else{
            eeprom.rw_ptr[0] = (page_address >> 8) & 0xFF; // high byte
            //eeprom.rw_ptr[1] = page_address & 0xFF; // low byte
        //}

        // if the device has block select bits, we need to adjust the address
        // if the device doen't have block select bits, then page_address is always 0 in the upper bits and this has no effect
        uint8_t block_select_bits = (page_address>>(8*eeprom.device->address_bytes));
        // adjust to correct location in address (ie 1025)
        uint8_t i2caddr_7bit = eeprom.device_address | (block_select_bits << eeprom.device->block_select_offset);        

        //printf("Reading EEPROM page %d at address 0x%04X (I2C address 0x%02X)\r\n", i, page_address, i2caddr_7bit);
        printProgress(i, addr_range_256b);

        // loop over the page size
        uint32_t write_pages = read_size / eeprom.device->page_bytes;
        uint8_t buffer[128];
        memset(buffer, 0x88, sizeof(buffer)); // clear the buffer

        //printf("Writing EEPROM page %d at address 0x%04X, %d write pages (I2C address 0x%02X):", i, page_address, write_pages, i2caddr_7bit);

        for(uint32_t j = 0; j < write_pages; j++) {
            // read the page from the EEPROM
            /*if (i2c_transaction(i2caddr_7bit, eeprom.rw_ptr, eeprom.device->address_bytes, buffer, eeprom.device->page_bytes)) {
                printf("Error reading EEPROM at %d\r\n", i*256 + j);
                return; // error
            }*/
            //printf(" %d", j);
        }
        //printf("\r\n");
        #if 0
        // read the page from the EEPROM
        if (i2c_transaction(eeprom.device_address<<1, eeprom.rw_ptr, eeprom.device->address_bytes, buffer, EEPROM_READ_SIZE)) {
            printf("Error reading EEPROM at %d\r\n", i*256);
            return true; // error
        }
        #endif
    }

    printProgress(addr_range_256b, addr_range_256b);
    printf("\r\nWrite complete\r\n");

}














/**************************************************** */

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