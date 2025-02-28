// TODO: BIO use, pullups, psu
/*
    Welcome to dummy.c, a growing demonstration of how to add commands to the Bus Pirate firmware.
    You can also use this file as the basis for your own commands.
    Type "dummy" at the Bus Pirate prompt to see the output of this command
    Temporary info available at https://forum.buspirate.com/t/command-line-parser-for-developers/235
*/
#include <stdio.h>
#include <string.h>
#include "pico/stdlib.h"
#include "pirate.h"
#include "command_struct.h"       // File system related
#include "fatfs/ff.h"       // File system related
#include "pirate/storage.h" // File system related
#include "ui/ui_cmdln.h"    // This file is needed for the command line parsing functions
// #include "ui/ui_prompt.h" // User prompts and menu system
// #include "ui/ui_const.h"  // Constants and strings
#include "ui/ui_help.h"    // Functions to display help in a standardized way
#include "system_config.h" // Stores current Bus Pirate system configuration
#include "pirate/amux.h"   // Analog voltage measurement functions
#include "pirate/button.h" // Button press functions
#include "msc_disk.h"
#include "pico/bootrom.h"
#include "otp/bp_otp.h"
#include "ui/ui_term.h"

// This array of strings is used to display help USAGE examples for the dummy command
static const char* const usage[] = { "dummy [init|test]\r\n\t[-b(utton)] [-i(nteger) <value>] [-f <file>]",
                                     "Initialize: dummy init",
                                     "Test: dummy test",
                                     "Test, require button press: dummy test -b",
                                     "Integer, value required: dummy -i 123",
                                     "Create/write/read file: dummy -f dummy.txt",
                                     "Kitchen sink: dummy test -b -i 123 -f dummy.txt" };

// This is a struct of help strings for each option/flag/variable the command accepts
// Record type 1 is a section header
// Record type 0 is a help item displayed as: "command" "help text"
// This system uses the T_ constants defined in translation/ to display the help text in the user's preferred language
// To add a new T_ constant:
//      1. open the master translation en-us.h
//      2. add a T_ tag and the help text
//      3. Run json2h.py, which will rebuild the translation files, adding defaults where translations are missing
//      values
//      4. Use the new T_ constant in the help text for the command
static const struct ui_help_options options[] = {
    { 1, "", T_HELP_DUMMY_COMMANDS },    // section heading
    { 0, "init", T_HELP_DUMMY_INIT },    // init is an example we'll find by position
    { 0, "test", T_HELP_DUMMY_TEST },    // test is an example we'll find by position
    { 1, "", T_HELP_DUMMY_FLAGS },       // section heading for flags
    { 0, "-b", T_HELP_DUMMY_B_FLAG },    //-a flag, with no optional string or integer
    { 0, "-i", T_HELP_DUMMY_I_FLAG },    //-b flag, with optional integer
    { 0, "-f", T_HELP_DUMMY_FILE_FLAG }, //-f flag, a file name string
};

// Polynomial: 0x8005 (x^16 + x^15 + x^2 + 1)
#define POLYNOMIAL 0x8005

uint16_t crc16(const unsigned char *data, size_t length) {
    uint16_t crc = 0xFFFF; // Initial value

    for (size_t i = 0; i < length; i++) {
        crc ^= (uint16_t)data[i] << 8;
        for (uint8_t j = 0; j < 8; j++) {
            if (crc & 0x8000) {
                crc = (crc << 1) ^ POLYNOMIAL;
            } else {
                crc <<= 1;
            }
        }
    }

    return crc;
}

bool verify_crc16(const unsigned char *data, size_t length, uint16_t expected_crc) {
    uint16_t calculated_crc = crc16(data, length);
    return calculated_crc == expected_crc;
}

enum {
    OTP_DIRECTORY_ITEM_TYPE_END = 0x0000,
    OTP_DIRECTORY_ITEM_TYPE_USB_WHITELABEL,
    OTP_DIRECTORY_ITEM_TYPE_CERT,
    OTP_DIRECTORY_ITEM_TYPE_DEVICE_INFO,
};

typedef struct _OTP_DIRECTORY_ITEM {
    uint16_t EntryType; // with 0x0000 defined as "end of list"
    uint16_t StartRow;  // row where that entry is stored
    uint16_t RowCount;  // count of consecutive rows
    uint16_t CRC16; // Validates the prior three entries.
} OTP_DIRECTORY_ITEM;

bool otp_directory_entry_read(uint16_t type, OTP_DIRECTORY_ITEM *directory_item) {
    uint32_t result;
    otp_cmd_t cmd;

    //start from the back, read 4 rows at a time until we find the entry type
    //uint16_t directory_row = 0xf7c;
    uint16_t directory_row = 0xf60 - 4; //0xf7c;
    while(true){
        OTP_DIRECTORY_ITEM existing_directory_item;
        cmd.flags = directory_row | OTP_CMD_ECC_BITS;
        result = rom_func_otp_access((uint8_t*)&existing_directory_item, sizeof(existing_directory_item), cmd);
        if (result != 0){
            printf("Failed to read directory item at row %03x\r\n", directory_row);
            return false;
        }

        //did we find the entry type?
        if(existing_directory_item.EntryType == type){
            printf("Found directory item, type %d, row %03X, length %03X\r\n", existing_directory_item.EntryType, existing_directory_item.StartRow, existing_directory_item.RowCount);
            //crc check
            if(!verify_crc16((unsigned char*)&existing_directory_item, sizeof(existing_directory_item)-2, existing_directory_item.CRC16)){
                printf("Failed to verify directory item CRC\r\n");
                return false;
            }
            memcpy(directory_item, &existing_directory_item, sizeof(existing_directory_item));
            return true;
        }

        //did we find an empty row?
        if(existing_directory_item.EntryType == OTP_DIRECTORY_ITEM_TYPE_END){
            printf("No directory item found, type %d\r\n", type);
            return false;
        }

        //for now we have max 25 directory items
        if(directory_row <= 0xf7c - (25*4)){
            printf("No directory item found, type %d\r\n", type);
            return false;
        }

        directory_row -= 4;
    }

    return false;
}

bool otp_directory_entry_write(uint16_t type, uint16_t start_row, uint16_t row_count) {
    uint32_t result;
    otp_cmd_t cmd;

    //start from end record, read 4 rows at a time until we find an empty row set
    uint16_t directory_row = 0xf60 - 4; //0xf7c;
    while(true){
        OTP_DIRECTORY_ITEM existing_directory_item;
        cmd.flags = directory_row | OTP_CMD_ECC_BITS;
        result = rom_func_otp_access((uint8_t*)&existing_directory_item, sizeof(existing_directory_item), cmd);
        if (result != 0){
            printf("Failed to read directory item at row %03x\r\n", directory_row);
            return false;
        }

        //did we find an empty row?
        if(existing_directory_item.EntryType == OTP_DIRECTORY_ITEM_TYPE_END){
            printf("Found empty directory row at %03x\r\n", directory_row);
            break;
        }

        //for now we have max 25 directory items
        if(directory_row <= 0xf7c - (25*4)){
            printf("No empty directory row found\r\n");
            return false;
        }

        directory_row -= 4;
    }

    //write the new directory item
    OTP_DIRECTORY_ITEM new_directory_item = {.EntryType=type, .StartRow=start_row, .RowCount=row_count};
    new_directory_item.CRC16 = crc16((unsigned char*)&new_directory_item, sizeof(new_directory_item)-2);

    cmd.flags = directory_row | OTP_CMD_WRITE_BITS | OTP_CMD_ECC_BITS;
    result = rom_func_otp_access((uint8_t*)&new_directory_item, sizeof(new_directory_item), cmd);
    if (result != 0){
        printf("Failed to write directory item at row %03x\r\n", directory_row);
        return false;
    }

    //verify the write
    OTP_DIRECTORY_ITEM read_directory_item;
    cmd.flags = directory_row | OTP_CMD_ECC_BITS;
    result = rom_func_otp_access((uint8_t*)&read_directory_item, sizeof(read_directory_item), cmd);
    if (result != 0){
        printf("Failed to read back directory item at row %03x\r\n", directory_row);
        return false;
    }

    if(memcmp(&new_directory_item, &read_directory_item, sizeof(new_directory_item)) != 0){
        printf("Failed to verify directory item at row %03x\r\n", directory_row);
        return false;
    }

    return true;
}

void otp_handler(struct command_result* res) {
    uint32_t value; // somewhere to keep an integer value
    char file[13];  // somewhere to keep a string value (8.3 filename + 0x00 = 13 characters max)

    // the help -h flag can be serviced by the command line parser automatically, or from within the command
    // the action taken is set by the help_text variable of the command struct entry for this command
    // 1. a single T_ constant help entry assigned in the commands[] struct in commands.c will be shown automatically
    // 2. if the help assignment in commands[] struct is 0x00, it can be handled here (or ignored)
    // res.help_flag is set by the command line parser if the user enters -h
    // we can use the ui_help_show function to display the help text we configured above
    if (ui_help_show(res->help_flag, usage, count_of(usage), &options[0], count_of(options))) {
        return;
    }

    OTP_DIRECTORY_ITEM directory_item[] = {
        {OTP_DIRECTORY_ITEM_TYPE_CERT, 0x100, 7*64, 0},
        {OTP_DIRECTORY_ITEM_TYPE_DEVICE_INFO, 0x2c0, 1*64, 0},
        {OTP_DIRECTORY_ITEM_TYPE_USB_WHITELABEL, 0xd0, 1*64, 0},
    };    

    for(uint32_t i=0; i < count_of(directory_item); i++){
        //read the directory item - does it exist?
        OTP_DIRECTORY_ITEM existing_directory_item;
        if(otp_directory_entry_read(directory_item[i].EntryType, &existing_directory_item)){
            printf("Found existing directory item, type %d, row %03X, length %03X\r\n", existing_directory_item.EntryType, existing_directory_item.StartRow, existing_directory_item.RowCount);
            continue;
        }else{
            printf("No existing directory item found, type %d\r\n", directory_item[i].EntryType);
            if(!otp_directory_entry_write(directory_item[i].EntryType, directory_item[i].StartRow, directory_item[i].RowCount)){
                printf("Failed to write directory item\r\n");
            }else{
                printf("Directory item written\r\n");
            }
        }
    }

    return;

    const char manuf_string_01[] = "Bus Pirate";
    const char manuf_string_02[] = "6";
    const char manuf_string_03[] = "2";
    const char manuf_string_04[] = "2025-2-16 16:39";
    const char manuf_string_05[] = "Shenzhen China";

    struct {
        uint8_t type;
        uint8_t length;
        const char *data;
    } manuf_strings[] = { //includes trailing 0x00
        {0x01, sizeof(manuf_string_01), manuf_string_01},
        {0x02, sizeof(manuf_string_02), manuf_string_02},
        {0x03, sizeof(manuf_string_03), manuf_string_03},
        {0x04, sizeof(manuf_string_04), manuf_string_04},
        {0x05, sizeof(manuf_string_05), manuf_string_05},
    };
  
    return; 
#if 0
  if(cmdln_args_find_flag('m')){
        /*pico_unique_board_id_t id;
        pico_get_unique_board_id(&id); 
        //convert the unique ID to ANSI string
        snprintf(buf, sizeof(buf), "%02X:%02X:%02X:%02X:%02X:%02X:%02X:%02X", 
            id.id[0], id.id[1], id.id[2], id.id[3], id.id[4], id.id[5], id.id[6], id.id[7]);
        printf("Manufacturing ID string: %s\r\n", buf);
        if(!bp_otp_apply_manufacturing_string(buf)){
            printf("Failed to apply manufacturing string\r\n");
        }else{
            printf("Manufacturing string applied, OTP locked\r\n");
        }*/


    // if we only want to run this command in a certain mode
    //  we can use the system_config variable to determine the mode
    //  modes are the menu option number minus 1
    //  the best way to prevent a command from running in HiZ mode is configure the commands[] struct in commands.c
    //  NOTE: mode commands are a better way to do this now.
    //  mode commands are registered in a mode and are only accessible from there
    /*if(system_config.mode!=4)
    {
        printf("dummy command is currently only available in SPI mode\r\n");
        return;
    }*/
    // to make things easier we'll comment the above lines out and just print the mode ID
    printf("Current mode: %d\r\n", system_config.mode); // print the current mode

    // ui_help_check_vout_vref() checks for a valid voltage on the Vout pin
    // if the voltage is too low, the command print an error message and return false
    // checking for a voltage, if needed, and issuing an error or reminder saves a lot of frustration for everyone
    /*
    if(!ui_help_check_vout_vref())
    {
        return;
    }
    */
    // to make things easier we'll just print the status of the Vout pin
    if (!ui_help_check_vout_vref()) {
        printf("Waring: Vout pin is not connected to a valid voltage source\r\n");
    } else {
        printf("Vout pin is connected to a valid voltage source\r\n");
    }

    // you can have multiple parameters in multiple positions
    // we have two possible parameters: init and test, which
    // our parameter is the first argument following the command itself, but you can use it however works best
    char action_str[9];              // somewhere to store the parameter string
    bool init = false, test = false; // some flags to keep track of what we want to do

    // use the cmdln_args_string_by_position function to get the parameter by position
    // Position:    0   1   2   3
    // Command:  dummy  init
    // in this case position 1, which is the first parameter after the command
    // the function returns true if a parameter is present at position 1, and false if it's not
    if (cmdln_args_string_by_position(1, sizeof(action_str), action_str)) {
        if (strcmp(action_str, "init") == 0) {
            init = true;
        }
        if (strcmp(action_str, "test") == 0) {
            test = true;
        }
        printf("Parameter 1: %s, init=%s, test=%s\r\n",
               action_str,
               (init ? "true" : "false"),
               (test ? "true" : "false")); // print the parameter we found
    } else {
        printf("No parameter found at position 1\r\n");
    }

    //-b is a flag without an additional parameters
    bool b_flag = cmdln_args_find_flag('b');
    printf("Flag -b is %s\r\n", (b_flag ? "set" : "not set"));
    if (b_flag) { // press bus pirate button to continue
        printf("Press Bus Pirate button to continue\r\n");
        // using the polling method to wait for the button press
        while (!button_get(0)) {
            tight_loop_contents();
        }
        printf("Button pressed\r\n");
    }

    //-i is a flag with an additional integer parameter
    // check if a flag is present and get the integer value
    // returns true if flag is present AND has an integer value
    // if the user entered a string value, it generally also fail with false
    command_var_t arg; // this struct will contain additional information about the integer
                       // such as the format the user enter, so we can react differently to DEC/HEX/BIN formats
                       //  it also helps future proof code because we can add variables later without reworking every
                       //  place the function is used
    // check for the -i flag with an integer value
    bool i_flag = cmdln_args_find_flag_uint32('i', &arg, &value);
    // if the flag is set, print the value
    if (i_flag) {
        printf("Flag -i is set with value %d, entry format %d\r\n", value, arg.number_format);
    } else { // error parsing flag and value
        // we can test the has_arg flag to see if the user entered a flag/arg but no valid integer value
        if (arg.has_arg) { // entered the flag/arg but no valid integer value
            printf("Flag -i is set with no or invalid integer value. Try -i 0\r\n");
            // setting a system config error flag will
            // effect if the next commands chained with || or && will run
            system_config.error = true; // set the error flag
            return;
        } else { // flag/arg not entered
            printf("Flag -i is not set\r\n");
        }
    }

    // -f is a flag with an additional string parameter
    // we'll get a file name, create it, write to it, close it, open it, and print the contents
    bool f_flag = cmdln_args_find_flag_string('f', &arg, sizeof(file), file);

    if (!f_flag && arg.has_arg) { // entered the flag/arg but no valid string value
        printf("Flag -f is set with no or invalid file name. Try -f dummy.txt\r\n");
        system_config.error = true; // set the error flag
        return;
    } else if (!f_flag && !arg.has_arg) { // flag/arg not entered
        printf("Flag -f is not set\r\n");
    } else if (f_flag) {
        printf("Flag -f is set with file name %s\r\n", file);

        /* First create a file and write to it */
        // create a file
        printf("Creating file %s\r\n", file);
        FIL file_handle;                                                  // file handle
        FRESULT result;                                                   // file system result
        result = f_open(&file_handle, file, FA_CREATE_ALWAYS | FA_WRITE); // create the file, overwrite if it exists
        if (result != FR_OK) {                                            // error
            printf("Error creating file %s\r\n", file);
            system_config.error = true; // set the error flag
            return;
        }
        // if the file was created
        printf("File %s created\r\n", file);

        // write to the file
        char buffer[256] =
            "This is a test file created by the dummy command"; // somewhere to store the data we want to write
        printf("Writing to file %s: %s\r\n", file, buffer);
        UINT bytes_written; // somewhere to store the number of bytes written
        result = f_write(&file_handle, buffer, strlen(buffer), &bytes_written); // write the data to the file
        if (result != FR_OK) {
            printf("Error writing to file %s\r\n", file);
            FRESULT result2 = f_close(&file_handle); // close the file
            if (result2 != FR_OK) {
                printf("Error closing file %s after error writing to file -- reboot recommended\r\n", file);
            }
            system_config.error = true; // set the error flag
            return;
        }
        // if the write was successful
        printf("Wrote %d bytes to file %s\r\n", bytes_written, file);

        // close the file
        result = f_close(&file_handle); // close the file
        if (result != FR_OK) {
            printf("Error closing file %s\r\n", file);
            system_config.error = true; // set the error flag
            return;
        }
        // if the file was closed
        printf("File %s closed\r\n", file);
        // make the file available to the host
        // refresh_usbmsdrive();

        /* Open file and read */
        // open the file
        result = f_open(&file_handle, file, FA_READ); // open the file for reading
        if (result != FR_OK) {
            printf("Error opening file %s for reading\r\n", file);
            system_config.error = true; // set the error flag
            return;
        }
        // if the file was opened
        printf("File %s opened for reading\r\n", file);

        // read the file
        UINT bytes_read; // somewhere to store the number of bytes read
        result = f_read(&file_handle, buffer, sizeof(buffer), &bytes_read); // read the data from the file
        if (result == FR_OK) {                                              // if the read was successful
            printf("Read %d bytes from file %s\r\n", bytes_read, file);
            printf("File contents: %s\r\n", buffer); // print the file contents
        } else {                                     // error reading file
            printf("Error reading file %s\r\n", file);
            system_config.error = true; // set the error flag
        }

        // close the file
        result = f_close(&file_handle); // close the file
        if (result != FR_OK) {
            printf("Error closing file %s\r\n", file);
            system_config.error = true; // set the error flag
            return;
        }
        // if the file was closed
        printf("File %s closed\r\n", file);

        printf("Hint: use the ls and check that %s is in the list of files\r\n", file);
        printf("Hint: use the cat %s to print the contents\r\n", file);
        printf("Hint: use the rm %s to delete\r\n", file);
    }

    // bonus: you might notice we are handed a struct of type command_result from the command parser
    //  this is a way to return errors and other (future) data
    // I'm not sure it actually does anything right now, but it's there for future use
    // command_result.error=true; //set the error flag

    // to set an error back to the command line parser and indicate an error for chaining purposes (; || &&)
    // system_config.error=true; //set the error flag

    // Other things to add:
    //  voltage and current measurement
    //  pull-up resistors
    //  vreg (voltage/current)
    //  LEDs
    //  buffered IO pins
    // prompt and parse/menus

    printf("dummy command complete\r\n");
#endif
}