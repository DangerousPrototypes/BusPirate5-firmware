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
#include "mode/jtag.h"
#include "ui/ui_prompt.h"

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
    //if (ui_help_show(res->help_flag, usage, count_of(usage), &options[0], count_of(options))) {
    //    return;
    //}

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

//TODO: use our prompt function to get channels
static uint32_t get_channels(uint32_t minChannels, uint32_t maxChannels)
{
    printf("\r\nNumber of connected IO, starting with IO0 (Min %d, Max %d, x to exit): ", minChannels, maxChannels);
    char c;
    rx_fifo_get_blocking(&c);
    while(true)
    {
        if(c == 'x')
        {
            return 0;
        }
        if(c >= '0'+minChannels && c <= '0'+maxChannels)
        {
            printf("\r\n\r\n\tNumber of channels set to: %d\r\n\r\n",c-'0');
            return c-'0';
        }
        printf("\r\nEnter a valid value (Min %d, Max %d, x to exit): ", minChannels, maxChannels);
        rx_fifo_get_blocking(&c);
    }
}

// to maintain mergability with the upstream project,
// I'm reproducing some of the control functions here
static void bluetag_cli(void){
    bool jPulsePins=true;
    jtag_cleanup();
    splashScreen();
    showMenu();
    showPrompt();
    char cmd;
    while(1)
    {
        rx_fifo_get_blocking(&cmd);
        printf("%c\r\n",cmd);
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
                jtag_cleanup();
                struct jtagScan_t jtag;
                jtag.channelCount = get_channels(4, 8);
                if(jtag.channelCount == 0){
                    printf("\r\nAbort\r\n\r\n");
                    break;
                }
                if(!jtagScan(&jtag)){
                    bluetag_progressbar_cleanup(jtag.maxPermutations);
                    printf("\r\n\r\n");
                    printf("\tNo JTAG devices found. Please try again.\r\n");
                }else{
                    char jtag_pin_labels[][5] = { "TRST", "TCK", "TDI", "TDO", "TMS" }; 
                    system_bio_update_purpose_and_label(true, (jtag.xTCK-8), BP_PIN_MODE, jtag_pin_labels[1]);
                    system_bio_update_purpose_and_label(true, (jtag.xTDI-8), BP_PIN_MODE, jtag_pin_labels[2]);
                    system_bio_update_purpose_and_label(true, (jtag.xTDO-8), BP_PIN_MODE, jtag_pin_labels[3]);
                    system_bio_update_purpose_and_label(true, (jtag.xTMS-8), BP_PIN_MODE, jtag_pin_labels[4]);
                    if(jtag.xTRST != 0){
                        system_bio_update_purpose_and_label(true, (jtag.xTRST-8), BP_PIN_MODE, jtag_pin_labels[0]);
                    }
                }
                break;

            case 's':  
                jtag_cleanup();
                struct swdScan_t swd;
                swd.channelCount = get_channels(2, 8);  
                if(swd.channelCount == 0){
                    printf("\r\nAbort\r\n\r\n");
                    break;
                }            
                if(!swdScan(&swd)){
                    bluetag_progressbar_cleanup(swd.maxPermutations);
                    printf("\r\n\r\n");
                    printf("\tNo devices found. Please try again.\r\n");
                }else{
                    char swd_pin_labels[][5] = { "SCLK", "SDIO" };
                    system_bio_update_purpose_and_label(true, (swd.xSwdClk-8), BP_PIN_MODE, swd_pin_labels[0]);
                    system_bio_update_purpose_and_label(true, (swd.xSwdIO-8), BP_PIN_MODE, swd_pin_labels[1]);
                }                
                break;

            case 'p':
                jPulsePins=!jPulsePins;
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
                jtag_cleanup();
                jtag_setup_exc();
                return;
                break;

            default:
                printf("\tUnknown command.\r\n");
                break;
        }
        showPrompt();
    }    
}