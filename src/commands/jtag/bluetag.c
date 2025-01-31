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
#include "lib/bluetag/src/blueTag.h"
#include "usb_rx.h"

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

static void bluetag_cli(void);

void bluetag_handler(struct command_result* res) {
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

    // check for verb (jtag, swd, cli)

    // cli
    bluetag_cli();
}

static void splashScreen(void)
{
    printf("\r\n\t _______ ___     __   __ _______ _______ _______ _______");
    printf("\r\n\t|  _    |   |   |  | |  |       |       |   _   |       |");
    printf("\r\n\t| |_|   |   |   |  | |  |    ___|_     _|  |_|  |    ___|");
    printf("\r\n\t|       |   |   |  |_|  |   |___  |   | |       |   | __");
    printf("\r\n\t|  _   ||   |___|       |    ___| |   | |       |   ||  |");
    printf("\r\n\t| |_|   |       |       |   |___  |   | |   _   |   |_| |");
    printf("\r\n\t|_______|_______|_______|_______| |___| |__| |__|_______|");
    printf("\r\n");
    printf("\r\n\t[ JTAGulator alternative for Raspberry Pi RP2040 Dev Boards ]");
    printf("\r\n\t+-----------------------------------------------------------+");
    printf("\r\n\t| @Aodrulez             https://github.com/Aodrulez/blueTag |");
    printf("\r\n\t+-----------------------------------------------------------+\r\n\r\n");   
}            


static void showPrompt(void)
{
    printf(" > ");
}

static void showMenu(void)
{
    printf(" Supported commands:\r\n\r\n");
    printf("     \"h\" = Show this menu\r\n");
    printf("     \"v\" = Show current version\r\n");
    printf("     \"p\" = Toggle 'pin pulsing' setting (Default:ON)\r\n");
    printf("     \"j\" = Perform JTAG pinout scan\r\n");
    printf("     \"s\" = Perform SWD pinout scan\r\n");
    printf("     \"x\" = Exit blueTag\r\n\r\n");
    printf(" [ Note 1: Disable 'local echo' in your terminal emulator program ]\r\n");
    printf(" [ Note 2: Try deactivating 'pin pulsing' (p) if valid pinout isn't found ]\r\n\r\n");
}

// to maintain mergability with the upstream project,
// I'm reproducing some of the control functions here
static void bluetag_cli(void){
    // GPIO init
    //initChannels();

    bool jPulsePins=true;
    bluetag_jPulsePins_set(jPulsePins);
    splashScreen();
    showMenu();
    showPrompt();
    char cmd;
    while(1)
    {
        //cmd=getc(stdin);
        rx_fifo_get_blocking(&cmd);
        printf("%c\n\n",cmd);
        switch(cmd)
        {
            // Help menu requested
            case 'h':
                showMenu();
                break;
            case 'v':
                //version is extern char *version;
                printf("\tCurrent version: %s\r\n\r\n", version);
                break;

            case 'j':
                //jtagScan();
                break;

            case 's':
                //swdScan();
                break;

            case 'p':
                jPulsePins=!jPulsePins;
                bluetag_jPulsePins_set(jPulsePins);
                if(jPulsePins){
                    printf("\tPin pulsing activated.\r\n\r\n");
                }else{
                    printf("\tPin pulsing deactivated.\r\n\r\n");
                }                
                break;


            case 'x':
                /*for(int x=0;x<=25;x++)
                {
                    gpio_put(onboardLED, 1);
                    sleep_ms(250);
                    gpio_put(onboardLED, 0);
                    sleep_ms(250);
                }*/
               // cleanup pins
                return;
                break;

            default:
                printf(" Unknown command. \n\n");
                break;
        }
        showPrompt();
    }    
}