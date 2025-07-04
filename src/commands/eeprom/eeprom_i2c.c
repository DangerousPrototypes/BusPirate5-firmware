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
#include "ui/ui_hex.h" // Hex display related
#include "ui/ui_progress_indicator.h" // Progress indicator related
#include "pirate/file.h" // File handling related
#include "pirate/hwi2c_pio.h"
#include "eeprom_base.h"

#define I2C_EEPROM_DEFAULT_ADDRESS 0x50 // Default I2C address for EEPROMs

static const struct eeprom_device_t eeprom_devices[] = {
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
    { 1, "", T_HELP_I2C_EEPROM },               // command help
    { 0, "dump", T_HELP_EEPROM_DUMP },  
    { 0, "erase", T_HELP_EEPROM_ERASE },    // erase
    { 0, "write", T_HELP_EEPROM_WRITE },    // write
    { 0, "read", T_HELP_EEPROM_READ },      // read
    { 0, "verify", T_HELP_EEPROM_VERIFY },  // verify
    { 0, "test", T_HELP_EEPROM_TEST },      // test
    { 0, "list", T_HELP_EEPROM_LIST},      // list devices
    { 0, "-f", T_HELP_EEPROM_FILE_FLAG },   // file to read/write/verify
    { 0, "-v", T_HELP_EEPROM_VERIFY_FLAG }, // with verify (after write)
    { 0, "-s", UI_HEX_HELP_START }, // start address for dump
    { 0, "-b", UI_HEX_HELP_BYTES }, // bytes to dump
    { 0, "-q", UI_HEX_HELP_QUIET}, // quiet mode, disable address and ASCII columns
    { 0, "-a", T_HELP_EEPROM_ADDRESS_FLAG }, // alternate I2C address (default is 0x50)
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
        if(!i2c_result){
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

static bool i2c_eeprom_write_page(struct eeprom_info *eeprom, uint32_t address, uint8_t *buf, uint32_t page_write_size){
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

static bool eeprom_get_args(struct eeprom_info *args) {
    command_var_t arg;
    char arg_str[9];
    
    // common function to parse the command line verb or action
    if(cmdln_args_get_action(eeprom_actions, count_of(eeprom_actions), &args->action)){
        ui_help_show(true, usage, count_of(usage), &options[0], count_of(options)); // show help if requested
        return true;
    }

    if(args->action == EEPROM_LIST) {
        eeprom_display_devices(eeprom_devices, count_of(eeprom_devices)); // display devices if list action
        printf("Compatible with most common 24X I2C EEPROMs: AT24C, 24C/LC/AA/FC, etc.\r\n");
        printf("3.3volts is suitable for most devices.\r\n");
        return true; // no error, just listing devices
    }
    
    if(!cmdln_args_find_flag_string('d', &arg, sizeof(arg_str), arg_str)){
        printf("Missing EEPROM device name: -d <device name>\r\n");
        eeprom_display_devices(eeprom_devices, count_of(eeprom_devices)); // display the available EEPROM devices
        return true;
    }

    // we have a device name, find it in the list
    uint8_t eeprom_type = 0xFF; // invalid by default
    strupr(arg_str);
    for(uint8_t i = 0; i <count_of(eeprom_devices); i++) {
        if(strcmp(arg_str, eeprom_devices[i].name) == 0) {
            eeprom_type = i; // found the device
            break;
        }
    }

    if(eeprom_type == 0xFF) {
        printf("Invalid EEPROM device name: %s\r\n", arg_str);
        eeprom_display_devices(eeprom_devices, count_of(eeprom_devices));
        return true; // error
    }
 
    args->device = &eeprom_devices[eeprom_type];
    uint32_t i2c_address = I2C_EEPROM_DEFAULT_ADDRESS; // default I2C address for EEPROMs
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
    //if(ui_hex_get_args(args->device->size_bytes, &args->start_address, &args->user_bytes)) return true;

    return false;
}


void i2c_eeprom_handler(struct command_result* res) {
    if(res->help_flag) {
        //eeprom_display_devices(eeprom_devices, count_of(eeprom_devices)); // display the available EEPROM devices
        ui_help_show(true, usage, count_of(usage), &options[0], count_of(options)); // show help if requested
        return; // if help was shown, exit
    }
    struct eeprom_info eeprom;
    eeprom.hal = &i2c_eeprom_hal; // set the HAL for EEPROM operations
    if(eeprom_get_args(&eeprom)) {       
        return;
    }

    //nice full description of chip and capabilities
    printf("%s: %d bytes,  %d block select bits, %d byte address, %d byte pages\r\n\r\n", eeprom.device->name, eeprom.device->size_bytes, eeprom.device->block_select_bits, eeprom.device->address_bytes, eeprom.device->page_bytes);

    char buf[EEPROM_ADDRESS_PAGE_SIZE]; // buffer for reading/writing
    uint8_t verify_buf[EEPROM_ADDRESS_PAGE_SIZE]; // buffer for reading data from EEPROM

    //we manually control any FALA capture
    fala_start_hook(); 
    
    #if 0
    if(eeprom.action == EEPROM_PROTECT){
        eeprom_probe_block_protect(&eeprom);
        goto i2c_eeprom_cleanup;
    }
    #endif

    if(eeprom.action == EEPROM_DUMP) {
        //dump the EEPROM contents
        eeprom_dump(&eeprom, buf, sizeof(buf));
        goto i2c_eeprom_cleanup; // no need to continue
    }
 
    if (eeprom.action == EEPROM_ERASE || eeprom.action == EEPROM_TEST) {
        if(eeprom_action_erase(&eeprom, buf, sizeof(buf), verify_buf, sizeof(verify_buf), eeprom.verify_flag || eeprom.action == EEPROM_TEST)) {
            goto i2c_eeprom_cleanup; // error during erase
        }
    }

    if (eeprom.action == EEPROM_TEST) {
        if(eeprom_action_test(&eeprom, buf, sizeof(buf), verify_buf, sizeof(verify_buf))) {
            goto i2c_eeprom_cleanup; // error during test
        }
    }

    if (eeprom.action==EEPROM_WRITE) {
        if(eeprom_action_write(&eeprom, buf, sizeof(buf), verify_buf, sizeof(verify_buf), eeprom.verify_flag)) {
            goto i2c_eeprom_cleanup; // error during write
        }
    }

    if (eeprom.action==EEPROM_READ) {
        if(eeprom_action_read(&eeprom, buf, sizeof(buf), verify_buf, sizeof(verify_buf), eeprom.verify_flag)) {
            goto i2c_eeprom_cleanup; // error during read
        }
    }

    if (eeprom.action==EEPROM_VERIFY) {
        if(eeprom_action_verify(&eeprom, buf, sizeof(buf), verify_buf, sizeof(verify_buf))){
            goto i2c_eeprom_cleanup; // error during verify
        }
    }
    printf("Success :)\r\n");

i2c_eeprom_cleanup:
    //we manually control any FALA capture
    fala_stop_hook();
    fala_notify_hook();

}
