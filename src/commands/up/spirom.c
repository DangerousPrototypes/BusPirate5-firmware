/*
    spirom stuff for the universal programmer 'plank' (https:// xx) 
    
    (C) Chris van Dongen 2025-26

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
#include "pirate/hwspi.h"
#include "pirate/lsb.h"
#include "ui/ui_term.h"
#include "bytecode.h"   
#include "pirate/bio.h"
#include "usb_rx.h"
#include "pirate/mem.h"

#include "mode/up.h"
#include "commands/up/universalprogrammer_pinout.h"
#include "commands/up/spirom.h"

static void spiromreadid(void);


enum uptest_actions_enum {
    UP_SPIROM_READID,
};

static const struct cmdln_action_t uptest_actions[] = {
    {UP_SPIROM_READID, "readid"},
};    

static const char* const usage[] = { "up [test|vtest|dram|logic|buffer|eprom|spirom|display|ram]\r\n\t[-t <device>]",
                                     "Adapter test:%s up test",
                                     "Adapter voltage test:%s up vtest",
                                     "DRAM test:%s up dram -t <device>",
                                     "Logic IC test:%s up logic -t <device>",
                                     "Firmware buffer:%s up buffer read -f <filename>",
                                     "EEPROM read/readid/blank/write/verify:%s up eprom read -t <device>",
                                     "SPI ROM readid/read:%s up spirom readid -t <device>",
                                     "Display test:%s up display -t <device>",
                                     "RAM test:%s up ram -t <device>" };

static const struct ui_help_options options[] = {
    0
};

void up_spirom_handler(struct command_result* res)
{
    uint32_t action;

    if (ui_help_show(res->help_flag, usage, count_of(usage), &options[0], count_of(options))) {
        return;
    }

    if(!ui_help_check_vout_vref())
    {
        return;
    }

    if(cmdln_args_get_action(uptest_actions, count_of(uptest_actions), &action)){
        ui_help_show(true, usage, count_of(usage), &options[0], count_of(options)); // show help if requested
        return;
    }

    switch(action)
    {
      case UP_SPIROM_READID:
        spiromreadid();
        break;
      default: //should never get here, should throw help
        printf("No action defined (test, vtest, dram, logic, buffer, eprom)\r\n");
        system_config.error = 1;  
        return;
    }
    
    up_init();      // to be sure
    up_setvcc(0);  // to be sure
    up_setvpp(0);  // to be sure
}


/// --------------------------------------------------------------------- spirom helpers
static void spiromreadid(void)
{
  uint32_t dutin, id, id2;
  char c;
  
  up_icprint(8, 8, 4, 33);
  
  printf("Is this correct? y to continue\r\n");
  while(!rx_fifo_try_get(&c));
  if(c!='y')
  {
    printf("Aborted!!\r\n");
    system_config.error = 1;
    return;
  }
  
  up_setpullups(UP_SPIROM_8PIN_PU);
  up_setdirection(UP_SPIROM_8PIN_DIR);
  up_setvcc(1);
  up_setvpp(0);
  
  dutin=(UP_SPIROM_8PIN_CS|UP_SPIROM_8PIN_WP|UP_SPIROM_8PIN_HOLD);
  up_pins(dutin);

  // command 0x90
  dutin=up_setcs(dutin, UP_SPIROM_8PIN_CS, UP_SPIMODE0_CS0_MSB, UP_CS_ACTIVE);
     up_sendspi(dutin, UP_SPIROM_8PIN_MOSI, UP_SPIROM_8PIN_MISO, UP_SPIROM_8PIN_CLK, 0,  8, UP_SPIMODE0_CS0_MSB, 0x90);
     up_sendspi(dutin, UP_SPIROM_8PIN_MOSI, UP_SPIROM_8PIN_MISO, UP_SPIROM_8PIN_CLK, 0, 24, UP_SPIMODE0_CS0_MSB, 0);
  id=up_sendspi(dutin, UP_SPIROM_8PIN_MOSI, UP_SPIROM_8PIN_MISO, UP_SPIROM_8PIN_CLK, 0, 16, UP_SPIMODE0_CS0_MSB, 0);
  dutin=up_setcs(dutin, UP_SPIROM_8PIN_CS, UP_SPIMODE0_CS0_MSB, UP_CS_INACTIVE);
  
  // command 0x9F
  dutin=up_setcs(dutin, UP_SPIROM_8PIN_CS, UP_SPIMODE0_CS0_MSB, UP_CS_ACTIVE);
      up_sendspi(dutin, UP_SPIROM_8PIN_MOSI, UP_SPIROM_8PIN_MISO, UP_SPIROM_8PIN_CLK, 0,  8, UP_SPIMODE0_CS0_MSB, 0x9F);
  id2=up_sendspi(dutin, UP_SPIROM_8PIN_MOSI, UP_SPIROM_8PIN_MISO, UP_SPIROM_8PIN_CLK, 0, 24, UP_SPIMODE0_CS0_MSB, 0);
  dutin=up_setcs(dutin, UP_SPIROM_8PIN_CS, UP_SPIMODE0_CS0_MSB, UP_CS_INACTIVE);
  
  printf(" id-0x90=%08X id-0x9F=%08X\r\n", id, id2);
  
  up_setvcc(0);
  up_setvpp(0);
}


