// TODO: BIO use, pullups, psu
/*
    Welcome to dummy.c, a growing demonstration of how to add commands to the Bus Pirate firmware.
    You can also use this file as the basis for your own commands.
    Type "dummy" at the Bus Pirate prompt to see the output of this command
    Temporary info available at https://forum.buspirate.com/t/command-line-parser-for-developers/235
*/
#include <stdbool.h>
#include <stdio.h>
#include <string.h>
#include "pico/stdlib.h"
#include "pirate.h"
#include "command_struct.h"       // File system related
#include "fatfs/ff.h"       // File system related
#include "pirate/storage.h" // File system related
#include "lib/bp_args/bp_cmd.h"
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
static const char* const usage[] = { "bluetag [jtag|swd] [-c <channels>] [-v(ersion)] [-d(isable pulsing)]",
                                     "blueTag interactive interface:%s bluetag",
                                     "JTAG scan, 6 channels:%s bluetag jtag -c 6",
                                     "SWD scan, 4 channels:%s bluetag swd -c 4",
                                     "Show version:%s bluetag -v",
                                     "Disable JTAG pin pulsing:%s bluetag jtag -c 6 -d",
                                     "",
                                     "blueTag by @Aodrulez https://github.com/Aodrulez/blueTag" };

enum bluetag_actions {
    BLUETAG_JTAG = 0,
    BLUETAG_SWD,
};

static const bp_command_action_t bluetag_action_defs[] = {
    { BLUETAG_JTAG, "jtag", T_JTAG_BLUETAG_JTAG },
    { BLUETAG_SWD,  "swd",  T_JTAG_BLUETAG_SWD },
};

static const bp_command_opt_t bluetag_opts[] = {
    { "channels", 'c', BP_ARG_REQUIRED, "<count>", T_JTAG_BLUETAG_CHANNELS },
    { "version",  'v', BP_ARG_NONE,     NULL,      T_JTAG_BLUETAG_VERSION },
    { "disable",  'd', BP_ARG_NONE,     NULL,      T_JTAG_BLUETAG_DISABLE },
    { 0 }
};

const bp_command_def_t bluetag_def = {
    .name         = "bluetag",
    .description  = T_JTAG_BLUETAG_OPTIONS,
    .actions      = bluetag_action_defs,
    .action_count = count_of(bluetag_action_defs),
    .opts         = bluetag_opts,
    .usage        = usage,
    .usage_count  = count_of(usage),
};

static void bluetag_cli(void);

void bluetag_handler(struct command_result* res) {
    if (bp_cmd_help_check(&bluetag_def, res->help_flag)) {
        return;
    }

    //print version
    if(bp_cmd_find_flag(&bluetag_def, 'v')){
        printf("\r\nCurrent version: %s\r\n\r\n", version);
        return;
    }

    // check for option verb
    uint32_t action;
    if(bp_cmd_get_action(&bluetag_def, &action)) {

        //disable pin pulsing
        bool disable_pulse=bp_cmd_find_flag(&bluetag_def, 'd');
        if(disable_pulse){
            printf("\r\nDisabled pin pulsing\r\n\r\n");
        }

        //number of channels
        uint32_t channels=0;
        bool c_flag = bp_cmd_get_uint32(&bluetag_def, 'c', &channels);
        if(!c_flag){
            printf("\r\nSpecify the number of channels with the -c flag, -h for help\r\n");
            return;
        }else{
            printf("\r\nNumber of channels set to: %d\r\n\r\n", channels);
        }

        // lets do jtag!
        if(action == BLUETAG_JTAG){
            if(channels < 4 || channels > 8){
                printf("Invalid number of JTAG channels (min 4, max 8)\r\n");
                return;
            }

            jtag_cleanup();
            struct jtagScan_t jtag;
            jtag.channelCount = channels;
            jtag.jPulsePins = !disable_pulse;
            if(!jtagScan(&jtag)){
                bluetag_progressbar_cleanup(jtag.maxPermutations);
                printf("\r\n\r\n");
                printf("\tNo JTAG devices found. Please try again.\r\n");
            }else{
                //char jtag_pin_labels[][5] = { "TRST", "TCK", "TDI", "TDO", "TMS" }; 
                /*system_bio_update_purpose_and_label(true, (jtag.xTCK-8), BP_PIN_MODE, jtag_pin_labels[1]);
                system_bio_update_purpose_and_label(true, (jtag.xTDI-8), BP_PIN_MODE, jtag_pin_labels[2]);
                system_bio_update_purpose_and_label(true, (jtag.xTDO-8), BP_PIN_MODE, jtag_pin_labels[3]);
                system_bio_update_purpose_and_label(true, (jtag.xTMS-8), BP_PIN_MODE, jtag_pin_labels[4]);
                if(jtag.xTRST != 0){
                    system_bio_update_purpose_and_label(true, (jtag.xTRST-8), BP_PIN_MODE, jtag_pin_labels[0]);
                }*/
            }            
        }
        
        if(action == BLUETAG_SWD){
            if(channels < 2 || channels > 8){
                printf("Invalid number of SWD channels (min 2, max 8)\r\n");
                return;
            }
            jtag_cleanup();
            struct swdScan_t swd;
            swd.channelCount = channels;         
            if(!swdScan(&swd)){
                bluetag_progressbar_cleanup(swd.maxPermutations);
                printf("\r\n\r\n");
                printf("\tNo SWD devices found. Please try again.\r\n");
            }else{
                /*char swd_pin_labels[][5] = { "SCLK", "SDIO" };
                system_bio_update_purpose_and_label(true, (swd.xSwdClk-8), BP_PIN_MODE, swd_pin_labels[0]);
                system_bio_update_purpose_and_label(true, (swd.xSwdIO-8), BP_PIN_MODE, swd_pin_labels[1]);*/
            }     
        }

    } else {
        bluetag_cli();
    }
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
        struct jtagScan_t jtag;
        struct swdScan_t swd;
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
                jtag.channelCount = get_channels(4, 8);
                jtag.jPulsePins = jPulsePins;
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
                //struct swdScan_t swd;
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