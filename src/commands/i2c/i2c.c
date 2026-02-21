#include <stdio.h>
#include "pico/stdlib.h"
#include <stdint.h>
#include <string.h>
#include "pirate.h"
#include "ui/ui_term.h"
#include "command_struct.h"
#include "ui/ui_help.h"
#include "lib/bp_args/bp_cmd.h"
#include "binmode/fala.h"
#include "fatfs/ff.h"       // File system related
#include "ui/ui_hex.h" // Hex display related
#include "ui/ui_progress_indicator.h" // Progress indicator related
#include "pirate/file.h" // File handling related
#include "pirate/hwi2c_pio.h"

#define I2C_DUMP_MAX_BUFFER_SIZE 1024

static const char* const usage[] = {
    "i2c [dump|read]\r\n\t[-a <7 bit i2c address>] [-w <register width>] [-r <register address>] [-b <bytes>] [-f <file>] [-h(elp)]",
    "Dump 16 bytes from device:%s i2c dump -a 0x50 -w 1 -r 0x00 -b 16",
    "Read 256 bytes to file:%s i2c read -a 0x50 -w 1 -r 0x00 -b 256 -f example.bin",
    "Dump device with 2 byte wide register:%s i2c dump -a 0x50 -w 2 -r 0x0000 -b 64",
    "Dump device with 3 bytes wide register:%s i2c dump -a 0x50 -w 3 -r 0x000000 -b 64",
};

enum i2c_dump_actions_enum {
    I2CDUMP_DUMP=0,
    I2CDUMP_READ,
};

static const bp_command_action_t i2c_dump_action_defs[] = {
    { I2CDUMP_DUMP, "dump", T_HELP_EEPROM_DUMP },
    { I2CDUMP_READ, "read", T_HELP_EEPROM_READ },
};

static const bp_command_opt_t i2c_dump_opts[] = {
    { "address",    'a', BP_ARG_REQUIRED, "7-bit", T_HELP_I2C_I2CDUMP_ADDRESS },
    { "regwidth",   'w', BP_ARG_REQUIRED, "bytes", T_HELP_I2C_I2CDUMP_REG_WIDTH },
    { "regaddr",    'r', BP_ARG_REQUIRED, "addr",  T_HELP_I2C_I2CDUMP_REG_ADDR },
    { "file",       'f', BP_ARG_REQUIRED, "file",  T_HELP_SLE4442_FILE_FLAG },
    { "bytes",      'b', BP_ARG_REQUIRED, "count", T_HELP_EEPROM_BYTES_FLAG },
    { "quiet",      'q', BP_ARG_NONE,     NULL,      UI_HEX_HELP_QUIET },
    { "nopager",    'c', BP_ARG_NONE,     NULL,      T_HELP_DISK_HEX_PAGER_OFF },
    { 0 }
};

const bp_command_def_t i2c_dump_def = {
    .name         = "i2c",
    .description  = T_HELP_I2C_I2CDUMP,
    .actions      = i2c_dump_action_defs,
    .action_count = count_of(i2c_dump_action_defs),
    .opts         = i2c_dump_opts,
    .usage        = usage,
    .usage_count  = count_of(usage),
};  

struct i2c_dump_t {
    uint32_t action; // action to perform
    uint8_t i2c_address_7bit; // 7-bit I2C address for the EEPROM
    uint32_t register_address_width; // width of the register address in bytes
    uint32_t register_address; // register address to start dumping from
    uint32_t data_size_bytes;
    uint8_t file_name[13]; // file name to read/write/verify
};

static bool i2c_get_args(struct i2c_dump_t *args) {
    
    // common function to parse the command line verb or action
    if(!bp_cmd_get_action(&i2c_dump_def, &args->action)){
        bp_cmd_help_show(&i2c_dump_def);
        return true;
    }
    
    uint32_t i2c_address;
    if(!bp_cmd_get_uint32(&i2c_dump_def, 'a', &i2c_address)) {
        //printf("Missing I2C 7 bit address: -a <address>\r\n");
        printf("Using default I2C address: 0x50\r\n");
        i2c_address = 0x50; // default I2C address
    }

    if (i2c_address > 0x7F) {
        printf("Invalid I2C address: %d\r\nSpecify a 7 bit I2C address\r\n", i2c_address);
        return true; // error
    }
    args->i2c_address_7bit= i2c_address; // set the device address

    if(!bp_cmd_get_uint32(&i2c_dump_def, 'w', &args->register_address_width)) {
        //printf("Missing register width: -w <reg. width>\r\n");
        printf("Using default register width: 1 byte\r\n");
        args->register_address_width = 1; // default register width
    }

    if(!bp_cmd_get_uint32(&i2c_dump_def, 'r', &args->register_address)) {
        //printf("Missing register address: -r <reg. address>\r\n");
        printf("Using default register address: 0x00\r\n");
        args->register_address = 0; // default register address
    }

    if(!bp_cmd_get_uint32(&i2c_dump_def, 'b', &args->data_size_bytes)) {
        //printf("Missing byte length: -b <bytes>\r\n");
        printf("Using default bytes: 16 bytes\r\n");
        args->data_size_bytes = 16; // default data size
    }

    // file to read/write/verify
    if ((args->action == I2CDUMP_READ)) {
        if(!bp_file_get_name_flag(&i2c_dump_def, 'f', args->file_name, sizeof(args->file_name))) return true;
        if(args->data_size_bytes > I2C_DUMP_MAX_BUFFER_SIZE){
            printf("Error: Data size exceeds maximum buffer size (%d bytes)\r\n", I2C_DUMP_MAX_BUFFER_SIZE);
            return true;
        }
    }

    return false;
}

bool i2c_dump(struct i2c_dump_t *eeprom){
    // align the start address to 16 bytes, and calculate the end address
    struct hex_config_t hex_config;
    hex_config.max_size_bytes= 0xffffffff; // maximum size of the device in bytes
    ui_hex_get_args_config(&i2c_dump_def, &hex_config);
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



bool i2c_dump_file(struct i2c_dump_t *eeprom){
    uint8_t buf[I2C_DUMP_MAX_BUFFER_SIZE];

    // copy the start address to an array
    uint8_t j =0;
    for (int16_t cnt = eeprom->register_address_width - 1; cnt >= 0; cnt--) {
        buf[j]= (eeprom->register_address >> (cnt * 8)) & 0xFF; // write the register address byte by byte
        j++;
     }
    //printf("Buf: %02X %02X\r\n", buf[0], buf[1]); // print the first byte of the buffer
    // read the data from the EEPROM
    if (i2c_transaction((eeprom->i2c_address_7bit<<1) | 0, buf, eeprom->register_address_width, buf, eeprom->data_size_bytes)) {
        return true; // error
    }

    printf("Writing %d bytes to file: %s\r\n", eeprom->data_size_bytes, eeprom->file_name);
    FIL file_handle;                                                  // file handle
    if(file_open(&file_handle, eeprom->file_name, FA_CREATE_ALWAYS | FA_WRITE)) return true; // create the file, overwrite if it exists

    // write the data to the file
    if(file_write(&file_handle, buf, eeprom->data_size_bytes)) { 
        return true; // if the write was unsuccessful (file closed in lower layer)
    }
    // close the file
    f_close(&file_handle); // close the file
    printf("Success :)\r\n");

    return false;
}

void i2c_dump_handler(struct command_result* res) {
    if(bp_cmd_help_check(&i2c_dump_def, res->help_flag)) {
        return;
    }
    
    struct i2c_dump_t eeprom;
    if(i2c_get_args(&eeprom)) {       
        return;
    }
    //we manually control any FALA capture
    fala_start_hook(); 
    
    #if 0
    if(eeprom.action == I2CDUMP_PROTECT){
        eeprom_probe_block_protect(&eeprom);
        goto i2c_dump_cleanup;
    }
    #endif

    if(eeprom.action == I2CDUMP_DUMP) {
        //dump the EEPROM contents
        i2c_dump(&eeprom);
        goto i2c_dump_cleanup; // no need to continue
    }
    if (eeprom.action==I2CDUMP_READ) {
        if(i2c_dump_file(&eeprom)) {
            printf("Error during I2C read\r\n");
            goto i2c_dump_cleanup; // error during read
        }
    }
    
 #if 0
    if (eeprom.action == I2CDUMP_ERASE || eeprom.action == I2CDUMP_TEST) {
        if(eeprom_action_erase(&eeprom, buf, sizeof(buf), verify_buf, sizeof(verify_buf), eeprom.verify_flag || eeprom.action == I2CDUMP_TEST)) {
            goto i2c_dump_cleanup; // error during erase
        }
    }

    if (eeprom.action == I2CDUMP_TEST) {
        if(eeprom_action_test(&eeprom, buf, sizeof(buf), verify_buf, sizeof(verify_buf))) {
            goto i2c_dump_cleanup; // error during test
        }
    }

    if (eeprom.action==I2CDUMP_WRITE) {
        if(eeprom_action_write(&eeprom, buf, sizeof(buf), verify_buf, sizeof(verify_buf), eeprom.verify_flag)) {
            goto i2c_dump_cleanup; // error during write
        }
    }

    if (eeprom.action==I2CDUMP_READ) {
        if(eeprom_action_read(&eeprom, buf, sizeof(buf), verify_buf, sizeof(verify_buf), eeprom.verify_flag)) {
            goto i2c_dump_cleanup; // error during read
        }
    }

    if (eeprom.action==I2CDUMP_VERIFY) {
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
