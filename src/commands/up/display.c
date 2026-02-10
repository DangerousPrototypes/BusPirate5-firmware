/*
    display test for the universal programmer 'plank' (https:// xx) 
    
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
#include "commands/up/up_lutdl1414.h"
#include "commands/up/display.h"

static void testdl1414(void);
static void testtil305(void);



enum uptest_actions_enum {
    UP_DISPLAY_TEST,
};

static const struct cmdln_action_t uptest_actions[] = {
    {UP_DISPLAY_TEST, "test"},

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

void up_display_handler(struct command_result* res)
{
    uint32_t action;
    char type[16];

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
      case UP_DISPLAY_TEST:
        if (!cmdln_args_string_by_position(2, sizeof(type), type))
        {
          printf("you need to specify the logic ic\r\n");
          system_config.error = 1;
          return;
        }

          if(strcmp(type, "dl1414")==0) testdl1414();
          else if(strcmp(type, "til305")==0) testtil305();
          else
          {
            printf("DISPLAY type unknown\r\n");
            printf(" available are: dl1414, til305\r\n");
            system_config.error = 1;
            return;
          }
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


/// --------------------------------------------------------------------- display helpers

void putc_dl1414(uint8_t addr, uint8_t c)
{
  uint32_t dutin;
  
  dutin=lut_dl1414[(c&0x7F)];
  dutin|=((addr&0x01)?UP_DL1414_A0:0);
  dutin|=((addr&0x02)?UP_DL1414_A1:0);
  
  up_pins(dutin|UP_DL1414_WR);
  up_pins(dutin             );
  up_pins(dutin|UP_DL1414_WR);
}

static void testdl1414(void)
{
  int i;
  uint32_t dutin;
  char c;

  up_icprint(12, 6, 7, 33);
  
  printf("Is this correct? y to continue\r\n");
  while(!rx_fifo_try_get(&c));
  if(c!='y')
  {
    printf("Aborted!!\r\n");
    system_config.error = 1;
    return;
  }
  
  up_setvcc(1);
  up_setpullups(UP_DL1414_PU);
  up_setdirection(UP_DL1414_DIR);
  
  for(i=0x00; i<0x80; i++)
  {
    putc_dl1414(3, i);
    putc_dl1414(2, i);
    putc_dl1414(1, i);
    putc_dl1414(0, i);
    
    printf("display: %c%c%c%c\r", (i>0x1f?i:' '), (i>0x1f?i:' '), (i>0x1f?i:' '), (i>0x1f?i:' '));
    busy_wait_us(500000);
  }
  
  printf("display:     \r\n");
  // turn display off (display space)
  putc_dl1414(3, ' ');
  putc_dl1414(2, ' ');
  putc_dl1414(1, ' ');
  putc_dl1414(0, ' ');

}

static void testtil305(void)
{
  int i;
  char c;
  
  up_icprint(14, 33, 33, 33);
  
  printf("Is this correct? y to continue\r\n");
  while(!rx_fifo_try_get(&c));
  if(c!='y')
  {
    printf("Aborted!!\r\n");
    system_config.error = 1;
    return;
  }
  
  up_setpullups(UP_TIL305_PU);
  up_setdirection(UP_TIL305_DIR);

  up_pins(UP_TIL305_COL1               |UP_TIL305_ROW2|UP_TIL305_ROW3|UP_TIL305_ROW4|UP_TIL305_ROW5|UP_TIL305_ROW6|UP_TIL305_ROW7);  
  busy_wait_us(100000);
  up_pins(UP_TIL305_COL1|UP_TIL305_ROW1               |UP_TIL305_ROW3|UP_TIL305_ROW4|UP_TIL305_ROW5|UP_TIL305_ROW6|UP_TIL305_ROW7);  
  busy_wait_us(100000);
  up_pins(UP_TIL305_COL1|UP_TIL305_ROW1|UP_TIL305_ROW2|UP_TIL305_ROW3               |UP_TIL305_ROW5|UP_TIL305_ROW6|UP_TIL305_ROW7);  
  busy_wait_us(100000);
  up_pins(UP_TIL305_COL1|UP_TIL305_ROW1|UP_TIL305_ROW2|UP_TIL305_ROW3|UP_TIL305_ROW4               |UP_TIL305_ROW6|UP_TIL305_ROW7);  
  busy_wait_us(100000);
  up_pins(UP_TIL305_COL1|UP_TIL305_ROW1|UP_TIL305_ROW2|UP_TIL305_ROW3|UP_TIL305_ROW4|UP_TIL305_ROW5               |UP_TIL305_ROW7);  
  busy_wait_us(100000);
  up_pins(UP_TIL305_COL1|UP_TIL305_ROW1|UP_TIL305_ROW2|UP_TIL305_ROW3|UP_TIL305_ROW4|UP_TIL305_ROW5|UP_TIL305_ROW6               );  
  busy_wait_us(100000);

  up_pins(UP_TIL305_COL2               |UP_TIL305_ROW2|UP_TIL305_ROW3|UP_TIL305_ROW4|UP_TIL305_ROW5|UP_TIL305_ROW6|UP_TIL305_ROW7);  
  busy_wait_us(100000);
  up_pins(UP_TIL305_COL2|UP_TIL305_ROW1               |UP_TIL305_ROW3|UP_TIL305_ROW4|UP_TIL305_ROW5|UP_TIL305_ROW6|UP_TIL305_ROW7);  
  busy_wait_us(100000);
  up_pins(UP_TIL305_COL2|UP_TIL305_ROW1|UP_TIL305_ROW2|UP_TIL305_ROW3               |UP_TIL305_ROW5|UP_TIL305_ROW6|UP_TIL305_ROW7);  
  busy_wait_us(100000);
  up_pins(UP_TIL305_COL2|UP_TIL305_ROW1|UP_TIL305_ROW2|UP_TIL305_ROW3|UP_TIL305_ROW4               |UP_TIL305_ROW6|UP_TIL305_ROW7);  
  busy_wait_us(100000);
  up_pins(UP_TIL305_COL2|UP_TIL305_ROW1|UP_TIL305_ROW2|UP_TIL305_ROW3|UP_TIL305_ROW4|UP_TIL305_ROW5               |UP_TIL305_ROW7);  
  busy_wait_us(100000);
  up_pins(UP_TIL305_COL2|UP_TIL305_ROW1|UP_TIL305_ROW2|UP_TIL305_ROW3|UP_TIL305_ROW4|UP_TIL305_ROW5|UP_TIL305_ROW6               );  
  busy_wait_us(100000);

  up_pins(UP_TIL305_COL3               |UP_TIL305_ROW2|UP_TIL305_ROW3|UP_TIL305_ROW4|UP_TIL305_ROW5|UP_TIL305_ROW6|UP_TIL305_ROW7);  
  busy_wait_us(100000);
  up_pins(UP_TIL305_COL3|UP_TIL305_ROW1               |UP_TIL305_ROW3|UP_TIL305_ROW4|UP_TIL305_ROW5|UP_TIL305_ROW6|UP_TIL305_ROW7);  
  busy_wait_us(100000);
  up_pins(UP_TIL305_COL3|UP_TIL305_ROW1|UP_TIL305_ROW2|UP_TIL305_ROW3               |UP_TIL305_ROW5|UP_TIL305_ROW6|UP_TIL305_ROW7);  
  busy_wait_us(100000);
  up_pins(UP_TIL305_COL3|UP_TIL305_ROW1|UP_TIL305_ROW2|UP_TIL305_ROW3|UP_TIL305_ROW4               |UP_TIL305_ROW6|UP_TIL305_ROW7);  
  busy_wait_us(100000);
  up_pins(UP_TIL305_COL3|UP_TIL305_ROW1|UP_TIL305_ROW2|UP_TIL305_ROW3|UP_TIL305_ROW4|UP_TIL305_ROW5               |UP_TIL305_ROW7);  
  busy_wait_us(100000);
  up_pins(UP_TIL305_COL3|UP_TIL305_ROW1|UP_TIL305_ROW2|UP_TIL305_ROW3|UP_TIL305_ROW4|UP_TIL305_ROW5|UP_TIL305_ROW6               );  
  busy_wait_us(100000);

  up_pins(UP_TIL305_COL4               |UP_TIL305_ROW2|UP_TIL305_ROW3|UP_TIL305_ROW4|UP_TIL305_ROW5|UP_TIL305_ROW6|UP_TIL305_ROW7);  
  busy_wait_us(100000);
  up_pins(UP_TIL305_COL4|UP_TIL305_ROW1               |UP_TIL305_ROW3|UP_TIL305_ROW4|UP_TIL305_ROW5|UP_TIL305_ROW6|UP_TIL305_ROW7);  
  busy_wait_us(100000);
  up_pins(UP_TIL305_COL4|UP_TIL305_ROW1|UP_TIL305_ROW2|UP_TIL305_ROW3               |UP_TIL305_ROW5|UP_TIL305_ROW6|UP_TIL305_ROW7);  
  busy_wait_us(100000);
  up_pins(UP_TIL305_COL4|UP_TIL305_ROW1|UP_TIL305_ROW2|UP_TIL305_ROW3|UP_TIL305_ROW4               |UP_TIL305_ROW6|UP_TIL305_ROW7);  
  busy_wait_us(100000);
  up_pins(UP_TIL305_COL4|UP_TIL305_ROW1|UP_TIL305_ROW2|UP_TIL305_ROW3|UP_TIL305_ROW4|UP_TIL305_ROW5               |UP_TIL305_ROW7);  
  busy_wait_us(100000);
  up_pins(UP_TIL305_COL4|UP_TIL305_ROW1|UP_TIL305_ROW2|UP_TIL305_ROW3|UP_TIL305_ROW4|UP_TIL305_ROW5|UP_TIL305_ROW6               );  
  busy_wait_us(100000);

  up_pins(UP_TIL305_COL5               |UP_TIL305_ROW2|UP_TIL305_ROW3|UP_TIL305_ROW4|UP_TIL305_ROW5|UP_TIL305_ROW6|UP_TIL305_ROW7);  
  busy_wait_us(100000);
  up_pins(UP_TIL305_COL5|UP_TIL305_ROW1               |UP_TIL305_ROW3|UP_TIL305_ROW4|UP_TIL305_ROW5|UP_TIL305_ROW6|UP_TIL305_ROW7);  
  busy_wait_us(100000);
  up_pins(UP_TIL305_COL5|UP_TIL305_ROW1|UP_TIL305_ROW2|UP_TIL305_ROW3               |UP_TIL305_ROW5|UP_TIL305_ROW6|UP_TIL305_ROW7);  
  busy_wait_us(100000);
  up_pins(UP_TIL305_COL5|UP_TIL305_ROW1|UP_TIL305_ROW2|UP_TIL305_ROW3|UP_TIL305_ROW4               |UP_TIL305_ROW6|UP_TIL305_ROW7);  
  busy_wait_us(100000);
  up_pins(UP_TIL305_COL5|UP_TIL305_ROW1|UP_TIL305_ROW2|UP_TIL305_ROW3|UP_TIL305_ROW4|UP_TIL305_ROW5               |UP_TIL305_ROW7);  
  busy_wait_us(100000);
  up_pins(UP_TIL305_COL5|UP_TIL305_ROW1|UP_TIL305_ROW2|UP_TIL305_ROW3|UP_TIL305_ROW4|UP_TIL305_ROW5|UP_TIL305_ROW6               );  
  busy_wait_us(100000);

}

