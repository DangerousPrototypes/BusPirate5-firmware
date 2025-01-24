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
//#include "fatfs/ff.h"       // File system related
//#include "pirate/storage.h" // File system related
#include "ui/ui_cmdln.h"    // This file is needed for the command line parsing functions
// #include "ui/ui_prompt.h" // User prompts and menu system
// #include "ui/ui_const.h"  // Constants and strings
#include "ui/ui_help.h"    // Functions to display help in a standardized way
#include "system_config.h" // Stores current Bus Pirate system configuration
//#include "pirate/amux.h"   // Analog voltage measurement functions
//#include "pirate/button.h" // Button press functions
//#include "msc_disk.h"
#include "pico/bootrom.h"
#include "hardware/structs/otp.h"

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
    /*{ 1, "", T_HELP_DUMMY_COMMANDS },    // section heading
    { 0, "init", T_HELP_DUMMY_INIT },    // init is an example we'll find by position
    { 0, "test", T_HELP_DUMMY_TEST },    // test is an example we'll find by position
    { 1, "", T_HELP_DUMMY_FLAGS },       // section heading for flags
    { 0, "-b", T_HELP_DUMMY_B_FLAG },    //-a flag, with no optional string or integer
    { 0, "-i", T_HELP_DUMMY_I_FLAG },    //-b flag, with optional integer*/
    { 0, "-f", T_HELP_DUMMY_FILE_FLAG }, //-f flag, a file name string
};

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

    otp_cmd_t cmd;
    int8_t ret;

    // Row to write ECC data
    uint16_t ecc_row = 0x400;
    // Row to write raw data
    uint16_t raw_row = 0x410;

    // Check rows are empty - else the rest of the tests won't behave as expected
    unsigned char initial_data[32] = {0};
    cmd.flags = ecc_row;
    ret = rom_func_otp_access(initial_data, sizeof(initial_data)/2, cmd);
    if (ret) {
        printf("ERROR: Initial ECC Row Read failed with error %d\r\n", ret);
    }
    cmd.flags = raw_row;
    ret = rom_func_otp_access(initial_data+(sizeof(initial_data)/2), sizeof(initial_data)/2, cmd);
    if (ret) {
        printf("ERROR: Initial Raw Row Read failed with error %d\r\n", ret);
    }
    for (int i=0; i < sizeof(initial_data); i++) {
        if (initial_data[i] != 0) {
            printf("ERROR: This example requires empty OTP rows to run - change the ecc_row and raw_row variables to an empty row and recompile\r\n");
            //return 0;
        }
    }

    if (ecc_row) {
        // Write an ECC value to OTP - the buffer must have a multiple of 2 length for ECC data
        unsigned char ecc_write_data[16] = "Hello from OTP";
        cmd.flags = ecc_row | OTP_CMD_ECC_BITS | OTP_CMD_WRITE_BITS;
        ret = rom_func_otp_access(ecc_write_data, sizeof(ecc_write_data), cmd);
        if (ret) {
            printf("ERROR: ECC Write failed with error %d\r\n", ret);
        } else {
            printf("ECC Write succeeded\r\n");
        }

        // Read it back
        unsigned char ecc_read_data[sizeof(ecc_write_data)] = {0};
        cmd.flags = ecc_row | OTP_CMD_ECC_BITS;
        ret = rom_func_otp_access(ecc_read_data, sizeof(ecc_read_data), cmd);
        if (ret) {
            printf("ERROR: ECC Read failed with error %d\r\n", ret);
        } else {
            printf("ECC Data read is \"%s\"\r\n", ecc_read_data);
        }

        // Set some bits, to demonstrate ECC error correction
        unsigned char ecc_toggle_buffer[sizeof(ecc_write_data)*2] = {0};
        cmd.flags = ecc_row;
        ret = rom_func_otp_access(ecc_toggle_buffer, sizeof(ecc_toggle_buffer), cmd);
        if (ret) {
            printf("ERROR: Raw read of ECC data failed with error %d\r\n", ret);
        } else {
            ecc_toggle_buffer[0] = 'x'; // will fail to recover, as flips 2 bits from 'H' (100_1000 -> 111_1000)
            ecc_toggle_buffer[24] = 't'; // will recover, as only flips 1 bit from 'T' (101_0100 -> 111_0100)
            cmd.flags = ecc_row | OTP_CMD_WRITE_BITS;
            ret = rom_func_otp_access(ecc_toggle_buffer, sizeof(ecc_toggle_buffer), cmd);
            if (ret) {
                printf("ERROR: Raw overwrite of ECC data failed with error %d\r\n", ret);
            } else {
                printf("Raw overwrite of ECC data succeeded\r\n");
            }
        }

        // Read it back
        unsigned char ecc_toggled_read_data[sizeof(ecc_write_data)] = {0};
        cmd.flags = ecc_row | OTP_CMD_ECC_BITS;
        ret = rom_func_otp_access(ecc_toggled_read_data, sizeof(ecc_toggled_read_data), cmd);
        if (ret) {
            printf("ERROR: ECC Read failed with error %d\r\n", ret);
        } else {
            printf("ECC Data read is now \"%s\"\r\n", ecc_toggled_read_data);
        }

        // Attempt to write a different ECC value to OTP - should fail
        unsigned char ecc_overwrite_data[sizeof(ecc_write_data)] = "hello from otp";
        cmd.flags = ecc_row | OTP_CMD_ECC_BITS | OTP_CMD_WRITE_BITS;
        ret = rom_func_otp_access(ecc_overwrite_data, sizeof(ecc_overwrite_data), cmd);
        if (ret == BOOTROM_ERROR_UNSUPPORTED_MODIFICATION) {
            printf("Overwrite of ECC data failed as expected\r\n");
        } else {
            printf("ERROR: ");
            if (ret) {
                printf("Overwrite failed with error %d\r\n", ret);
            } else {
                printf("Overwrite succeeded\r\n");
            }
        }
    }

    if (raw_row) {
        // Write a raw value to OTP - the buffer must have a multiple of 4 length for raw data
        // Each row only holds 24 bits, so every 4th byte isn't written to OTP
        unsigned char raw_write_data[20] = "Hel\0lo \0fro\0m O\0TP";
        cmd.flags = raw_row | OTP_CMD_WRITE_BITS;
        ret = rom_func_otp_access(raw_write_data, sizeof(raw_write_data), cmd);
        if (ret) {
            printf("ERROR: Raw Write failed with error %d\r\n", ret);
        } else {
            printf("Raw Write succeeded\r\n");
        }

        // Read it back
        unsigned char raw_read_data[sizeof(raw_write_data)] = {0};
        cmd.flags = raw_row;
        ret = rom_func_otp_access(raw_read_data, sizeof(raw_read_data), cmd);
        if (ret) {
            printf("ERROR: Raw Read failed with error %d\r\n", ret);
        } else {
            // Remove the null bytes
            for (int i=0; i < sizeof(raw_read_data)/4; i++) {
                memcpy(raw_read_data + i*3, raw_read_data + i*4, 3);
            }
            printf("Raw Data read is \"%s\"\r\n", raw_read_data);
        }

        // Attempt to write a different raw value to OTP - should succeed, provided no bits are cleared
        // This can be done by using '~' for even characters, and 'o' for odd ones
        unsigned char raw_overwrite_data[sizeof(raw_write_data)] = {0};
        for (int i=0; i < sizeof(raw_write_data); i++) {
            if (raw_write_data[i]) {
                raw_overwrite_data[i] = (raw_write_data[i] % 2) ? 'o' : '~';
            } else {
                raw_overwrite_data[i] = 0;
            }
        }
        cmd.flags = raw_row | OTP_CMD_WRITE_BITS;
        ret = rom_func_otp_access(raw_overwrite_data, sizeof(raw_overwrite_data), cmd);
        if (ret) {
            printf("ERROR: Raw Overwrite failed with error %d\r\n", ret);
        } else {
            printf("Raw Overwrite succeeded\r\n");
        }

        // Read it back
        unsigned char raw_read_data_again[sizeof(raw_write_data)] = {0};
        cmd.flags = raw_row;
        ret = rom_func_otp_access(raw_read_data_again, sizeof(raw_read_data_again), cmd);
        // Remove the null bytes
        for (int i=0; i < sizeof(raw_read_data_again)/4; i++) {
            memcpy(raw_read_data_again + i*3, raw_read_data_again + i*4, 3);
        }
        if (ret) {
            printf("ERROR: Raw Read failed with error %d\r\n", ret);
        } else {
            printf("Raw Data read is now \"%s\"\r\n", raw_read_data_again);
        }
    }

    int16_t lock_row = ecc_row ? ecc_row : raw_row;
    // Lock the OTP page, to prevent any more reads or writes until the next reset
    int page = lock_row / 0x40;
    otp_hw->sw_lock[page] = 0xf;
    printf("OTP Software Lock Done\r\n");

    // Attempt to read it back again - should fail
    unsigned char read_data_locked[8] = {0};
    cmd.flags = lock_row | (lock_row == ecc_row ? OTP_CMD_ECC_BITS : 0);
    ret = rom_func_otp_access(read_data_locked, sizeof(read_data_locked), cmd);
    if (ret == BOOTROM_ERROR_NOT_PERMITTED) {
        printf("Locked read failed as expected\r\n");
    } else {
        printf("ERROR: ");
        if (ret) {
            printf("Locked read failed with error %d\r\n", ret);
        } else {
            printf("Locked read succeeded. Data read is \"%s\"\r\n", read_data_locked);
        }
    }








    //Below is the demo code for dummy.c template, which is useful for cribbing
    // opt-args parsing and other boilerplate code
#if 0
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

    printf("dummy command complete\r\n");
    #endif
}