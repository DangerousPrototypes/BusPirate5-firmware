#include <stdio.h>
#include "pico/stdlib.h"
#include <stdint.h>
#include <string.h>
#include "pirate.h"
#include "pirate/hwi2c_pio.h"
#include "ui/ui_term.h"
//#include "system_config.h"
#include "command_struct.h"
//#include "bytecode.h"
//#include "mode/hwi2c.h"
#include "ui/ui_help.h"
#include "ui/ui_cmdln.h"
//#include "lib/ms5611/ms5611.h"
//#include "lib/tsl2561/driver_tsl2561.h"
#include "binmode/fala.h"
//#include "usb_rx.h"
//#include "pio_config.h"
#include "fatfs/ff.h"       // File system related
#include "pirate/storage.h" // File system related
#include "ui/ui_hex.h" // Hex display related
#include "ui/ui_progress_indicator.h" // Progress indicator related
#include "commands/i2c/eeprom.h"

struct eeprom_device_t {
    char name[9];
    uint32_t size_bytes;
    uint8_t address_bytes; 
    uint8_t block_select_bits;
    uint8_t block_select_offset;
    uint16_t page_bytes; 
};

const struct eeprom_device_t eeprom_devices[] = {
    { "24XM02", 262144, 2, 2, 0, 256 },
    { "24XM01", 131072, 2, 1, 0, 256 },    
    { "24X1026", 131072, 2, 1, 0, 128 },
    { "24X1025", 131072, 2, 1, 3, 128 },
    { "24X512",  65536,  2, 0, 0, 128 },
    { "24X256",  32768,  2, 0, 0, 64  },
    { "24X128",  16384,  2, 0, 0, 64  },
    { "24X64",   8192,   2, 0, 0, 32  },
    { "24X32",   4096,   2, 0, 0, 32  },
    { "24X16",   2048,   1, 3, 0, 16  },
    { "24X08",   1024,   1, 2, 0, 16  },
    { "24X04",   512,    1, 1, 0, 16  },
    { "24X02",   256,    1, 0, 0, 8   },
    { "24X01",   128,    1, 0, 0, 8   }
};


static bool file_close(FIL *file_handle) {
    FRESULT result = f_close(file_handle); // close the file
    if (result != FR_OK) {
        printf("\r\nError closing file\r\n");
        return true; // return true if there was an error
    }
    return false; // return false if the file was closed successfully
}

// true for error, false for success
static bool file_open(FIL *file_handle, const char *file, uint8_t file_status) {
    FRESULT result;
    result = f_open(file_handle, file, file_status); 
    if (result != FR_OK){
        printf("\r\nError opening file %s\r\n", file);
        file_close(file_handle); // close the file if there was an error
        return true;
    }
    return false; // return false if the file was opened successfully
}

static bool file_size_check(FIL *file_handle, uint32_t expected_size) {
    if(f_size(file_handle) != expected_size) { // get the file size
        printf("\r\nError: File must be exactly %d bytes long\r\n", expected_size);
        file_close(file_handle); // close the file
        return true; // return true if the file size is not as expected
    }
    return false; // return false if the file size is as expected
}

static bool file_read(FIL *file_handle, uint8_t *buffer, uint32_t size) {
    UINT bytes_read;
    FRESULT result = f_read(file_handle, buffer, size, &bytes_read); // read the file
    if (result != FR_OK || bytes_read != size) { // check if the read was successful
        printf("\r\nError reading file\r\n");
        file_close(file_handle); // close the file if there was an error
        return true; // return true if there was an error
    }
    return false; // return false if the read was successful
}

static bool file_write(FIL *file_handle, uint8_t *buffer, uint32_t size) {
    UINT bytes_written;
    FRESULT result = f_write(file_handle, buffer, size, &bytes_written); // write the file
    if (result != FR_OK || bytes_written != size) { // check if the write was successful
        printf("\r\nError writing to file\r\n");
        file_close(file_handle); // close the file if there was an error
        return true; // return true if there was an error
    }
    return false; // return false if the write was successful
}
/**************************************************** */
enum eeprom_actions_enum {
    EEPROM_DUMP=0,
    EEPROM_ERASE,
    EEPROM_WRITE,
    EEPROM_READ,
    EEPROM_VERIFY,
    EEPROM_TEST
};

struct eeprom_action_t {
    enum eeprom_actions_enum action;
    const char name[7];
};

const struct eeprom_action_t eeprom_actions[] = {
    { EEPROM_DUMP, "dump" },
    { EEPROM_ERASE, "erase" },
    { EEPROM_WRITE, "write" },
    { EEPROM_READ, "read" },
    { EEPROM_VERIFY, "verify" },
    { EEPROM_TEST, "test" }
};

struct eeprom_info{
    const struct eeprom_device_t* device;
    uint8_t device_address; // 7-bit address for the device
    int action;
    char file_name[13]; // file to read/write/verify
    bool verify_flag; // verify flag
    uint32_t start_address; // start address for read/write
    uint32_t user_bytes; // user specified number of bytes to read/write
    FIL file_handle;     // file handle
};

// custom I2C write function for EEPROM to get best speed
hwi2c_status_t eeprom_i2c_write(uint8_t i2c_addr, uint8_t *eeprom_addr, uint8_t eeprom_addr_len, uint8_t* txbuf, uint txbuf_len) {
    uint32_t timeout = 0xfffffu; // default timeout for I2C operations
    if(pio_i2c_start_timeout(timeout)) return HWI2C_TIMEOUT;
    hwi2c_status_t i2c_result = pio_i2c_write_timeout(i2c_addr, timeout);
    if(i2c_result != HWI2C_OK) return i2c_result;

    while (eeprom_addr_len) {
        --eeprom_addr_len;
        i2c_result = pio_i2c_write_timeout(*eeprom_addr++, timeout);
        if(i2c_result != HWI2C_OK) return i2c_result;
    }    

    while (txbuf_len) {
        --txbuf_len;
        i2c_result = pio_i2c_write_timeout(*txbuf++, timeout);
        if(i2c_result != HWI2C_OK) return i2c_result;
    }

    if (pio_i2c_stop_timeout(timeout)) return HWI2C_TIMEOUT;
    if (pio_i2c_wait_idle_extern(timeout)) return HWI2C_TIMEOUT;
    return HWI2C_OK;
}

bool eeprom_get_args(struct eeprom_info *args) {
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
    args->device = &eeprom_devices[eeprom_type];
    args->device_address = 0x50; // default I2C address for EEPROMs

    // verify_flag
    args->verify_flag = cmdln_args_find_flag('v' | 0x20);

    // file to read/write/verify
    bool file_flag = cmdln_args_find_flag_string('f' | 0x20, &arg, sizeof(args->file_name), args->file_name);
    if ((args->action == EEPROM_READ || args->action == EEPROM_WRITE || args->action==EEPROM_VERIFY) && !file_flag) {
        printf("Missing file name: -f <file name>\r\n");
        return true;
    }
    //bool override_flag = cmdln_args_find_flag('o' | 0x20);

    // start address
    if (cmdln_args_find_flag_uint32('s' | 0x20, &arg, &args->start_address)) {
        if (args->start_address >= args->device->size_bytes) {
            printf("Start address out of range: %d\r\n", args->start_address);
            return true; // error
        }
    } else {
        args->start_address = 0; // default to 0
    }

    // end address: user provides number of bytes to read/write, we calculate the end address
    if (cmdln_args_find_flag_uint32('b' | 0x20, &arg, &args->user_bytes)) {
        if(args->user_bytes == 0) {
            args->user_bytes = 1;
        }
    }else{
        args->user_bytes = args->device->size_bytes;
    }

    return false;
}

uint32_t eeprom_get_address_blocks_total(struct eeprom_info *eeprom) {
    //Each EEPROM 8 bit address covers 256 bytes
    uint32_t addr_range_256b= eeprom->device->size_bytes / EEPROM_ADDRESS_PAGE_SIZE;
    if(addr_range_256b ==0) addr_range_256b = 1; //for devices smaller than 256 bytes
    return addr_range_256b;
}

uint32_t eeprom_get_address_block_size(struct eeprom_info *eeprom) {
    uint32_t read_size = EEPROM_ADDRESS_PAGE_SIZE; // read 256 bytes at a time
    if(eeprom->device->size_bytes < EEPROM_ADDRESS_PAGE_SIZE){
        read_size = eeprom->device->size_bytes; // use the page size for reading        
    }
    return read_size;
}

uint32_t eeprom_get_address_block_start(struct eeprom_info *eeprom, uint32_t address_block) {
    // calculate the address for the 256 byte address range
    return ((address_block * EEPROM_ADDRESS_PAGE_SIZE) >> 8) & 0xFF; // high byte
}

//get the address with optional block select bits 
uint8_t eeprom_get_address_block_i2c_address(struct eeprom_info *eeprom, uint32_t address_block) {
    // if the device has block select bits, we need to adjust the address
    // if the device doen't have block select bits, then address_block is always 0 in the upper bits and this has no effect
    uint8_t block_select_bits = ((address_block*EEPROM_ADDRESS_PAGE_SIZE)>>(8*eeprom->device->address_bytes));
    // adjust to correct location in address (ie 1025)
    uint8_t i2caddr_7bit = eeprom->device_address | (block_select_bits << eeprom->device->block_select_offset);  
    return i2caddr_7bit;
} 

//function to return the block select, address given a byte address
bool eeprom_get_address(struct eeprom_info *eeprom, uint32_t address, uint8_t *i2caddr_7bit, uint8_t *address_bytes) {
    // check if the address is valid
    if (address >= eeprom->device->size_bytes) {
        printf("Error: Address out of range\r\n");
        return true; // invalid address
    }
    if(eeprom->device->address_bytes == 1) {
        address_bytes[0] = (uint8_t)(address & 0xFF); // 8-bit address
    }else if(eeprom->device->address_bytes == 2) {
        address_bytes[0] = (uint8_t)((address >> 8) & 0xFF); // high byte
        address_bytes[1] = (uint8_t)(address & 0xFF); // low byte
    } else {
        printf("Error: Invalid address bytes\r\n");
        return true; // invalid address bytes
    }
    // if the device has block select bits, we need to adjust the address
    // if the device doen't have block select bits, then address_block is always 0 in the upper bits and this has no effect
    uint8_t block_select_bits = ((address)>>(8*eeprom->device->address_bytes));
    // adjust to correct location in address (ie 1025)
    (*i2caddr_7bit) = eeprom->device_address | (block_select_bits << eeprom->device->block_select_offset); 
    return false; 
}

//function to display hex editor like dump of the EEPROM contents
bool eeprom_dump(struct eeprom_info *eeprom, char *buf, uint32_t buf_size){
    // align the start address to 16 bytes, and calculate the end address
    uint32_t start_address, end_address, total_read_bytes;
    ui_hex_align(eeprom->start_address, eeprom->user_bytes, eeprom->device->size_bytes, &start_address, &end_address, &total_read_bytes);

    // print the header
    ui_hex_header(start_address, end_address, total_read_bytes);

    for(uint32_t i =start_address; i<(end_address+1); i+=16) {
        // find the address for current byte and read 16 bytes at a time
        uint8_t i2caddr_7bit;
        uint8_t address_bytes[2];
        if(eeprom_get_address(eeprom, i, &i2caddr_7bit,address_bytes)) return true; // if there was an error getting the address
        if(i2c_transaction(i2caddr_7bit<<1, address_bytes, eeprom->device->address_bytes, buf, 16)) {
            return true; // error
        }

        // print the row
        ui_hex_row(i, buf, 16);
    }
}

bool eeprom_write(struct eeprom_info *eeprom, char *buf, uint32_t buf_size, bool write_from_buf) {
   
    //Each EEPROM 8 bit address covers 256 bytes
    uint32_t address_blocks_total=eeprom_get_address_blocks_total(eeprom);
    
    //reset pointers
    uint8_t block_ptr[2] = {0x00, 0x00}; // reset the block pointer

    if(!write_from_buf){
        if(file_open(&eeprom->file_handle, eeprom->file_name, FA_READ)) return true; // create the file, overwrite if it exists, close if fails
    }

    for(uint32_t i = 0; i < address_blocks_total; i++) {
        // 256 bytes at a time, less for smaller devices (128 bytes)
        uint32_t write_size = eeprom_get_address_block_size(eeprom); 
        // calculate the start address for the current 256 byte address page  
        block_ptr[0] = eeprom_get_address_block_start(eeprom, i);
        // set any block select bits in the I2C address
        uint8_t i2caddr_7bit = eeprom_get_address_block_i2c_address(eeprom, i);

        if(!write_from_buf){
            //read the next file chunk into the buffer
            if(file_read(&eeprom->file_handle, buf, write_size))return true;
        }

        // loop over the page size
        uint32_t write_pages = write_size / eeprom->device->page_bytes;

        #if !EEPROM_DEBUG
            print_progress(i, address_blocks_total);
        #else
            printf("Block %d, I2C address 0x%02X, address 0x%02X00, %d write pages, %d bytes\r\n", i, i2caddr_7bit, block_ptr[0], write_pages, eeprom->device->page_bytes); //debug
        #endif

        
        for(uint32_t j = 0; j < write_pages; j++) {
            // write page to the EEPROM
            #if !EEPROM_DEBUG
                // BUG BUG BUG: advance block pointer by page bytes
                block_ptr[0] = (j * eeprom->device->page_bytes);
                if (eeprom_i2c_write(i2caddr_7bit<<1, block_ptr, eeprom->device->address_bytes, &buf[j*eeprom->device->page_bytes], eeprom->device->page_bytes)) {
                    printf("Error writing EEPROM at %d\r\n", i*256 + j);
                    if(!write_from_buf) file_close(&eeprom->file_handle); // close the file if there was an error
                    return true; // error
                }
                //poll for write complete
                uint32_t timeout = 0xfffffu; // default timeout for I2C operations
                while(timeout) { // wait for the write to complete
                    hwi2c_status_t i2c_result = pio_i2c_write_array_timeout(i2caddr_7bit<<1, NULL, 0, 0xfffffu);
                    if(i2c_result==HWI2C_OK){
                        break; // if the transaction was successful, we can continue
                    }
                    timeout--; // decrement the timeout
                    if(!timeout) {
                        printf("\r\nError: EEPROM write timeout at %d\r\n", i*256 + j);
                        if(!write_from_buf) file_close(&eeprom->file_handle); // close the file if there was an error
                        return true; // error
                    }
                }

            #else
                printf("%d ", j);
            #endif
        }
        #if EEPROM_DEBUG
            printf("\r\n");
        #endif   
    }

    #if !EEPROM_DEBUG
        print_progress(address_blocks_total, address_blocks_total);
    #endif
    if(!write_from_buf){
        if(file_close(&eeprom->file_handle)) return true;
    }   
    return false; // success
}

bool eeprom_read(struct eeprom_info *eeprom, char *buf, uint32_t buf_size, char *verify_buf, uint32_t verify_buf_size, bool verify, bool verify_against_verify_buf){   
      
    //Each EEPROM 8 bit address covers 256 bytes
    uint32_t address_blocks_total=eeprom_get_address_blocks_total(eeprom);
    
    //reset pointers
    uint8_t block_ptr[2] = {0x00, 0x00}; // reset the block pointer

    if(!verify && !verify_against_verify_buf) { //read contents TO file
        if(file_open(&eeprom->file_handle, eeprom->file_name, FA_CREATE_ALWAYS | FA_WRITE)) return true; // create the file, overwrite if it exists
    }else if(verify && !verify_against_verify_buf){ //verify contents AGAINST file
        if(file_open(&eeprom->file_handle, eeprom->file_name, FA_READ)) return true; // open the file for reading
    }else if(verify_against_verify_buf){ //verify contents AGAINST 0xff
        if(buf_size < EEPROM_ADDRESS_PAGE_SIZE) {
            printf("Buffer size must be at least %d bytes for EEPROM read operation\r\n", EEPROM_ADDRESS_PAGE_SIZE);
            return true; // error
        }
    }else{
        printf("Invalid parameters for EEPROM read operation\r\n");
        return true; // error
    }

    for(uint32_t i = 0; i < address_blocks_total; i++) {
        // 256 bytes at a time, less for smaller devices (128 bytes)
        uint32_t read_size = eeprom_get_address_block_size(eeprom); 
        // calculate the address for the current 256 byte address range  
        block_ptr[0] = eeprom_get_address_block_start(eeprom, i);
        uint8_t i2caddr_7bit = eeprom_get_address_block_i2c_address(eeprom, i);
        #if !EEPROM_DEBUG
            print_progress(i, address_blocks_total);
        #else
            printf("Block %d, I2C address 0x%02X, address 0x%02X00, %d bytes\r\n", i, i2caddr_7bit, block_ptr[0], read_size); //debug
        #endif
    
        // read the page from the EEPROM
        #if !EEPROM_DEBUG
            if (i2c_transaction(eeprom->device_address<<1, block_ptr, eeprom->device->address_bytes, buf, EEPROM_ADDRESS_PAGE_SIZE)) {
                printf("Error reading EEPROM at %d\r\n", i*256);
                return true; // error
            }

            if(!verify) {
                // write the read data to the file
                if(file_write(&eeprom->file_handle, buf, read_size)) return true; // write the read data to the file
            } else if(verify){
                if(!verify_against_verify_buf) { //read more data if we are not verifying erase
                    if(file_read(&eeprom->file_handle, verify_buf, read_size)) return true; 
                }
                // compare the read data with the file data (or 0xff if verifying erase)
                for(uint32_t j = 0; j < read_size; j++) {
                    // verify against file data
                    if(buf[j] != verify_buf[j]) {
                        printf("\r\nError at 0x%08X: expected 0x%02X, read 0x%02X\r\n", i*256 + j, verify_buf[j], buf[j]);
                        if(!verify_against_verify_buf) if(!file_close(&eeprom->file_handle)) return true; // close the file if there was an error
                        return true; // error
                    }
                }
            }
        #endif

    }
    #if !EEPROM_DEBUG
        print_progress(address_blocks_total, address_blocks_total);
    #endif
    if(!verify_against_verify_buf) if(file_close(&eeprom->file_handle)) return true; // close the file after writing
    return false; // success
}

void eeprom_handler(struct command_result* res) {
    //help

    struct eeprom_info eeprom;
    if(eeprom_get_args(&eeprom)) {
        //ui_help_show(true, usage, count_of(usage), &options[0], count_of(options));
        return;
    }

    //nice full description of chip and capabilities
    printf("%s: %d bytes,  %d block select bits, %d byte address, %d byte pages\r\n\r\n", eeprom.device->name, eeprom.device->size_bytes, eeprom.device->block_select_bits, eeprom.device->address_bytes, eeprom.device->page_bytes);

    char buf[EEPROM_ADDRESS_PAGE_SIZE]; // buffer for reading/writing
    uint8_t verify_buf[EEPROM_ADDRESS_PAGE_SIZE]; // buffer for reading data from EEPROM

    //we manually control any FALA capture
    fala_start_hook(); 

    if(eeprom.action == EEPROM_DUMP) {
        //dump the EEPROM contents
        eeprom_dump(&eeprom, buf, sizeof(buf));
        goto eeprom_cleanup; // no need to continue
    }
 
    if (eeprom.action == EEPROM_ERASE || eeprom.action == EEPROM_TEST) {
        printf("Erase: Writing 0xFF to all bytes...\r\n");
        memset(buf, 0xFF, sizeof(buf)); // fill the buffer with 0xFF for erase
        if (eeprom_write(&eeprom, buf, sizeof(buf), true)) {
            goto eeprom_cleanup;
        }
        printf("\r\nErase complete\r\n");
        if (eeprom.verify_flag || eeprom.action == EEPROM_TEST) {
            printf("Erase verify...\r\n");
            memset(verify_buf, 0xFF, sizeof(verify_buf)); // fill the verify buffer with 0xFF for erase
            if (eeprom_read(&eeprom, buf, sizeof(buf), verify_buf, sizeof(verify_buf), true, true)) {
                goto eeprom_cleanup;
            }
            printf("\r\nErase verify complete\r\n");
        }
    }

    if (eeprom.action == EEPROM_TEST) {
        printf("\r\nTest: Writing alternating patterns\r\n");
        printf("Writing 0xAA 0x55...\r\n");
        //fill the buffer with 0xaa 0x55 for testing
        for(uint32_t i = 0; i < sizeof(verify_buf); i++) {
            if (i % 2 == 0) {
                verify_buf[i] = 0xAA; // even bytes
            } else {
                verify_buf[i] = 0x55; // odd bytes
            }
        }
        if (eeprom_write(&eeprom, verify_buf, sizeof(verify_buf), true)) {
            goto eeprom_cleanup;
        }
        printf("\r\nWrite complete\r\nWrite verify...\r\n");
        if (eeprom_read(&eeprom, buf, sizeof(buf), verify_buf, sizeof(verify_buf), true, true)) {
            goto eeprom_cleanup;
        }
        printf("\r\nWrite verify complete\r\nWriting 0x55 0xAA...\r\n");
        //fill the buffer with 0x55aa for testing
        for(uint32_t i = 0; i < sizeof(verify_buf); i++) {
            if (i % 2 == 0) {
                verify_buf[i] = 0x55; // even bytes
            } else {
                verify_buf[i] = 0xAA; // odd bytes
            }
        }
        if (eeprom_write(&eeprom, verify_buf, sizeof(verify_buf), true)) {
            goto eeprom_cleanup;
        }
        printf("\r\nWrite complete\r\nWrite verify...\r\n");
        if (eeprom_read(&eeprom, buf, sizeof(buf), verify_buf, sizeof(verify_buf), true, true)) {
            goto eeprom_cleanup;
        }        
        printf("\r\nWrite verify complete\r\n");
    }

    if (eeprom.action==EEPROM_WRITE) {
        printf("Write: Writing EEPROM from file %s...\r\n", eeprom.file_name);
        if (eeprom_write(&eeprom, buf, sizeof(buf), false)) {
            goto eeprom_cleanup;
        }
        printf("\r\nWrite complete\r\n");
        if (eeprom.verify_flag) {   
            printf("Write verify...\r\n");
            if(eeprom_read(&eeprom, buf, sizeof(buf), verify_buf, sizeof(verify_buf), true, false)){
                goto eeprom_cleanup;
            }
            printf("\r\nWrite verify complete\r\n");
        }
    }

    if (eeprom.action==EEPROM_READ) {
        printf("Read: Reading EEPROM to file %s...\r\n", eeprom.file_name);
        if(eeprom_read(&eeprom, buf, sizeof(buf), verify_buf, sizeof(verify_buf), false, false)){
            goto eeprom_cleanup;
        }
        printf("\r\nRead complete\r\n");
        if (eeprom.verify_flag) {
            printf("Read verify...\r\n");
            if(eeprom_read(&eeprom, buf, sizeof(buf), verify_buf, sizeof(verify_buf), true, false)){
                goto eeprom_cleanup;
            }
            printf("\r\nRead verify complete\r\n");
        }        
    }

    if (eeprom.action==EEPROM_VERIFY) {
        printf("Verify: Verifying EEPROM contents against file %s...\r\n", eeprom.file_name);
        if(eeprom_read(&eeprom, buf, sizeof(buf), verify_buf, sizeof(verify_buf), true, false)){
            goto eeprom_cleanup;
        }
        printf("\r\nVerify complete\r\n");
    }


eeprom_cleanup:
    //we manually control any FALA capture
    fala_stop_hook();
    fala_notify_hook();

}
