#include <stdio.h>
#include "pico/stdlib.h"
#include <stdint.h>
#include <string.h>
#include "pirate.h"
//#include "ui/ui_term.h"
#include "command_struct.h"
#include "ui/ui_help.h"
#include "ui/ui_cmdln.h" //for cmdln_action_t
//#include "binmode/fala.h"
#include "fatfs/ff.h"       // File system related
#include "ui/ui_hex.h" // Hex display related
#include "ui/ui_progress_indicator.h" // Progress indicator related
//#include "commands/i2c/eeprom.h"
#include "pirate/file.h" // File handling related
//#include "pirate/hwspi.h" // SPI related functions
#include "eeprom_base.h"

void eeprom_display_devices(const struct eeprom_device_t *eeprom_devices, uint8_t count) {
    printf("\r\nAvailable EEPROM devices:\r\n");
    printf("Device\t|Bytes\t|Page Size\t|Addr Bytes\t|Blk Sel Bits\t|kHz max\r\n");
    for(uint8_t i = 0; i < count; i++) {
        // Print device information
        printf("%s%s|%d\t|%d\t\t|%d\t\t|%d\t\t|%d\r\n",
                eeprom_devices[i].name,
                strlen(eeprom_devices[i].name)>7?"\t\t":"\t",
                eeprom_devices[i].size_bytes,
                eeprom_devices[i].page_bytes,
                eeprom_devices[i].address_bytes,
                eeprom_devices[i].block_select_bits,
                eeprom_devices[i].max_speed_khz
            );
    }
    printf("\r\n");
}

uint32_t eeprom_get_address_blocks_total(struct eeprom_info *eeprom) {
    //Each EEPROM 8 bit address covers 256 bytes
    uint32_t addr_range_256b= eeprom->device->size_bytes / EEPROM_ADDRESS_PAGE_SIZE;
    if(addr_range_256b ==0) addr_range_256b = 1; //for devices smaller than 256 bytes
    return addr_range_256b;
}

//get the size of the address block in bytes
uint32_t eeprom_get_address_block_size(struct eeprom_info *eeprom) {
    uint32_t read_size = EEPROM_ADDRESS_PAGE_SIZE; // read 256 bytes at a time
    if(eeprom->device->size_bytes < EEPROM_ADDRESS_PAGE_SIZE){
        read_size = eeprom->device->size_bytes; // use the page size for reading        
    }
    return read_size;
}

//function to return the block select, address given a byte address
bool eeprom_get_address(struct eeprom_info *eeprom, uint32_t address, uint8_t *block_select_bits, uint8_t *address_array) {
    // check if the address is valid
    if (address >= eeprom->device->size_bytes) {
        printf("Error: Address out of range\r\n");
        return true; // invalid address
    }
    if(eeprom->device->address_bytes == 1) {
        address_array[0] = (uint8_t)(address & 0xFF); // 8-bit address
    }else if(eeprom->device->address_bytes == 2) {
        address_array[0] = (uint8_t)((address >> 8) & 0xFF); // high byte
        address_array[1] = (uint8_t)(address & 0xFF); // low byte
    }else if (eeprom->device->address_bytes == 3) {
        address_array[0] = (uint8_t)((address >> 16) & 0xFF); // high byte
        address_array[1] = (uint8_t)((address >> 8) & 0xFF); // middle byte
        address_array[2] = (uint8_t)(address & 0xFF); // low byte
    } else {
        printf("Error: Invalid address bytes\r\n");
        return true; // invalid address bytes
    }
    // if the device has block select bits, we need to adjust the address
    // if the device doesn't have block select bits, then address_block is always 0 in the upper bits and this has no effect
    (*block_select_bits) = ((address)>>(8*eeprom->device->address_bytes));
    // adjust to correct location in address (ie 1025)
    (*block_select_bits) = ((*block_select_bits) << eeprom->device->block_select_offset); 
    //printf("Address: 0x%06X, Block Select Bits: 0x%02X\r\n", address, (*block_select_bits));
    return false; 
}


//-----------------Universal Functions-------------------------//

//function to display hex editor like dump of the EEPROM contents
bool eeprom_dump(struct eeprom_info *eeprom, uint8_t *buf, uint32_t buf_size){
    // align the start address to 16 bytes, and calculate the end address
    struct hex_config_t hex_config;
    hex_config.max_size_bytes= eeprom->device->size_bytes; // maximum size of the device in bytes
    ui_hex_get_args_config(&hex_config);
    ui_hex_align_config(&hex_config);
    ui_hex_header_config(&hex_config);

    for(uint32_t i=hex_config._aligned_start; i<(hex_config._aligned_end+1); i+=16) {
        eeprom->hal->read(eeprom, i, 16, buf); // read 16 bytes from the EEPROM
        ui_hex_row_config(&hex_config, i, buf, 16);
    }
}



bool eeprom_write(struct eeprom_info *eeprom, uint8_t *buf, uint32_t buf_size, bool write_from_buf) {

    uint32_t file_size_bytes; 
    if(!write_from_buf){
        // open the file to write, close with message if error
        if(file_open(&eeprom->file_handle, eeprom->file_name, FA_READ)) return true; 
        file_size_bytes = file_size(&eeprom->file_handle);
        if(file_size_bytes < eeprom->device->size_bytes) {
            printf("Warning: File smaller than EEPROM: writing the first %d bytes\r\n", file_size_bytes);
        }else if(file_size_bytes > eeprom->device->size_bytes) {
            printf("Warning: EEPROM is smaller than file: writing the first %d bytes\r\n", eeprom->device->size_bytes);
        }
    }

    //Each EEPROM 8 bit address covers 256 bytes
    uint32_t address_blocks_total=eeprom_get_address_blocks_total(eeprom);
    // 256 bytes at a time, less for smaller devices (128 bytes)
    uint32_t write_size = eeprom_get_address_block_size(eeprom); 
    uint32_t write_pages = write_size / eeprom->device->page_bytes; 

    for(uint32_t i = 0; i < address_blocks_total; i++) {
        uint32_t bytes_read=write_size;
        uint32_t page_write_size = eeprom->device->page_bytes; // default page write size is the page size
        
        if(!write_from_buf){
            //read the next file chunk into the buffer, close with message if error
            if(file_read2(&eeprom->file_handle, buf, write_size, &bytes_read))return true;
            if(bytes_read == 0){ //end of file
                //file_close(&eeprom->file_handle);
                //return false;
                goto eeprom_base_write_cleanup; // if we are at the end of the file, break out of the loop
            }
        }

        #if !EEPROM_DEBUG
            print_progress(i, address_blocks_total);
        #else
            printf("Block %d, I2C address 0x%02X, address 0x%02X00, %d write pages, %d bytes\r\n", i, i2caddr_7bit, block_ptr[0], write_pages, eeprom->device->page_bytes); //debug
        #endif
        
        for(uint32_t j = 0; j < write_pages; j++) {
            // write page to the EEPROM
            #if !EEPROM_DEBUG
                //TODO: add a write bytes amount, write fewer if at end of read
                if(!write_from_buf){
                    if(bytes_read < eeprom->device->page_bytes) {
                        page_write_size = bytes_read; // if we are at the end of the file, write only the remaining bytes
                    }
                }
                //TODO: pass page size to the write function
                if(eeprom->hal->write_page(eeprom, (i*256)+(j*eeprom->device->page_bytes), &buf[j*eeprom->device->page_bytes], page_write_size)) {
                    printf("Error writing EEPROM at %d\r\n", (i*256) + j);
                    if(!write_from_buf) file_close(&eeprom->file_handle); // close the file if there was an error
                    return true; // error
                }
            #else
                printf("%d ", j);
            #endif
            if(!write_from_buf){
                bytes_read -= page_write_size; // reduce the bytes read by the page size
                if(bytes_read == 0) {
                    //ile_close(&eeprom->file_handle);
                    goto eeprom_base_write_cleanup; // if we are at the end of the file, break out of the loop
                }
            }
        }
        #if EEPROM_DEBUG
            printf("\r\n");
        #endif   
    }
eeprom_base_write_cleanup:
    #if !EEPROM_DEBUG
        print_progress(address_blocks_total, address_blocks_total);
    #endif
    if(!write_from_buf){
        if(file_close(&eeprom->file_handle)) return true;
    }   
    return false; // success
}
#if 0

bool eeprom_write(struct eeprom_info *eeprom, uint8_t *buf, uint32_t buf_size, bool write_from_buf) {

    if(!write_from_buf){
        // open the file to write, close with message if error
        if(file_open(&eeprom->file_handle, eeprom->file_name, FA_READ)) return true; 
    }

    //Each EEPROM 8 bit address covers 256 bytes
    uint32_t address_blocks_total=eeprom_get_address_blocks_total(eeprom);
    // 256 bytes at a time, less for smaller devices (128 bytes)
    uint32_t write_size = eeprom_get_address_block_size(eeprom); 
    uint32_t write_pages = write_size / eeprom->device->page_bytes; 

    for(uint32_t i = 0; i < address_blocks_total; i++) {

        if(!write_from_buf){
            //read the next file chunk into the buffer, close with message if error
            if(file_read(&eeprom->file_handle, buf, write_size))return true;
        }

        #if !EEPROM_DEBUG
            print_progress(i, address_blocks_total);
        #else
            printf("Block %d, I2C address 0x%02X, address 0x%02X00, %d write pages, %d bytes\r\n", i, i2caddr_7bit, block_ptr[0], write_pages, eeprom->device->page_bytes); //debug
        #endif
        
        for(uint32_t j = 0; j < write_pages; j++) {
            // write page to the EEPROM
            #if !EEPROM_DEBUG
                if(eeprom->hal->write_page(eeprom, (i*256)+(j*eeprom->device->page_bytes), &buf[j*eeprom->device->page_bytes])) {
                    printf("Error writing EEPROM at %d\r\n", (i*256) + j);
                    if(!write_from_buf) file_close(&eeprom->file_handle); // close the file if there was an error
                    return true; // error
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
#endif
bool eeprom_read(struct eeprom_info *eeprom, char *buf, uint32_t buf_size, char *verify_buf, uint32_t verify_buf_size, enum eeprom_read_action action) {   
    uint32_t file_size_bytes;
    // figure out what we are doing
    switch(action){
        case EEPROM_READ_TO_FILE: // read contents TO file
            if(file_open(&eeprom->file_handle, eeprom->file_name, FA_CREATE_ALWAYS | FA_WRITE)) return true; // create the file, overwrite if it exists
            break;
        case EEPROM_VERIFY_FILE: // verify contents AGAINST file
            if(file_open(&eeprom->file_handle, eeprom->file_name, FA_READ)) return true; // open the file for reading
            file_size_bytes = file_size(&eeprom->file_handle);
            if(file_size_bytes < eeprom->device->size_bytes) {
                printf("Warning: File smaller than EEPROM: verifying the first %d bytes\r\n", file_size_bytes);
            }else if(file_size_bytes > eeprom->device->size_bytes) {
                printf("Warning: EEPROM is smaller than file: verifying the first %d bytes\r\n", eeprom->device->size_bytes);
            }            
            break;
        case EEPROM_VERIFY_BUFFER: // verify contents AGAINST buffer
            if(buf_size < EEPROM_ADDRESS_PAGE_SIZE) {
                printf("Buffer size must be at least %d bytes for EEPROM read operation\r\n", EEPROM_ADDRESS_PAGE_SIZE);
                return true; // error
            }
            break;
        default:
            printf("Invalid action for EEPROM read operation\r\n");
            return true; // error
    }
    
    //Each EEPROM 8 bit address covers 256 bytes
    uint32_t address_blocks_total=eeprom_get_address_blocks_total(eeprom);
    // 256 bytes at a time, less for smaller devices (128 bytes)
    uint32_t read_size = eeprom_get_address_block_size(eeprom);     

    for(uint32_t i = 0; i < address_blocks_total; i++) {
        #if !EEPROM_DEBUG
            print_progress(i, address_blocks_total);
        #else
            printf("Block %d, I2C address 0x%02X, address 0x%02X00, %d bytes\r\n", i, i2caddr_7bit, block_ptr[0], read_size); //debug
        #endif
    
        #if !EEPROM_DEBUG
            // read the page from the EEPROM
            if(eeprom->hal->read(eeprom, i*256, read_size, buf)) {
                printf("Error reading EEPROM at %d\r\n", i*256);
                if(action != EEPROM_VERIFY_BUFFER) {
                    file_close(&eeprom->file_handle); // close the file if there was an error
                }
                return true; // error
            }

            if(action==EEPROM_READ_TO_FILE) {
                // write the read data to the file
                if(file_write(&eeprom->file_handle, buf, read_size)) return true; // write the read data to the file
            } else {
                // read more data to verify against
                if(action==EEPROM_VERIFY_FILE) { 
                    if(file_read(&eeprom->file_handle, verify_buf, read_size)) return true; 
                }
                // compare the read data with the file data (or buf if verifying by buffer)
                for(uint32_t j = 0; j < read_size; j++) {
                    // verify against file data
                    if(buf[j] != verify_buf[j]) {
                        printf("\r\nError at 0x%08X: expected 0x%02X, read 0x%02X\r\n", i*256 + j, verify_buf[j], buf[j]);
                        if(action==EEPROM_VERIFY_FILE) file_close(&eeprom->file_handle); // close the file if there was an error
                        return true; // error
                    }
                }
            }
        #endif

    }
    #if !EEPROM_DEBUG
        print_progress(address_blocks_total, address_blocks_total);
    #endif
    if(action != EEPROM_VERIFY_BUFFER) if(file_close(&eeprom->file_handle)) return true; // close the file after writing
    return false; // success
}

bool eeprom_action_erase(struct eeprom_info *eeprom, uint8_t *buf, uint32_t buf_size, uint8_t *verify_buf, uint32_t verify_buf_size, bool verify) {
    printf("Erase: Writing 0xFF to all bytes...\r\n");
    memset(buf, 0xFF, buf_size); // fill the buffer with 0xFF for erase
    if (eeprom_write(eeprom, buf, buf_size, true)) {
        return true;
    }
    printf("\r\nErase complete\r\n");
    if (verify) {
        printf("Erase verify...\r\n");
        memset(verify_buf, 0xFF, verify_buf_size); // fill the verify buffer with 0xFF for erase
        if (eeprom_read(eeprom, buf, buf_size, verify_buf, verify_buf_size, EEPROM_VERIFY_BUFFER)) {
            return true;
        }
        printf("\r\nErase verify complete\r\n");
    }
    return false; // success
}

bool eeprom_action_test(struct eeprom_info *eeprom, uint8_t *buf, uint32_t buf_size, uint8_t *verify_buf, uint32_t verify_buf_size) {
    printf("\r\nTest: Writing alternating patterns\r\n");
    printf("Writing 0xAA 0x55...\r\n");
    //fill the buffer with 0xaa 0x55 for testing
    for(uint32_t i = 0; i < verify_buf_size; i++) {
        if (i % 2 == 0) {
            verify_buf[i] = 0xAA; // even bytes
        } else {
            verify_buf[i] = 0x55; // odd bytes
        }
    }
    if (eeprom_write(eeprom, verify_buf, verify_buf_size, true)) {
        return true; // error during write
    }
    printf("\r\nWrite complete\r\nWrite verify...\r\n");
    if (eeprom_read(eeprom, buf, buf_size, verify_buf, verify_buf_size, EEPROM_VERIFY_BUFFER)) {
        return true; // error during read
    }
    printf("\r\nWrite verify complete\r\nWriting 0x55 0xAA...\r\n");
    //fill the buffer with 0x55aa for testing
    for(uint32_t i = 0; i < verify_buf_size; i++) {
        if (i % 2 == 0) {
            verify_buf[i] = 0x55; // even bytes
        } else {
            verify_buf[i] = 0xAA; // odd bytes
        }
    }
    if (eeprom_write(eeprom, verify_buf, verify_buf_size, true)) {
        return true; // error during write
    }
    printf("\r\nWrite complete\r\nWrite verify...\r\n");
    if (eeprom_read(eeprom, buf, buf_size, verify_buf, verify_buf_size, EEPROM_VERIFY_BUFFER)) {
        return true; // error during read
    }        
    printf("\r\nWrite verify complete\r\n");
    return false; // success
}

bool eeprom_action_write(struct eeprom_info *eeprom, uint8_t *buf, uint32_t buf_size, uint8_t *verify_buf, uint32_t verify_buf_size, bool verify) {
    printf("Write: Writing EEPROM from file %s...\r\n", eeprom->file_name);
    if (eeprom_write(eeprom, buf, buf_size, false)) {
        return true; // error during write
    }
    printf("\r\nWrite complete\r\n");
    if (verify) {   
        printf("Write verify...\r\n");
        if(eeprom_read(eeprom, buf, buf_size, verify_buf, verify_buf_size, EEPROM_VERIFY_FILE)){
            return true; // error during read
        }
        printf("\r\nWrite verify complete\r\n");
    }
    return false; // success
}

bool eeprom_action_read(struct eeprom_info *eeprom, uint8_t *buf, uint32_t buf_size, uint8_t *verify_buf, uint32_t verify_buf_size, bool verify) {
    printf("Read: Reading EEPROM to file %s...\r\n", eeprom->file_name);
    if(eeprom_read(eeprom, buf, buf_size, verify_buf, verify_buf_size, EEPROM_READ_TO_FILE)){
        return true; // error during read
    }
    printf("\r\nRead complete\r\n");
    if (verify) {
        printf("Read verify...\r\n");
        if(eeprom_read(eeprom, buf, buf_size, verify_buf, verify_buf_size, EEPROM_VERIFY_FILE)){
            return true; // error during read
        }
        printf("\r\nRead verify complete\r\n");
    }        
    return false; // success
}

bool eeprom_action_verify(struct eeprom_info *eeprom, uint8_t *buf, uint32_t buf_size, uint8_t *verify_buf, uint32_t verify_buf_size) {
    printf("Verify: Verifying EEPROM contents against file %s...\r\n", eeprom->file_name);
    if(eeprom_read(eeprom, buf, buf_size, verify_buf, verify_buf_size, EEPROM_VERIFY_FILE)){
        return true; // error during read
    }
    printf("\r\nVerify complete\r\n");
} 