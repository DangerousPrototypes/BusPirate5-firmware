#include <stdio.h>
#include <string.h>
#include "pico/stdlib.h"
#include "pirate.h"
#include "bytecode.h"
#include "command_struct.h"       // File system related
#include "fatfs/ff.h"       // File system related
#include "pirate/storage.h" // File system related
#include "ui/ui_cmdln.h"    // This file is needed for the command line parsing functions
// #include "ui/ui_prompt.h" // User prompts and menu system
// #include "ui/ui_const.h"  // Constants and strings
#include "ui/ui_help.h"    // Functions to display help in a standardized way
#include "system_config.h" // Stores current Bus Pirate system configuration
//#include "pirate/amux.h"   // Analog voltage measurement functions
//#include "pirate/button.h" // Button press functions
//#include "msc_disk.h"
//#include "binmode/binmodes.h"
//#include "ui/ui_prompt.h"
#include "ui/ui_term.h"
#include "modes.h"


// This array of strings is used to display help USAGE examples for the dummy command
static const char* const usage[] = {
    "dump <bytes> <file>",
    "First, manually setup a read from the device",
    "Then, run this command to read X bytes to a file",
    "Read X bytes to a file: dump 256 example.bin",
};

static const struct ui_help_options options[] = { 0 };

void dump_handler(struct command_result* res) {
    // we can use the ui_help_show function to display the help text we configured above
    if (ui_help_show(res->help_flag, usage, count_of(usage), &options[0], count_of(options))) { 
        return;
    }

    // get number of bytes to read argument
    uint32_t dump_len = 0;
    bool has_len = cmdln_args_uint32_by_position(1, &dump_len);
    if (!has_len) {
        printf("Error: No length specified\r\n\r\n");
        goto display_help;
    }

    // get filename argument
    char filename[13];
    if (!cmdln_args_string_by_position(2, sizeof(filename), filename)) {
        printf("Error: No filename specified\r\n\r\n");
        goto display_help;
    }

    // open the file for writing, then use the current mode read function to read the specified number of bytes
    FIL file;
    FRESULT fsres = f_open(&file, filename, FA_WRITE | FA_CREATE_ALWAYS);
    if (fsres != FR_OK) {
        printf("Error: Failed to open file %s\r\n\r\n", filename);
        return;
    }

    // read the specified number of bytes to the file
    uint8_t buf[256];
    uint32_t bytes_read = 0;
    struct _bytecode result;
    while (bytes_read < dump_len) {
        uint32_t bytes_to_read = dump_len - bytes_read;
        if (bytes_to_read > sizeof(buf)) {
            bytes_to_read = sizeof(buf);
        }

        for(uint32_t i=0; i < bytes_to_read; i++){
            modes[system_config.mode].protocol_read(&result, NULL);  
            buf[i] = result.in_data;
        }

        // write the bytes to the file
        UINT bw;
        fsres = f_write(&file, buf, bytes_to_read, &bw);
        if (fsres != FR_OK) {
            printf("Error: Failed to write %d bytes to file %s\r\n", bytes_to_read, filename);
            f_close(&file);
            return;
        }

        // increment the number of bytes read
        bytes_read += bytes_to_read;
    }

    // close the file
    f_close(&file);
    return;

display_help:
    ui_help_show(true, usage, count_of(usage), &options[0], count_of(options));
    
}


