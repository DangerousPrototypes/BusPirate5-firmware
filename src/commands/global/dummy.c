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
#include "lib/bp_args/bp_cmd.h"    // New command line parsing functions
// #include "ui/ui_prompt.h" // User prompts and menu system
// #include "ui/ui_const.h"  // Constants and strings
#include "ui/ui_help.h"    // Functions to display help in a standardized way
#include "system_config.h" // Stores current Bus Pirate system configuration
#include "pirate/amux.h"   // Analog voltage measurement functions
#include "pirate/button.h" // Button press functions
#include "msc_disk.h"

// This array of strings is used to display help USAGE examples for the dummy command
static const char* const usage[] = { "dummy [init|test]\r\n\t[-b(utton)] [-i(nteger) <value>] [-f <file>]",
                                     "Initialize:%s dummy init",
                                     "Test:%s dummy test",
                                     "Test, require button press:%s dummy test -b",
                                     "Integer, value required:%s dummy -i 123",
                                     "Create/write/read file:%s dummy -f dummy.txt",
                                     "Kitchen sink:%s dummy test -b -i 123 -f dummy.txt" };

// New bp_command_opt_t array for flags
static const bp_command_opt_t dummy_opts[] = {
    { "button", 'b', BP_ARG_NONE,     NULL,      T_HELP_DUMMY_B_FLAG },
    { "integer",'i', BP_ARG_REQUIRED, "value", T_HELP_DUMMY_I_FLAG },
    { "file",   'f', BP_ARG_REQUIRED, "file",  T_HELP_DUMMY_FILE_FLAG },
    { 0 }
};

const bp_command_def_t dummy_def = {
    .name = "dummy",
    .description = 0x00,
    .actions = NULL,
    .action_count = 0,
    .opts = dummy_opts,
    .usage = usage,
    .usage_count = count_of(usage)
};

void dummy_handler(struct command_result* res) {
    uint32_t value; // somewhere to keep an integer value
    char file[13];  // somewhere to keep a string value (8.3 filename + 0x00 = 13 characters max)

    // the help -h flag can be serviced by the command line parser automatically, or from within the command
    // the action taken is set by the help_text variable of the command struct entry for this command
    // 1. a single T_ constant help entry assigned in the commands[] struct in commands.c will be shown automatically
    // 2. if the help assignment in commands[] struct is 0x00, it can be handled here (or ignored)
    // res.help_flag is set by the command line parser if the user enters -h
    // we can use the bp_cmd_help_check function to display the help text we configured above
    if (bp_cmd_help_check(&dummy_def, res->help_flag)) {
        return;
    }

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

    // use the bp_cmd_get_positional_string function to get the parameter by position
    // Position:    0   1   2   3
    // Command:  dummy  init
    // in this case position 1, which is the first parameter after the command
    // the function returns true if a parameter is present at position 1, and false if it's not
    if (bp_cmd_get_positional_string(&dummy_def, 1, action_str, sizeof(action_str))) {
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
    bool b_flag = bp_cmd_find_flag(&dummy_def, 'b');
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
    // check for the -i flag with an integer value
    bool i_flag = bp_cmd_get_uint32(&dummy_def, 'i', &value);
    // if the flag is set, print the value
    if (i_flag) {
        printf("Flag -i is set with value %d\r\n", value);
    } else { // error parsing flag and value
        // Note: new API doesn't provide number_format or has_arg info
        if (false) { // placeholder - can't detect this case with new API
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
    bool f_flag = bp_cmd_get_string(&dummy_def, 'f', file, sizeof(file));

    if (!f_flag) { // flag/arg not entered
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
}