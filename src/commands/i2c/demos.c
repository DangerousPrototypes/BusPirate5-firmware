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
#include "fatfs/ff.h"       // File system related
#include "pirate/storage.h" // File system related

static bool i2c_transaction(uint8_t addr, uint8_t *write_data, uint8_t write_len, uint8_t *read_data, uint16_t read_len) {
    if (pio_i2c_transaction_array_repeat_start(addr, write_data, write_len, read_data, read_len, 0xffffu)) {
        printf("\r\nDevice not detected (no ACK)\r\n");
        return true;
    }
    return false;
}

static bool i2c_write(uint8_t addr, uint8_t *data, uint16_t len) {
    hwi2c_status_t i2c_result = pio_i2c_write_array_timeout(addr, data, len, 0xfffffu);
    if(i2c_result != HWI2C_OK) {
        if(i2c_result == HWI2C_TIMEOUT) {
            printf("\r\nI2C Timeout\r\n");
        } else if(i2c_result == HWI2C_NACK) {
            printf("\r\nDevice not detected (no ACK)\r\n");
        } else {
            printf("\r\nI2C Error: %d\r\n", i2c_result);
        }
        return true;
    }
    return false;
}

static bool i2c_read(uint8_t addr, uint8_t *data, uint8_t len) {
    hwi2c_status_t i2c_result = pio_i2c_read_array_timeout(addr | 1u, data, len, 0xfffffu);
    if(i2c_result != HWI2C_OK) {
        if(i2c_result == HWI2C_TIMEOUT) {
            printf("\r\nI2C Timeout\r\n");
        } else if(i2c_result == HWI2C_NACK) {
            printf("\r\nDevice not detected (no ACK)\r\n");
        } else {
            printf("\r\nI2C Error: %d\r\n", i2c_result);
        }
        return true;
    }
    return false;
}

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

#define EEPROM_DEBUG 0
#define EEPROM_ADDRESS_PAGE_SIZE 256 // size of the EEPROM address page in bytes

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
    // user specified start address: align to 16 bytes, then adjust the number of bytes to read
    uint32_t start_address = eeprom->start_address & 0xFFFFFFF0; // align to 16 bytes
    uint32_t start_padding_bytes = eeprom->start_address - start_address; // calculate padding bytes
    // align user_bytes to 16 bytes, and adjust for padding
    uint32_t end_address = (eeprom->start_address + (eeprom->user_bytes-1)); // align to 16 bytes, remember -1 to account for the start byte (0)
    uint32_t total_read_bytes = (((end_address)&0xFFFFFFF0)+0x10)-eeprom->start_address;
    total_read_bytes = total_read_bytes + start_padding_bytes; // add the padding bytes to the total read bytes

    if(total_read_bytes > eeprom->device->size_bytes - start_address) {
        total_read_bytes = eeprom->device->size_bytes - start_address; // limit to device size
    }

    uint32_t end_address_aligned = start_address + total_read_bytes; // calculate the end address

    printf("Start address: %08X, end address: %08X\r\n\r\n", start_address, (start_address + total_read_bytes)-1);

    printf("          00 01 02 03 04 05 06 07 08 09 0A 0B 0C 0D 0E 0F\r\n");
    printf("---------------------------------------------------------\r\n");

    for(uint32_t i =start_address; i<end_address_aligned; i+=16) {
        // find the address for current byte and read 16 bytes at a time
        uint8_t i2caddr_7bit;
        uint8_t address_bytes[2];
        if(eeprom_get_address(eeprom, i, &i2caddr_7bit,address_bytes)) return true; // if there was an error getting the address
        if(i2c_transaction(i2caddr_7bit<<1, address_bytes, eeprom->device->address_bytes, buf, 16)) {
            return true; // error
        }
   
        // print the address
        printf("%08X: ", i);
        
        // print the data in hex
        for(uint32_t j = 0; j < 16; j++) {
            if(i+j < eeprom->device->size_bytes) {
                printf("%02X ", (uint8_t)buf[j]);
            } else {
                printf("   "); // print spaces for empty bytes
            }
        }
        printf(" ");
        
        // print the data as ASCII
        printf("|");
         // print the ASCII representation of the data
        for(uint32_t j = 0; j < 16; j++) {
            if(i+j < eeprom->device->size_bytes) {
                char c = (char)buf[j];
                if(c >= 32 && c <= 126) { // printable ASCII range
                    printf("%c", c);
                } else {
                    printf("."); // non-printable characters as dot
                }
            } else {
                printf(" "); // print space for empty bytes
            }
        }
        printf("|\r\n");
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
            printProgress(i, address_blocks_total);
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
        printProgress(address_blocks_total, address_blocks_total);
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
            printProgress(i, address_blocks_total);
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
        printProgress(address_blocks_total, address_blocks_total);
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