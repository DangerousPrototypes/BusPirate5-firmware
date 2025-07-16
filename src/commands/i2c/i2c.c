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

enum i2c_dump_actions_enum {
    EEPROM_DUMP=0,
    EEPROM_ERASE,
    EEPROM_WRITE,
    EEPROM_READ,
    EEPROM_VERIFY,
    EEPROM_TEST,
    EEPROM_LIST
};

const struct cmdln_action_t i2c_dump_actions[] = {
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
    "Display contents (x to exit):%s eeprom dump -d 24x02",
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
    { 0, "-c", T_HELP_DISK_HEX_PAGER_OFF },
    { 0, "-a", T_HELP_EEPROM_ADDRESS_FLAG }, // alternate I2C address (default is 0x50)
    { 0, "-h", T_HELP_FLAG },   // help
};  

struct i2c_dump_t {
    uint32_t action; // action to perform
    uint8_t i2c_address_7bit; // 7-bit I2C address for the EEPROM
    uint32_t register_address_width; // width of the register address in bytes
    uint32_t register_address; // register address to start dumping from
    uint32_t data_size_bytes;
};

static bool i2c_get_args(struct i2c_dump_t *args) {
    command_var_t arg;
    char arg_str[9];
    
    // common function to parse the command line verb or action
    if(cmdln_args_get_action(i2c_dump_actions, count_of(i2c_dump_actions), &args->action)){
        ui_help_show(true, usage, count_of(usage), &options[0], count_of(options)); // show help if requested
        return true;
    }
    
    uint32_t i2c_address;
    if(!cmdln_args_find_flag_uint32('i', &arg, &i2c_address)) {
        printf("Missing I2C 7 bit address: -i <I2C_address>\r\n");
        return true;
    }
    if (i2c_address > 0x7F) {
        printf("Invalid I2C address: %d\r\n", args->i2c_address_7bit);
        return true; // error
    }
    args->i2c_address_7bit= i2c_address; // set the device address

    if(!cmdln_args_find_flag_uint32('w', &arg, &args->register_address_width)) {
        printf("Missing register address: -w <register address width>\r\n");
        return true;
    }

    if(!cmdln_args_find_flag_uint32('r', &arg, &args->register_address)) {
        printf("Missing register address: -r <register address>\r\n");
        return true;
    }
    if(!cmdln_args_find_flag_uint32('b', &arg, &args->data_size_bytes)) {
        printf("Missing byte length: -b <bytes>\r\n");
        return true;
    }
    #if 0
    // file to read/write/verify
    if ((args->action == EEPROM_READ || args->action == EEPROM_WRITE || args->action==EEPROM_VERIFY)) {
        if(file_get_args(args->file_name, sizeof(args->file_name))) return true;
    }
    #endif
    // let hex editor parse its own arguments
    //if(ui_hex_get_args(args->device->size_bytes, &args->start_address, &args->user_bytes)) return true;

    return false;
}

bool i2c_dump(struct i2c_dump_t *eeprom){
    // align the start address to 16 bytes, and calculate the end address
    struct hex_config_t hex_config;
    hex_config.max_size_bytes= 0xffffffff; // maximum size of the device in bytes
    ui_hex_get_args_config(&hex_config);
    hex_config.requested_bytes = eeprom->data_size_bytes; // user requested number of bytes to read
    hex_config.start_address = eeprom->register_address; // start address for the hex dump
    ui_hex_align_config(&hex_config);
    ui_hex_header_config(&hex_config);
    //printf("Dumping I2C address 0x%02X, register address width %d bytes, start address 0x%08X, data size %d bytes\r\n",
           //eeprom->i2c_address_7bit, eeprom->register_address_width, eeprom->register_address, eeprom->data_size_bytes);
    //printf("Aligned start address: 0x%08X, aligned end address: 0x%08X, total bytes to read: %d\r\n",
          // hex_config._aligned_start, hex_config._aligned_end, hex_config._total_read_bytes);

    uint8_t buf[16];

    for(uint32_t i=hex_config._aligned_start; i<(hex_config._aligned_end+1); i+=16) {
        uint8_t j =0;
        //printf("%04X: ", i); // print the address

        for (int16_t cnt = eeprom->register_address_width - 1; cnt >= 0; cnt--) {
            buf[j]= (i >> (cnt * 8)) & 0xFF; // write the register address byte by byte
            j++;
        }
        //printf("Buf: %02X %02X\r\n", buf[0], buf[1]); // print the first byte of the buffer
        // read the data from the EEPROM
        if (i2c_transaction((eeprom->i2c_address_7bit<<1) | 0, buf, eeprom->register_address_width, buf, 16)) {
            return true; // error
        }
        if(ui_hex_row_config(&hex_config, i, buf, 16)){
            // user exists pager
            return true; // exit the hex dump   
        }
    }
    return false;
}


void i2c_dump_handler(struct command_result* res) {
    if(res->help_flag) {
        //eeprom_display_devices(eeprom_devices, count_of(eeprom_devices)); // display the available EEPROM devices
        ui_help_show(true, usage, count_of(usage), &options[0], count_of(options)); // show help if requested
        return; // if help was shown, exit
    }
    
    struct i2c_dump_t eeprom;
    if(i2c_get_args(&eeprom)) {       
        return;
    }
    //we manually control any FALA capture
    fala_start_hook(); 
    
    #if 0
    if(eeprom.action == EEPROM_PROTECT){
        eeprom_probe_block_protect(&eeprom);
        goto i2c_dump_cleanup;
    }
    #endif

    if(eeprom.action == EEPROM_DUMP) {
        //dump the EEPROM contents
        i2c_dump(&eeprom);
        goto i2c_dump_cleanup; // no need to continue
    }
 #if 0
    if (eeprom.action == EEPROM_ERASE || eeprom.action == EEPROM_TEST) {
        if(eeprom_action_erase(&eeprom, buf, sizeof(buf), verify_buf, sizeof(verify_buf), eeprom.verify_flag || eeprom.action == EEPROM_TEST)) {
            goto i2c_dump_cleanup; // error during erase
        }
    }

    if (eeprom.action == EEPROM_TEST) {
        if(eeprom_action_test(&eeprom, buf, sizeof(buf), verify_buf, sizeof(verify_buf))) {
            goto i2c_dump_cleanup; // error during test
        }
    }

    if (eeprom.action==EEPROM_WRITE) {
        if(eeprom_action_write(&eeprom, buf, sizeof(buf), verify_buf, sizeof(verify_buf), eeprom.verify_flag)) {
            goto i2c_dump_cleanup; // error during write
        }
    }

    if (eeprom.action==EEPROM_READ) {
        if(eeprom_action_read(&eeprom, buf, sizeof(buf), verify_buf, sizeof(verify_buf), eeprom.verify_flag)) {
            goto i2c_dump_cleanup; // error during read
        }
    }

    if (eeprom.action==EEPROM_VERIFY) {
        if(eeprom_action_verify(&eeprom, buf, sizeof(buf), verify_buf, sizeof(verify_buf))){
            goto i2c_dump_cleanup; // error during verify
        }
    }
    printf("Success :)\r\n");
#endif
i2c_dump_cleanup:
    //we manually control any FALA capture
    fala_stop_hook();
    fala_notify_hook();

}
