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
#if BP_HW_PSRAM
    #include "psram.h"
#endif

// This array of strings is used to display help USAGE examples for the dummy command
static const char* const usage[] = { "psram",
                                     "Test PSRAM memory allocation and write/read",
                                     "This command will allocate two blocks of PSRAM memory, write to them, and read back the data",
                                     };

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
    { 0, "-h", T_HELP_HELP },    // init is an example we'll find by position
};

void psram_handler(struct command_result* res) {
    if (ui_help_show(res->help_flag, usage, count_of(usage), &options[0], count_of(options))) {
        return;
    }  

    #if !BP_HW_PSRAM
        printf("PSRAM not present on this hardware\n");
        return;
    #else

        #define BLOCK_SIZE	(size_t)(128 * 1024u)
        uint32_t *mem1 = __psram_malloc(BLOCK_SIZE);
        if (mem1 == NULL){
            printf("Failed to allocate memory 1\n");
            return;
        }

        for (int i = 0; i < 1024; i++)
            mem1[i] = 0xCAFEFEED;
        
        for (int i = 0; i < 1024; i++)
            if (mem1[i] != 0xCAFEFEED ){
                printf("Failed to write to memory 1\n");
                __psram_free(mem1);
                return;
            }

        uint32_t *mem2 = __psram_malloc(BLOCK_SIZE * 2);
        if (mem2 == NULL){
            printf("Failed to allocate memory 2\n");
            __psram_free(mem1);
            return;
        }


        for(int i = 0; i < 2048; i++)
            mem2[i] = 0xDEADBEEF;

        for(int i = 0; i < 1024; i++)
            if (mem2[i] != 0xDEADBEEF){
                printf("Failed to write to memory 2\n");
                __psram_free(mem1);
                __psram_free(mem2);
                return;
            }

        __psram_free(mem1);
        __psram_free(mem2);  

    #endif


}