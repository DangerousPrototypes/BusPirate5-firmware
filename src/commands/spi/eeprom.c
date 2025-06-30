#include <stdio.h>
#include "pico/stdlib.h"
#include <stdint.h>
#include <string.h>
#include "pirate.h"
#include "pirate/hwi2c_pio.h"
#include "ui/ui_term.h"
#include "command_struct.h"
#include "ui/ui_help.h"
#include "ui/ui_cmdln.h"
#include "binmode/fala.h"
#include "fatfs/ff.h"       // File system related
#include "ui/ui_hex.h" // Hex display related
#include "ui/ui_progress_indicator.h" // Progress indicator related
#include "commands/i2c/eeprom.h"
#include "pirate/file.h" // File handling related
#include "pirate/hwspi.h" // SPI related functions

#define SPI_EEPROM_READ_CMD 0x03 // Read command for EEPROM
#define SPI_EEPROM_WRITE_CMD 0x02 // Write command for EEPROM   
#define SPI_EEPROM_WRDI_CMD 0x04 // Write Disable command for EEPROM
#define SPI_EEPROM_WREN_CMD 0x06 // Write Enable command for EEPROM
#define SPI_EEPROM_RDSR_CMD 0x05 // Read Status Register command for EEPROM
#define SPI_EEPROM_WRSR_CMD 0x01 // Write Status Register command

struct spi_eeprom_device_t {
    char name[9];
    uint32_t size_bytes;
    uint8_t address_bytes; 
    uint8_t block_select_bits;
    uint8_t block_select_offset;
    uint16_t page_bytes; 
    uint32_t max_speed_khz; //if max speed is <10MHz, then specify the speed in kHz
};

static const struct spi_eeprom_device_t eeprom_devices[] = {
    { "25X010",    128,     1, 0, 0,   8, 10000 }, //8 and 16 byte page variants
    //{ "25X010",    128,     1, 0, 0,  16 },//use the lowest common page size
    { "25X020",    256,     1, 0, 0,   8, 10000 },
    //{ "25X020",    256,     1, 0, 0,  16 },
    { "25X040",    512,     1, 1, 3,   8, 10000 },
    //{ "25X040",    512,     1, 1, 3,  16 },
    { "25X080",   1024,     2, 0, 0,  16, 10000 },
    //{ "25X080",   1024,     2, 0, 0,  32 },
    { "25X160",   2048,     2, 0, 0,  16, 10000 },
    //{ "25X160",   2048,     2, 0, 0,  32 },
    { "25X320",    4096,     2, 0, 0,  32, 10000 },
    { "25X640",    8192,     2, 0, 0,  32, 10000 },
    { "25X128",  16384,     2, 0, 0,  64, 10000 },
    { "25X256",  32768,     2, 0, 0,  64, 10000 },
    { "25X512",   65536,     2, 0, 0, 128, 10000 },
    { "25XM01",  131072,     3, 0, 0, 256, 10000 },
    { "25XM02",  262144,     3, 0, 0, 256, 5000 }, //5MHz
    { "25XM04",  524288,     3, 0, 0, 256, 8000 } 
};

enum eeprom_actions_enum {
    EEPROM_DUMP=0,
    EEPROM_ERASE,
    EEPROM_WRITE,
    EEPROM_READ,
    EEPROM_VERIFY,
    EEPROM_TEST,
    EEPROM_LIST
};

const struct cmdln_action_t eeprom_actions[] = {
    { EEPROM_DUMP, "dump" },
    { EEPROM_ERASE, "erase" },
    { EEPROM_WRITE, "write" },
    { EEPROM_READ, "read" },
    { EEPROM_VERIFY, "verify" },
    { EEPROM_TEST, "test" },
    { EEPROM_LIST, "list"  }
};

static const char* const usage[] = {
    "eeprom [dump|erase|write|read|verify|test|list]\r\n\t[-d <device>] [-f <file>] [-v(verify)] [-s <start address>] [-b <bytes>] [-a <i2c address>] [-h(elp)]",
    "List available EEPROM devices:%s eeprom list",
    "Display contents:%s eeprom dump -d 24x02",
    "Display 16 bytes starting at address 0x60:%s eeprom dump -d 24x02 -s 0x60 -b 16",
    "Erase, verify:%s eeprom erase -d 24x02 -v",
    "Write from file, verify:%s eeprom write -d 24x02 -f example.bin -v",
    "Read to file, verify:%s eeprom read -d 24x02 -f example.bin -v",
    "Verify against file:%s eeprom verify -d 24x02 -f example.bin",
    "Test chip (full erase/write/verify):%s eeprom test -d 24x02",
    "Use alternate I2C address (0x50 default):%s eeprom dump -d 24x02 -a 0x53",
};

static const struct ui_help_options options[] = {
    { 1, "", T_HELP_EEPROM },               // command help
    { 0, "dump", T_HELP_EEPROM_DUMP },  
    { 0, "erase", T_HELP_EEPROM_ERASE },    // erase
    { 0, "write", T_HELP_EEPROM_WRITE },    // write
    { 0, "read", T_HELP_EEPROM_READ },      // read
    { 0, "verify", T_HELP_EEPROM_VERIFY },  // verify
    { 0, "test", T_HELP_EEPROM_TEST },      // test
    { 0, "list", T_HELP_EEPROM_LIST},      // list devices
    { 0, "-f", T_HELP_EEPROM_FILE_FLAG },   // file to read/write/verify
    { 0, "-v", T_HELP_EEPROM_VERIFY_FLAG }, // with verify (after write)
    { 0, "-s", T_HELP_EEPROM_START_FLAG },  // start address for dump/read/write
    { 0, "-b", T_HELP_EEPROM_BYTES_FLAG },  // bytes to dump/read/write
    { 0, "-a", T_HELP_EEPROM_ADDRESS_FLAG }, // address for read/write
    { 0, "-h", T_HELP_FLAG },   // help
};  


struct eeprom_hal_t {
    bool (*read_16)(uint8_t block_select_bits, uint8_t address_bytes, uint8_t *address, char *buf);
    bool (*read_256)(uint8_t block_select_bits, uint8_t address_bytes, uint8_t *address, char *buf);
    bool (*write_page)(uint8_t block_select_bits, uint8_t address_bytes, uint8_t *address, uint32_t page_size, char *buf);
    bool (*write_protection_blocks)(uint8_t block_select_bits, uint8_t address_bytes, uint8_t *address, uint8_t reg);
    bool (*poll_busy)(uint8_t block_select_bits);
};

struct eeprom_info{
    const struct spi_eeprom_device_t* device;
    const struct eeprom_hal_t *hal; // HAL for EEPROM operations
    uint8_t device_address; // 7-bit address for the device
    uint32_t action;
    char file_name[13]; // file to read/write/verify
    bool verify_flag; // verify flag
    uint32_t start_address; // start address for read/write
    uint32_t user_bytes; // user specified number of bytes to read/write
    FIL file_handle;     // file handle
};

// custom I2C write function for EEPROM to get best speed
static hwi2c_status_t eeprom_i2c_write(uint8_t i2c_addr, uint8_t *eeprom_addr, uint8_t eeprom_addr_len, uint8_t* txbuf, uint txbuf_len) {
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


static void eeprom_display_devices(void) {
    printf("\r\nAvailable EEPROM devices:\r\n");
    printf("Device\t|Bytes\t|Page Size\t|Addr Bytes\t|Block Select Bits\r\n");
    for(uint8_t i = 0; i < count_of(eeprom_devices); i++) {
        // Print device information
        printf("%s%s|%d\t|%d\t\t|%d\t\t|%d\r\n",
               eeprom_devices[i].name,
               strlen(eeprom_devices[i].name)>7?"\t\t":"\t",
               eeprom_devices[i].size_bytes,
               eeprom_devices[i].page_bytes,
               eeprom_devices[i].address_bytes,
               eeprom_devices[i].block_select_bits);
    }
    printf("\r\nCompatible with most common 24X I2C EEPROMs: AT24C, 24C/LC/AA/FC, etc.\r\n");
    printf("3.3volts is suitable for most devices.\r\n\r\n");
}

static uint32_t eeprom_get_address_blocks_total(struct eeprom_info *eeprom) {
    //Each EEPROM 8 bit address covers 256 bytes
    uint32_t addr_range_256b= eeprom->device->size_bytes / EEPROM_ADDRESS_PAGE_SIZE;
    if(addr_range_256b ==0) addr_range_256b = 1; //for devices smaller than 256 bytes
    return addr_range_256b;
}

static uint32_t eeprom_get_address_block_size(struct eeprom_info *eeprom) {
    uint32_t read_size = EEPROM_ADDRESS_PAGE_SIZE; // read 256 bytes at a time
    if(eeprom->device->size_bytes < EEPROM_ADDRESS_PAGE_SIZE){
        read_size = eeprom->device->size_bytes; // use the page size for reading        
    }
    return read_size;
}

static uint32_t eeprom_get_address_block_start(struct eeprom_info *eeprom, uint32_t address_block) {
    // calculate the address for the 256 byte address range
    return ((address_block * EEPROM_ADDRESS_PAGE_SIZE) >> 8) & 0xFF; // high byte
}

//get the address with optional block select bits 
static uint8_t eeprom_get_address_block_i2c_address(struct eeprom_info *eeprom, uint32_t address_block) {
    // if the device has block select bits, we need to adjust the address
    // if the device doen't have block select bits, then address_block is always 0 in the upper bits and this has no effect
    uint8_t block_select_bits = ((address_block*EEPROM_ADDRESS_PAGE_SIZE)>>(8*eeprom->device->address_bytes));
    // adjust to correct location in address (ie 1025)
    uint8_t i2caddr_7bit = eeprom->device_address | (block_select_bits << eeprom->device->block_select_offset);  
    return i2caddr_7bit;
} 

//function to return the block select, address given a byte address
static bool eeprom_get_address(struct eeprom_info *eeprom, uint32_t address, uint8_t *i2caddr_7bit, uint8_t *address_bytes) {
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


//poll for busy status, return false if write is complete, true if timeout
static bool spi_eeprom_poll_busy(uint8_t block_select_bits){
    uint8_t reg;
    for(uint32_t i=0; i<0xfffff; i++) {
        hwspi_write_read_cs((uint8_t[]){SPI_EEPROM_RDSR_CMD}, 1, &reg, 1); // send the read status command
        if((reg & 0x01) == 0) { // check if WIP bit is clear
            return false; // write is complete
        }
    }
    //printf("Error: EEPROM write timeout\r\n");
    return true;
}

//TODO: HAL function to determine the write size, i2c address, block select bits, etc
static bool spi_eeprom_read_16(uint8_t block_select_bits, uint8_t address_bytes, uint8_t *address, char *buf) {
    //TODO: get the block select bits from the device
    //ensure row alignment!
    address[address_bytes-1] &= 0xF0; // align to 16 bytes
    hwspi_write_read_cs((uint8_t[]){SPI_EEPROM_READ_CMD|block_select_bits, address[0], address[1], address[2]}, 1+address_bytes, buf, 16); // read 16 bytes from the EEPROM
    return false;
}

static bool spi_eeprom_read_256(uint8_t block_select_bits, uint8_t address_bytes, uint8_t *address, char *buf){
    //TODO: get block select bits from the device
    //ensure row alignment!
    address[address_bytes-1] &= 0x00; // align to 256
    hwspi_write_read_cs((uint8_t[]){SPI_EEPROM_READ_CMD|block_select_bits, address[0], address[1], address[2]}, 1+address_bytes, buf, 256); // read
    return false;
}

static bool spi_eeprom_write_page(uint8_t block_select_bits, uint8_t address_bytes, uint8_t *address, uint32_t page_size, char *buf){
    //TODO: get block select bits from the device
    // ensure row alignment!
    address[address_bytes-1] &= page_size; // align to page
    hwspi_write_read_cs((uint8_t[]){SPI_EEPROM_WREN_CMD}, 1, NULL, 0); // enable write
    //TODO: this will need to be copied into a buffer with command, address, page...
    hwspi_write_read_cs((uint8_t[]){SPI_EEPROM_WRITE_CMD|block_select_bits, address[0], address[1], address[2]}, 1 + address_bytes + page_size, NULL, 0); // write 0x00 to the status register
}

static struct eeprom_hal_t spi_eeprom_hal = {
    .read_16 = spi_eeprom_read_16,
    .read_256 = spi_eeprom_read_256,
    .write_page = spi_eeprom_write_page,
    .write_protection_blocks = NULL, // not implemented
    .poll_busy = spi_eeprom_poll_busy
};

//function to display hex editor like dump of the EEPROM contents
static bool eeprom_dump(struct eeprom_info *eeprom, char *buf, uint32_t buf_size){
    // align the start address to 16 bytes, and calculate the end address
    struct hex_config_t hex_config;
    hex_config.max_size_bytes= eeprom->device->size_bytes; // maximum size of the device in bytes
    ui_hex_get_args_config(&hex_config);
    ui_hex_align_config(&hex_config);
    ui_hex_header_config(&hex_config);

    for(uint32_t i=hex_config._aligned_start; i<(hex_config._aligned_end+1); i+=16) {
        // find the address for current byte and read 16 bytes at a time
        //uint8_t i2caddr_7bit;
        //uint8_t address_bytes[2];
       // if(eeprom_get_address(eeprom, i, &i2caddr_7bit,address_bytes)) return true; // if there was an error getting the address
        //if(i2c_transaction(i2caddr_7bit<<1, address_bytes, eeprom->device->address_bytes, buf, 16)) {
        //    return true; // error
        //}
        //hwspi_write_read_cs((uint8_t[]){SPI_EEPROM_READ_CMD, (uint8_t)i}, 2, buf, 16); // read 16 bytes from the EEPROM
        eeprom->hal->read_16(0x00, eeprom->device->address_bytes, (uint8_t*)&i, buf); // read 16 bytes from the EEPROM
        ui_hex_row_config(&hex_config, i, buf, 16);
    }
}

static bool eeprom_write(struct eeprom_info *eeprom, char *buf, uint32_t buf_size, bool write_from_buf) {
   
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

static bool eeprom_read(struct eeprom_info *eeprom, char *buf, uint32_t buf_size, char *verify_buf, uint32_t verify_buf_size, bool verify, bool verify_against_verify_buf){   
      
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


static bool eeprom_get_args(struct eeprom_info *args) {
    command_var_t arg;
    char arg_str[9];
    
    // common function to parse the command line verb or action
    if(cmdln_args_get_action(eeprom_actions, count_of(eeprom_actions), &args->action)){
        ui_help_show(true, usage, count_of(usage), &options[0], count_of(options)); // show help if requested
        return true;
    }

    if(args->action == EEPROM_LIST) {
        eeprom_display_devices(); // display devices if list action
        return true; // no error, just listing devices
    }
    
    if(!cmdln_args_find_flag_string('d', &arg, sizeof(arg_str), arg_str)){
        printf("Missing EEPROM device name: -d <device name>\r\n");
        eeprom_display_devices();
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
        eeprom_display_devices();
        return true; // error
    }
 
    args->device = &eeprom_devices[eeprom_type];
    uint32_t i2c_address = 0x50; // default I2C address for EEPROMs
    if(cmdln_args_find_flag_uint32('a' | 0x20, &arg, &i2c_address)) {
        if (i2c_address > 0x7F) {
            printf("Invalid I2C address: %d\r\n", args->device_address);
            return true; // error
        }
    }
    args->device_address = i2c_address; // set the device address


    // verify_flag
    args->verify_flag = cmdln_args_find_flag('v' | 0x20);

    // file to read/write/verify
    if ((args->action == EEPROM_READ || args->action == EEPROM_WRITE || args->action==EEPROM_VERIFY)) {
        if(file_get_args(args->file_name, sizeof(args->file_name))) return true;
    }

    // let hex editor parse its own arguments
    if(ui_hex_get_args(args->device->size_bytes, &args->start_address, &args->user_bytes)) return true;

    return false;
}

void spi_eerpom_status_reg_print(uint8_t reg) {
    // print the status register in a human readable format
    printf("Status Register: 0x%02X\r\n", reg);
    printf("Write Enable Latch (WEL): %s\r\n", (reg & 0x02) ? "Enabled" : "Disabled");
    printf("Write In Progress (WIP): %s\r\n", (reg & 0x01) ? "In Progress" : "Idle");
    printf("Block Protect Bits (BP1, BP0): %d, %d\r\n", (reg >> 2) & 0x01, (reg >> 3) & 0x01);
    printf("Write Protect Enable (WPEN): %s\r\n", (reg & 0x80) ? "Enabled" : "Disabled");
}

bool spi_eeprom_poll_write_complete(void) {
    // Poll for write complete
    uint8_t reg;
    for(uint32_t i=0; i<0xfffff; i++) {
        hwspi_write_read_cs((uint8_t[]){SPI_EEPROM_RDSR_CMD}, 1, &reg, 1); // send the read status command
        if((reg & 0x01) == 0) { // check if WIP bit is clear
            return false; // write is complete
        }
    }
    printf("Error: EEPROM write timeout\r\n");
    return true;
}

void spi_eeprom_handler(struct command_result* res) {
    if(res->help_flag) {
        eeprom_display_devices(); // display the available EEPROM devices
        ui_help_show(true, usage, count_of(usage), &options[0], count_of(options)); // show help if requested
        return; // if help was shown, exit
    }
    struct eeprom_info eeprom;
    if(eeprom_get_args(&eeprom)) {       
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
        //eeprom_dump(&eeprom, buf, sizeof(buf));
        //test for write protect bits....
        //|7|6|5|4|3|2|1|0|
        //|-|-|-|-|-|-|-|-|
        //|WPEN|X|X|X|BP1|BP0|WEL|WIP|
        uint8_t reg, reg_old;
        hwspi_write_read_cs((uint8_t[]){SPI_EEPROM_RDSR_CMD}, 1, &reg_old, 1); // read the status register
        spi_eerpom_status_reg_print(reg_old); // print the status register
        //now write 0x00 to the status register to disable write protect
        hwspi_write_read_cs((uint8_t[]){SPI_EEPROM_WREN_CMD}, 1, NULL, 0); // enable write
        hwspi_write_read_cs((uint8_t[]){SPI_EEPROM_WRSR_CMD, 0x00}, 2, NULL, 0); // write 0x00 to the status register
        if(spi_eeprom_poll_write_complete()) goto eeprom_cleanup; // poll for write complete
        //confirm status = 0
        hwspi_write_read_cs((uint8_t[]){SPI_EEPROM_RDSR_CMD}, 1, &reg, 1); // read the status register again
        if(reg != 0x00) {
            printf("Error: Failed to disable write protect\r\n");
        }else {
            printf("Write protect disabled\r\nTesting for WPEN, BP1, BP0...\r\n");
        }
        //write WPEN, BP1, BP0, then read back to see the status
        hwspi_write_read_cs((uint8_t[]){SPI_EEPROM_WREN_CMD}, 1, NULL, 0); // enable write
        hwspi_write_read_cs((uint8_t[]){SPI_EEPROM_WRSR_CMD,0b10001100}, 2, NULL, 0); // write 0x00 to the status register
        if(spi_eeprom_poll_write_complete()) goto eeprom_cleanup; // poll for write complete
        hwspi_write_read_cs((uint8_t[]){SPI_EEPROM_RDSR_CMD}, 1, &reg, 1); // read the status register again
        printf("WPEN: %s\r\n", (reg & 0x80) ? "Present" : "Not detected");
        printf("BP1: %s\r\n", (reg & 0b1000) ? "Present" : "Not detected");
        printf("BP0: %s\r\n", (reg & 0b100) ? "Present" : "Not detected");
        //printf("WEL: %s\r\n", (reg & 0b10) ? "Enabled" : "Disabled");
        //restore old settings
        printf("Restoring old status register value: 0x%02X\r\n", reg_old);
        hwspi_write_read_cs((uint8_t[]){SPI_EEPROM_WREN_CMD}, 1, NULL, 0); // enable write
        hwspi_write_read_cs((uint8_t[]){SPI_EEPROM_WRSR_CMD, reg_old}, 2, NULL, 0); // write old status register value
        if(spi_eeprom_poll_write_complete()) goto eeprom_cleanup; // poll for write complete
        printf("Done :)\r\n");
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
    printf("Success :)\r\n");

eeprom_cleanup:
    //we manually control any FALA capture
    fala_stop_hook();
    fala_notify_hook();

}
