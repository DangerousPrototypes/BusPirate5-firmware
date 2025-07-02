#include <stdio.h>
#include "pico/stdlib.h"
#include <stdint.h>
#include <string.h>
#include "pirate.h"
#include "ui/ui_term.h"
#include "command_struct.h"
#include "ui/ui_help.h"
#include "ui/ui_cmdln.h"
#include "binmode/fala.h"
#include "fatfs/ff.h"       // File system related
//#include "ui/ui_hex.h" // Hex display related
//#include "ui/ui_progress_indicator.h" // Progress indicator related
#include "pirate/file.h" // File handling related
#include "pirate/hwi2c_pio.h"
#include "eeprom_base.h"

static const struct eeprom_device_t i2c_eeprom_devices[] = {
    { "24X01",   128,    1, 0, 0, 8, 400   },
    { "24X02",   256,    1, 0, 0, 8, 400   },
    { "24X04",   512,    1, 1, 0, 16, 400  },
    { "24X08",   1024,   1, 2, 0, 16, 400  },
    { "24X16",   2048,   1, 3, 0, 16, 400  },
    { "24X32",   4096,   2, 0, 0, 32, 400  },
    { "24X64",   8192,   2, 0, 0, 32, 400  },
    { "24X128",  16384,  2, 0, 0, 64, 400  },
    { "24X256",  32768,  2, 0, 0, 64, 400  },
    { "24X512",  65536,  2, 0, 0, 128, 400 },
    { "24X1025", 131072, 2, 1, 3, 128, 400 },
    { "24X1026", 131072, 2, 1, 0, 128, 400 },
    { "24XM01",  131072, 2, 1, 0, 256, 400 },    
    { "24XM02",  262144, 2, 2, 0, 256, 400 }
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

//----------------------------------------------------------------------------------
// I2C EEPROM hardware abstraction layer functions
//----------------------------------------------------------------------------------
//poll for busy status, return false if write is complete, true if timeout
static bool i2c_eeprom_poll_busy(struct eeprom_info *eeprom){
    //poll for write complete
    for(uint32_t i = 0; i<0xfffffu; i++){
    uint32_t timeout = 0xfffffu; // default timeout for I2C operations
        hwi2c_status_t i2c_result = pio_i2c_write_array_timeout(eeprom->device_address<<1, NULL, 0, 0xfffffu);
        if(i2c_result){
            return false; // write is complete
        }
    }
    return true; // timeout, write is not complete
}

static bool i2c_eeprom_read(struct eeprom_info *eeprom, uint32_t address, uint32_t read_bytes, uint8_t *buf) {
    //ensure row alignment!
    if(read_bytes !=16 && read_bytes != 256) {
        printf("Internal error: Invalid read size, must be 16 or 256 bytes\r\n");
        return true; // invalid read size
    }

    // get the address for the current byte
    uint8_t block_select_bits = 0;
    uint8_t address_array[3];
    if(eeprom->hal->get_address(eeprom, address, &block_select_bits, address_array)){ 
        return true; // error getting address
    }

    // read the data from the EEPROM
    if (i2c_transaction((eeprom->device_address|block_select_bits)<<1, address_array, eeprom->device->address_bytes, buf, read_bytes)) {
        return true; // error
    }
    return false;
}

static bool i2c_eeprom_write_page(struct eeprom_info *eeprom, uint32_t address, uint8_t *buf){
    //get address
    uint8_t block_select_bits = 0;
    uint8_t address_array[3];
    if(eeprom->hal->get_address(eeprom, address, &block_select_bits, address_array))return true; // get the address   

    uint32_t timeout = 0xfffffu; // default timeout for I2C operations
    if(pio_i2c_start_timeout(timeout)) return true;
    hwi2c_status_t i2c_result = pio_i2c_write_timeout((eeprom->device_address|block_select_bits)<<1, timeout);
    if(i2c_result != HWI2C_OK) return true;

    for(uint8_t i = 0; i < eeprom->device->address_bytes; i++) {
        i2c_result = pio_i2c_write_timeout(address_array[i], timeout);
        if(i2c_result != HWI2C_OK) return true;
    }

    for(uint32_t i=0; i < eeprom->device->page_bytes; i++) {
        i2c_result = pio_i2c_write_timeout(buf[i], timeout);
        if(i2c_result != HWI2C_OK) return true;
    }

    if (pio_i2c_stop_timeout(timeout)) return true;
    if (pio_i2c_wait_idle_extern(timeout)) return true;

    //poll for write complete
    if(i2c_eeprom_poll_busy(eeprom))return true;
    return false; // write is complete
}

static struct eeprom_hal_t i2c_eeprom_hal = {
    .get_address = eeprom_get_address,
    .read = i2c_eeprom_read,
    .write_page = i2c_eeprom_write_page,
    .write_protection_blocks = NULL, // not implemented
};

static bool eeprom_get_args(struct eeprom_info *args, const struct eeprom_device_t *eeprom_devices, uint32_t count_of_eeprom_devices) {
    command_var_t arg;
    char arg_str[9];
    
    // common function to parse the command line verb or action
    if(cmdln_args_get_action(eeprom_actions, count_of(eeprom_actions), &args->action)){
        ui_help_show(true, usage, count_of(usage), &options[0], count_of(options)); // show help if requested
        return true;
    }

    if(args->action == EEPROM_LIST) {
        eeprom_display_devices(eeprom_devices, count_of_eeprom_devices); // display devices if list action
            printf("\r\nCompatible with most common 24X I2C EEPROMs: AT24C, 24C/LC/AA/FC, etc.\r\n");
    printf("3.3volts is suitable for most devices.\r\n");
        return true; // no error, just listing devices
    }
    
    if(!cmdln_args_find_flag_string('d', &arg, sizeof(arg_str), arg_str)){
        printf("Missing EEPROM device name: -d <device name>\r\n");
        eeprom_display_devices(eeprom_devices, count_of_eeprom_devices); // display the available EEPROM devices
        return true;
    }

    // we have a device name, find it in the list
    uint8_t eeprom_type = 0xFF; // invalid by default
    strupr(arg_str);
    for(uint8_t i = 0; i <count_of_eeprom_devices; i++) {
        if(strcmp(arg_str, eeprom_devices[i].name) == 0) {
            eeprom_type = i; // found the device
            break;
        }
    }

    if(eeprom_type == 0xFF) {
        printf("Invalid EEPROM device name: %s\r\n", arg_str);
        eeprom_display_devices(eeprom_devices, count_of_eeprom_devices);
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

void spi_eeprom_handler(struct command_result* res) {
    if(res->help_flag) {
        eeprom_display_devices(spi_eeprom_devices, count_of(spi_eeprom_devices)); // display the available EEPROM devices
        ui_help_show(true, usage, count_of(usage), &options[0], count_of(options)); // show help if requested
        return; // if help was shown, exit
    }
    struct eeprom_info eeprom;
    eeprom.hal = &spi_eeprom_hal; // set the HAL for EEPROM operations
    if(eeprom_get_args(&eeprom, spi_eeprom_devices, count_of(spi_eeprom_devices))) {       
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
        #if 0
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
        #endif
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
            if (eeprom_read(&eeprom, buf, sizeof(buf), verify_buf, sizeof(verify_buf), EEPROM_VERIFY_BUFFER)) {
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
        if (eeprom_read(&eeprom, buf, sizeof(buf), verify_buf, sizeof(verify_buf), EEPROM_VERIFY_BUFFER)) {
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
        if (eeprom_read(&eeprom, buf, sizeof(buf), verify_buf, sizeof(verify_buf), EEPROM_VERIFY_BUFFER)) {
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
            if(eeprom_read(&eeprom, buf, sizeof(buf), verify_buf, sizeof(verify_buf), EEPROM_VERIFY_FILE)){
                goto eeprom_cleanup;
            }
            printf("\r\nWrite verify complete\r\n");
        }
    }

    if (eeprom.action==EEPROM_READ) {
        printf("Read: Reading EEPROM to file %s...\r\n", eeprom.file_name);
        if(eeprom_read(&eeprom, buf, sizeof(buf), verify_buf, sizeof(verify_buf), EEPROM_READ_TO_FILE)){
            goto eeprom_cleanup;
        }
        printf("\r\nRead complete\r\n");
        if (eeprom.verify_flag) {
            printf("Read verify...\r\n");
            if(eeprom_read(&eeprom, buf, sizeof(buf), verify_buf, sizeof(verify_buf), EEPROM_VERIFY_FILE)){
                goto eeprom_cleanup;
            }
            printf("\r\nRead verify complete\r\n");
        }        
    }

    if (eeprom.action==EEPROM_VERIFY) {
        printf("Verify: Verifying EEPROM contents against file %s...\r\n", eeprom.file_name);
        if(eeprom_read(&eeprom, buf, sizeof(buf), verify_buf, sizeof(verify_buf), EEPROM_VERIFY_FILE)){
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
