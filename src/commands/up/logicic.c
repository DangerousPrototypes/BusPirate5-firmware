/*
    logic test for 74xx and 40xx chip for the universal programmer 'plank' (https:// xx) 
    
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
#include "commands/up/logicic.h"
#include "commands/up/up_logicic.h"

static bool up_logic_find(const char* type, int* numpins, uint16_t* starttest, uint16_t* endtest);
static void print_logic_types(void);
static void testlogicic(int numpins, uint16_t logicteststart, uint16_t logictestend);

enum uptest_actions_enum {
    UP_LOGIC_TEST,
};

static const struct cmdln_action_t uptest_actions[] = {
    {UP_LOGIC_TEST, "test"},
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

static const up_logic_table_desc_t up_logic_tables[] = {
  {logicic14, count_of(logicic14), 14},
  {logicic16, count_of(logicic16), 16},
  {logicic20, count_of(logicic20), 20},
  {logicic24, count_of(logicic24), 24},
  {logicic28, count_of(logicic28), 28},
  {logicic40, count_of(logicic40), 40},
};


void up_logic_handler(struct command_result* res)
{
    uint32_t action;
    char type[16];
    int numpins;
    uint16_t starttest,endtest;

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
      case UP_LOGIC_TEST:
        if (!cmdln_args_string_by_position(2, sizeof(type), type))
        {
          printf("you need to specify the logic ic\r\n");
          system_config.error = 1;
          return;
        }

        if(!up_logic_find(type, &numpins, &starttest, &endtest))
        {
          printf("Not found: %s\r\n", type);
          print_logic_types();
          system_config.error = 1;
          return;
        }        
        testlogicic(numpins, starttest, endtest);

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



static bool up_logic_find(const char* type, int* numpins, uint16_t* starttest, uint16_t* endtest)
{
  for (size_t t = 0; t < count_of(up_logic_tables); t++) {
    const up_logic_table_desc_t* lt = &up_logic_tables[t];
    for (size_t i = 0; i < lt->count; i++) {
      if (strcmp(type, lt->table[i].name) == 0) {
        *numpins   = lt->numpins;
        *starttest = lt->table[i].start;
        *endtest   = lt->table[i].end;
        return true;
      }
    }
  }
  return false;
}

static void print_logic_types(void)
{
  printf("Supported logic IC types:\r\n");
  for (size_t t = 0; t < count_of(up_logic_tables); t++) {
    const up_logic_table_desc_t* lt = &up_logic_tables[t];
    printf("%s%d-pin types:%s\r\n", ui_term_color_info(), lt->numpins, ui_term_color_reset());
    for (size_t i = 0; i < lt->count; i++) {
      printf("%s\t", lt->table[i].name);
    }
    printf("\r\n\r\n");
  }
}

/// --------------------------------------------------------------------- logictest helpers
// explanation of the tests here: https://github.com/Johnlon/integrated-circuit-tester
//

static void testlogicic(int numpins, uint16_t logicteststart, uint16_t logictestend)
{
  int i, pin, vcc, gnd;
  int pinoffset;
  uint32_t dutin, dutout, direction, pullups, expected, clock, ignore;
  uint32_t starttime;
  char pintest, c;

  pinoffset=(32-numpins)/2;

  // find the vcc and gnd pins
  for(i=0; i<numpins; i++)
  {
    if(numpins==14) pintest=logictest14[logicteststart][i];
    else if(numpins==16) pintest=logictest16[logicteststart][i];
    else if(numpins==20) pintest=logictest20[logicteststart][i];
    else if(numpins==24) pintest=logictest24[logicteststart][i];
    else if(numpins==28) pintest=logictest28[logicteststart][i];
    else if(numpins==40) pintest=logictest40[logicteststart][i];  

    if(pintest=='V') vcc=i+1;
    if(pintest=='G') gnd=i+1;
  }

  up_icprint(numpins, vcc, gnd, 33);

  printf("Is this correct? y to continue\r\n");
  while(!rx_fifo_try_get(&c));
  if(c!='y')
  {
    printf("Aborted!!\r\n");
    system_config.error = 1;
    return;
  }
  
  up_setvpp(0);
  up_setvcc(1);

  starttime=time_us_32();

  for(i=logicteststart; i<logictestend; i++)
  {
    dutin=0;
    clock=0;
    dutout=0;
    direction=0;
    pullups=0;
    expected=0;
    ignore=0;
    
    for(pin=0; pin<numpins; pin++)
    {
      if(numpins==14) pintest=logictest14[i][pin];
      else if(numpins==16) pintest=logictest16[i][pin];
      else if(numpins==20) pintest=logictest20[i][pin];
      else if(numpins==24) pintest=logictest24[i][pin];
      else if(numpins==28) pintest=logictest28[i][pin];
      else if(numpins==40) pintest=logictest40[i][pin];
      
      if((pintest=='0')||                   // output 0
         (pintest=='V')||                   // Vcc/Vdd  (user should jumper properly)
         (pintest=='G'))                    // GND (user should jumper properly)
      {
        //dutin|=matrix[pinoffset+pin];     // pin=0
        //clock|=matrix[pinoffset+pin];     // no clock for this pin
        //direction|=matrix[pinoffset+pin]; // output
        //pullups|=matrix[pinoffset+pin];   // no pullup
        //expected|=matrix[pinoffset+pin];  // expect 0
        //ignore|=matrix[pinoffset+pin];    // don't ignore this pin
      }
      else if(pintest=='1')                 // output 1
      {
        dutin|=matrix[pinoffset+pin];       // pin=1
        //clock|=matrix[pinoffset+pin];     // no clock for this pin
        //direction|=matrix[pinoffset+pin]; // output
        //pullups|=matrix[pinoffset+pin];   // no pullup
        expected|=matrix[pinoffset+pin];    // expect 1
        //ignore|=matrix[pinoffset+pin];    // don't ignore this pin
      }
      else if(pintest=='L')                 // input 0
      {
        //dutin|=matrix[pinoffset+pin];     // don't care
        //clock|=matrix[pinoffset+pin];     // no clock for this pin
        direction|=matrix[pinoffset+pin];   // input
        //pullups|=matrix[pinoffset+pin];   // no pullup ?
        //expected|=matrix[pinoffset+pin];  // expect 0
        //ignore|=matrix[pinoffset+pin];    // don't ignore this pin
      }
      else if(pintest=='H')                 // input 1/Z(?)
      {
        //dutin|=matrix[pinoffset+pin];     // don't care
        //clock|=matrix[pinoffset+pin];     // no clock for this pin
        direction|=matrix[pinoffset+pin];   // input
        //pullups|=matrix[pinoffset+pin];   // no pullup ?
        expected|=matrix[pinoffset+pin];    // expect 1
        //ignore|=matrix[pinoffset+pin];    // don't ignore this pin
      }
      else if(pintest=='Z')                 // input Z, let pullup do its work
      {
        //dutin|=matrix[pinoffset+pin];     // don't care
        //clock|=matrix[pinoffset+pin];     // no clock for this pin
        direction|=matrix[pinoffset+pin];   // input
        pullups|=matrix[pinoffset+pin];     // pullup
        expected|=matrix[pinoffset+pin];    // expect 1
        //ignore|=matrix[pinoffset+pin];    // don't ignore this pin
      }
      else if(pintest=='C')                 // clock pin 0 -> 1 -> 0
      {
        //dutin|=matrix[pinoffset+pin];     // pin=0 -> 1 -> 0
        clock|=matrix[pinoffset+pin];       // set clock for this pin
        //direction|=matrix[pinoffset+pin]; // output
        //pullups|=matrix[pinoffset+pin];   // pullup
        //expected|=matrix[pinoffset+pin];  // expect 0
        //ignore|=matrix[pinoffset+pin];    // don't ignore this pin
      }
      else if(pintest=='X')                 // ignore pin
      {
        //dutin|=matrix[pinoffset+pin];     // dont care
        //clock|=matrix[pinoffset+pin];     // no clock for this pin
        direction|=matrix[pinoffset+pin];   // input
        //pullups|=matrix[pinoffset+pin];   // pullup
        expected|=matrix[pinoffset+pin];    // expect 1
        ignore|=matrix[pinoffset+pin];      // ignore this pin
      }
    }
    
    // actually test
    up_setpullups(pullups);	
    up_setdirection(direction);
    
    // do we have a clock signal?
    if(clock)
    {
      dutout=up_pins(dutin);
      dutout=up_pins(dutin|clock);
      dutout=up_pins(dutin);
    }
    else dutout=up_pins(dutin);
    
    // mask dont care pins
    dutout|=ignore;
    
    printf(" Pass %d ", ((i-logicteststart)+1));
    
    if(dutout==expected) printf("OK\r\n");
    else printf("Not OK\r\n");
  }
  
  printf("Took %d ms to execute\r\n", ((time_us_32()-starttime)/1000));
  
  up_setvpp(0);
  up_setvcc(0);
}


