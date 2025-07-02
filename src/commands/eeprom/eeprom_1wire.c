/*
MIT License

Some defines, and portions of 'ow_eeprom_read' and 'ow_eeprom_write' functions based on: https://github.com/tommag/DS2431_Arduino/tree/master

Copyright (c) 2017 Tom Magnier
Modified 2018 by Nicolò Veronese
Modified 2025 by Ian Lesnet for Bus Pirate 5+ firmware

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.
 */
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
//#include "ui/ui_progress_indicator.h" // Progress indicator related
//#include "commands/i2c/eeprom.h"
#include "pirate/file.h" // File handling related
#include "hardware/pio.h"
#include "pirate/hw1wire_pio.h" // SPI related functions
#include "eeprom_base.h"
#include "pirate/hwspi.h" // SPI related functions

#define SPI_EEPROM_READ_CMD 0x03 // Read command for EEPROM
#define SPI_EEPROM_WRITE_CMD 0x02 // Write command for EEPROM   
#define SPI_EEPROM_WRDI_CMD 0x04 // Write Disable command for EEPROM
#define SPI_EEPROM_WREN_CMD 0x06 // Write Enable command for EEPROM
#define SPI_EEPROM_RDSR_CMD 0x05 // Read Status Register command for EEPROM
#define SPI_EEPROM_WRSR_CMD 0x01 // Write Status Register command

#define OW_SKIP_ROM_CMD 0xCC // Skip ROM command for 1-Wire

#define DS243X_WRITE_SCRATCHPAD 0x0F
#define DS243X_READ_SCRATCHPAD 0xAA
#define DS243X_COPY_SCRATCHPAD 0x55
#define DS243X_READ_MEMORY 0xF0

#define DS243X_PF_MASK 0x07
#define DS243X_WRITE_MASK 0xAA    
#define DS243X_CMD_SIZE 3
#define DS243X_CRC_SIZE 2
#define DS243X_MAX_PAGE_SIZE 32 //DS2433 has 32 bit page
#define DS243X_BUFFER_SIZE (DS243X_CMD_SIZE + DS243X_MAX_PAGE_SIZE)

enum eeprom_actions_enum {
    EEPROM_DUMP=0,
    EEPROM_ERASE,
    EEPROM_WRITE,
    EEPROM_READ,
    EEPROM_VERIFY,
    EEPROM_TEST,
    EEPROM_LIST,
    EEPROM_PROTECT
};

static const struct cmdln_action_t eeprom_actions[] = {
    { EEPROM_DUMP, "dump" },
    { EEPROM_ERASE, "erase" },
    { EEPROM_WRITE, "write" },
    { EEPROM_READ, "read" },
    { EEPROM_VERIFY, "verify" },
    { EEPROM_TEST, "test" },
    { EEPROM_LIST, "list"  },
    { EEPROM_PROTECT, "protect"}
};

static const struct eeprom_device_t eeprom_devices[] = {
    { "DS2431",    128,     2, 0, 0,   8, 16 }, 
    { "DS2433",    512,     2, 0, 0,   32, 16 },
};

static const char* const usage[] = {
    "eeprom [dump|erase|write|read|verify|test|list|protect]\r\n\t[-d <device>] [-f <file>] [-v(verify)] [-s <start address>] [-b <bytes>] [-h(elp)]",
    "List available EEPROM devices:%s eeprom list",
    "Display contents:%s eeprom dump -d ds2431",
    "Display 16 bytes starting at address 0x10:%s eeprom dump -d ds2431 -s 0x10 -b 16",
    "Erase, verify:%s eeprom erase -d ds2431 -v",
    "Write from file, verify:%s eeprom write -d ds2431 -f example.bin -v",
    "Read to file, verify:%s eeprom read -d ds2431 -f example.bin -v",
    "Verify against file:%s eeprom verify -d ds2431 -f example.bin",
    "Test chip (full erase/write/verify):%s eeprom test -d ds2431",
    "Probe protection control block:%s eeprom protect -d ds2431",
};

static const struct ui_help_options options[] = {
    { 1, "", T_HELP_1WIRE_EEPROM },               // command help
    { 0, "dump", T_HELP_EEPROM_DUMP },  
    { 0, "erase", T_HELP_EEPROM_ERASE },    // erase
    { 0, "write", T_HELP_EEPROM_WRITE },    // write
    { 0, "read", T_HELP_EEPROM_READ },      // read
    { 0, "verify", T_HELP_EEPROM_VERIFY },  // verify
    { 0, "test", T_HELP_EEPROM_TEST },      // test
    { 0, "list", T_HELP_EEPROM_LIST},      // list devices
    //{ 0, "protect", T_HELP_EEPROM_PROTECT }, // protect
    { 0, "-f", T_HELP_EEPROM_FILE_FLAG },   // file to read/write/verify
    { 0, "-v", T_HELP_EEPROM_VERIFY_FLAG }, // with verify (after write)
    { 0, "-s", T_HELP_EEPROM_START_FLAG },  // start address for dump/read/write
    { 0, "-b", T_HELP_EEPROM_BYTES_FLAG },  // bytes to dump/read/write
    { 0, "-h", T_HELP_FLAG },   // help
};  //protect, -p, -t, -w?

//-----------------------------------------------------------------------
// 1-Wire EEPROM hardware abstraction layer functions
//-----------------------------------------------------------------------
// Calculate CRC16 for DS2431 (polynomial 0x8005, initial value 0, inverted output)
uint16_t ds2431_crc16(const uint8_t *data, uint16_t len) {
    uint16_t crc = 0;
    for (uint16_t i = 0; i < len; i++) {
        crc ^= (uint16_t)data[i];
        for (uint8_t j = 0; j < 8; j++) {
            if (crc & 1) {
                crc = (crc >> 1) ^ 0xA001; // 0xA001 is the reflected 0x8005
            } else {
                crc >>= 1;
            }
        }
    }
    return ~crc; // Invert the result as required by DS2431
}

static bool ow_eeprom_read(struct eeprom_info *eeprom, uint32_t address, uint32_t read_bytes, uint8_t *buf) {
    // get the address for the current byte
    uint8_t block_select_bits = 0;
    uint8_t address_array[3];
    if(eeprom->hal->get_address(eeprom, address, &block_select_bits, address_array)){ 
        return true; // error getting address
    }

    // read the data from the EEPROM
    if(!onewire_reset()) {
        printf("Error: 1-Wire device not responding.\r\n");
        return true; // 1-Wire bus not responding
    }
    onewire_tx_byte(OW_SKIP_ROM_CMD);   // Skip ROM command
    onewire_tx_byte(DS243X_READ_MEMORY); // send the read memory command
    onewire_tx_byte(address_array[1]); // send the low byte of the address
    onewire_tx_byte(address_array[0]); // send the high byte of the address
    for (uint32_t i = 0; i < read_bytes; i++) {
        buf[i] = onewire_rx_byte(); // read the data byte by byte
    }

    return false;
}

static bool ow_eeprom_write_page(struct eeprom_info *eeprom, uint32_t address, uint8_t *buf){

    //get address
    uint8_t block_select_bits = 0;
    uint8_t address_array[3];
    if(eeprom->hal->get_address(eeprom, address, &block_select_bits, address_array))return true; // get the address  

    uint8_t buffer[DS243X_BUFFER_SIZE];
    uint8_t crc16[DS243X_CRC_SIZE];

    // write scratchpad with page of data
    buffer[0] = DS243X_WRITE_SCRATCHPAD; // Command to write scratchpad
    buffer[1] = address_array[1]; // Low byte of address
    buffer[2] = address_array[0]; // High byte of address
    memcpy(&buffer[DS243X_CMD_SIZE], buf, eeprom->device->page_bytes); // Copy the page data to the buffer

    if(!onewire_reset()) {
        printf("Error: 1-Wire device not responding.\r\n");
        return true; // 1-Wire bus not responding
    }
    onewire_tx_byte(OW_SKIP_ROM_CMD);   // Skip ROM command
    for(uint32_t i = 0; i < (DS243X_CMD_SIZE + eeprom->device->page_bytes); i++) {
        onewire_tx_byte(buffer[i]); // write the page data byte by byte
    } 
    crc16[0]= onewire_rx_byte(); // read the CRC-16 byte 1
    crc16[1]= onewire_rx_byte(); // read the CRC-16 byte 2

    // Calculate CRC16 for the buffer
    uint16_t calculated_crc = ds2431_crc16(buffer, DS243X_CMD_SIZE + eeprom->device->page_bytes);
    // Check if the calculated CRC matches the received CRC
    if (calculated_crc != ((crc16[1] << 8) | crc16[0])) {
        printf("Error: CRC mismatch, expected 0x%04X, got 0x%04X\r\n", calculated_crc, (crc16[1] << 8) | crc16[0]);
        return true; // CRC mismatch
    }

    // Read verification
    uint8_t TA1, TA2, ES;
    //Read scratchpad to compare with the data sent
    if(!onewire_reset()) {
        printf("Error: 1-Wire device not responding.\r\n");
        return true; // 1-Wire bus not responding
    }
    onewire_tx_byte(OW_SKIP_ROM_CMD);   // Skip ROM command
    onewire_tx_byte(DS243X_READ_SCRATCHPAD);   // read Scatchpad
    TA1 = onewire_rx_byte(); // Read TA1
    TA2 = onewire_rx_byte(); // Read TA2
    ES = onewire_rx_byte(); // Read E/S
    if(ES != DS243X_PF_MASK){
        printf("Error: E/S not matching, expected 0x%02X, got 0x%02X\r\n", DS243X_PF_MASK, ES);
        return true; // E/S not matching
    }

    //Copy scratchpad
    if(!onewire_reset()) {
        printf("Error: 1-Wire device not responding.\r\n");
        return true; // 1-Wire bus not responding
    }
    onewire_tx_byte(OW_SKIP_ROM_CMD);   // Skip ROM command
    onewire_tx_byte(DS243X_COPY_SCRATCHPAD); // Send the copy scratchpad command
    onewire_tx_byte(TA1); //Send authorization code (TA1, TA2, E/S)
    onewire_tx_byte(TA2); // Send TA2
    onewire_tx_byte(ES); // Send E/S (DS2431_PF_MASK)
    // Wait for the write to complete
    busy_wait_ms(15); // t_PROG = 12.5ms worst case.
    uint8_t write_result = onewire_rx_byte(); // Read the result of the write operation
    // Check if the write was successful
    if (write_result != DS243X_WRITE_MASK) {
        printf("Error: Write operation failed, expected 0x%02X, got 0x%02X\r\n", DS243X_WRITE_MASK, write_result);
        return true; // Write operation failed
    }

    return false; // write is complete
}

static struct eeprom_hal_t ow_eeprom_hal = {
    .get_address = eeprom_get_address,
    .read = ow_eeprom_read,
    .write_page = ow_eeprom_write_page,
    .write_protection_blocks = NULL, // not implemented
};
//---------------------------------------------------------------------------

static void ow_eerpom_user_bytes_print(uint8_t *ub, uint32_t base_page) {
    // print the status register in a human readable format
    // ctrl[0] = 0x80, ctrl[1] = 0x81, ..., ctrl[7] = 0x87
    for (int page = 0; page < 4; page++) {
        printf("Page %d Protection (0x%03X): ", page, base_page + page);
        if (ub[page] == 0x55) {
            printf("Write Protected");
        } else if (ub[page] == 0xAA) {
            printf("EPROM Mode");
        } else {
            printf("Unprotected");
        }
        printf(" (0x%02X)\r\n", ub[page]);
    }

    // Copy protection byte
    printf("Copy Protection (0x%03X): ",  base_page + 4);
    if (ub[4] == 0x55 || ub[4] == 0xAA) {
        printf("Copy Protect 0x%03X-0x%03X and any write-protected pages", base_page, base_page + 0xf);
    } else {
        printf("Unprotected");
    }
    printf(" (0x%02X)\r\n", ub[4]);

    // Factory byte
    printf("Factory Byte (0x%03X): ", base_page + 5);
    if (ub[5] == 0xAA) {
        printf("Write Protect 0x%03X, 0x%03X, 0x%03X", base_page + 5, base_page + 6, base_page + 7);
    } else if (ub[5] == 0x55) {
        printf("Write Protect 0x%03X, Unprotect 0x%03X, 0x%03X", base_page + 5, base_page +6, base_page + 7);
    } else {
        printf("0x%02X (factory set)", ub[5]);
    }
    printf(" (0x%02X)\r\n", ub[5]);

    // User/manufacturer bytes
    printf("User/Manufacturer ID0 (0x%03X): 0x%02X\r\n", base_page + 6, ub[6]);
    printf("User/Manufacturer ID1 (0x%03X): 0x%02X\r\n", base_page + 7, ub[7]);
}

static bool eeprom_probe_block_protect(struct eeprom_info *eeprom) {   
    uint8_t user_buf[8];
    //DS243X has 8 bytes of protection and other registers. Read 8 bytes from end of EEPROM area
        // read the data from the EEPROM
    if(!onewire_reset()) {
        printf("Error: 1-Wire device not responding.\r\n");
        return true; // 1-Wire bus not responding
    }
    uint16_t address = eeprom->device->size_bytes; // address to read the protection area
    onewire_tx_byte(OW_SKIP_ROM_CMD);   // Skip ROM command
    onewire_tx_byte(DS243X_READ_MEMORY); // send the read memory command
    onewire_tx_byte(address & 0xFF); // send the low byte of the address
    onewire_tx_byte((address >> 8) & 0xFF); // send the high byte of the address
    for (uint32_t i = 0; i < 8; i++) {
        user_buf[i] = onewire_rx_byte(); // read the data byte by byte
    }
    ow_eerpom_user_bytes_print(user_buf, eeprom->device->size_bytes); // print the user bytes
    return false;
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
        eeprom_display_devices(eeprom_devices, count_of(eeprom_devices)); // display devices if list action
        printf("Compatible with common 1-Wire EEPROMs: DS/GX2431. 2433 untested!\r\n");
        printf("3.3volts is suitable for most devices.\r\n");
        return true; // no error, just listing devices
    }
    
    if(!cmdln_args_find_flag_string('d', &arg, sizeof(arg_str), arg_str)){
        printf("Missing EEPROM device name: -d <device name>\r\n");
        eeprom_display_devices(eeprom_devices, count_of(eeprom_devices));
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
        eeprom_display_devices(eeprom_devices, count_of(eeprom_devices));
        return true; // error
    }
 
    args->device = &eeprom_devices[eeprom_type];

    // verify_flag
    args->verify_flag = cmdln_args_find_flag('v' | 0x20);

    // test block protect bits
    //args->protect_test_flag = cmdln_args_find_flag('t' | 0x20);
    // program block protect bits
    #if 0
    args->protect_flag=cmdln_args_find_flag_uint32('p' | 0x20, &arg, &args->protect_bits);
    if(arg.error){
        printf("Specify block protect bits (0-3, 0b00-0b11)\r\n");
        return true;
    }
    if(args->protect_flag){
        if (args->protect_bits>=4) {
            printf("Block write protect bits out of range (0-3, 0b00-0b11 valid): %d\r\n", args->protect_bits);
            return true; // error
        }
    }
    #endif

    // file to read/write/verify
    if ((args->action == EEPROM_READ || args->action == EEPROM_WRITE || args->action==EEPROM_VERIFY)) {
        if(file_get_args(args->file_name, sizeof(args->file_name))) return true;
    }

    // let hex editor parse its own arguments (done in the dump function)
    //if(ui_hex_get_args(args->device->size_bytes, &args->start_address, &args->user_bytes)) return true;

    return false;
}

void onewire_eeprom_handler(struct command_result* res) {
    if(res->help_flag) {
        eeprom_display_devices(eeprom_devices, count_of(eeprom_devices)); // display the available EEPROM devices
        ui_help_show(true, usage, count_of(usage), &options[0], count_of(options)); // show help if requested
        return; // if help was shown, exit
    }
    struct eeprom_info eeprom;
    eeprom.hal = &ow_eeprom_hal; // set the HAL for EEPROM operations
    // bus specific arguments (action, protect blocks, etc)
    if(eeprom_get_args(&eeprom)) { 
        return;
    }

    //nice full description of chip and capabilities
    printf("%s: %d bytes,  %d block select bits, %d byte address, %d byte pages\r\n\r\n", eeprom.device->name, eeprom.device->size_bytes, eeprom.device->block_select_bits, eeprom.device->address_bytes, eeprom.device->page_bytes);

    char buf[EEPROM_ADDRESS_PAGE_SIZE]; // buffer for reading/writing
    uint8_t verify_buf[EEPROM_ADDRESS_PAGE_SIZE]; // buffer for reading data from EEPROM

    //we manually control any FALA capture
    fala_start_hook(); 

    if(eeprom.action == EEPROM_PROTECT){
        eeprom_probe_block_protect(&eeprom);
        goto ow_eeprom_cleanup;
    }

    if(eeprom.action == EEPROM_DUMP) {
        //dump the EEPROM contents
        eeprom_dump(&eeprom, buf, sizeof(buf));
        goto ow_eeprom_cleanup; // no need to continue
    }
 
    if (eeprom.action == EEPROM_ERASE || eeprom.action == EEPROM_TEST) {
        if(eeprom_action_erase(&eeprom, buf, sizeof(buf), verify_buf, sizeof(verify_buf), eeprom.verify_flag || eeprom.action == EEPROM_TEST)) {
            goto ow_eeprom_cleanup; // error during erase
        }
    }

    if (eeprom.action == EEPROM_TEST) {
        if(eeprom_action_test(&eeprom, buf, sizeof(buf), verify_buf, sizeof(verify_buf))) {
            goto ow_eeprom_cleanup; // error during test
        }
    }

    if (eeprom.action==EEPROM_WRITE) {
        if(eeprom_action_write(&eeprom, buf, sizeof(buf), verify_buf, sizeof(verify_buf), eeprom.verify_flag)) {
            goto ow_eeprom_cleanup; // error during write
        }
    }

    if (eeprom.action==EEPROM_READ) {
        if(eeprom_action_read(&eeprom, buf, sizeof(buf), verify_buf, sizeof(verify_buf), eeprom.verify_flag)) {
            goto ow_eeprom_cleanup; // error during read
        }
    }

    if (eeprom.action==EEPROM_VERIFY) {
        if(eeprom_action_verify(&eeprom, buf, sizeof(buf), verify_buf, sizeof(verify_buf))){
            goto ow_eeprom_cleanup; // error during verify
        }
    }
    printf("Success :)\r\n");

ow_eeprom_cleanup:
    //we manually control any FALA capture
    fala_stop_hook();
    fala_notify_hook();

}
