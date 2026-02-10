/*
    hardware 'driver' for the universal programmer 'plank' (https:// xx) 
    
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
#include "commands/up/test.h"

static void up_test(void);
static void up_vtest(void);


enum uptest_actions_enum {
    UP_TEST_HW,
    UP_TEST_VOLT,
};

static const struct cmdln_action_t uptest_actions[] = {
    {UP_TEST_HW, "hw"},
    {UP_TEST_VOLT, "volts"}
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

void up_test_handler(struct command_result* res)
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
      case UP_TEST_HW:
        up_test();
        break;
      case UP_TEST_VOLT:
        up_test();
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


/// --------------------------------------------------------------------- Hardware test
// let the user tweak the voltages
void up_vtest(void)
{
  char c;
  
  up_setvpp(0);
  up_setvcc(0);
  up_setpullups(0);              // disable pullups
  up_setdirection(0xFFFFFFFFl);  // all inputs
  
  printf("IC disconnected?\r\n");
  
  while(!rx_fifo_try_get(&c));
  if(c!='y')
  {
    printf("Aborted!!\r\n");
    system_config.error = 1;
    return;
  }
  
  printf("Tweak Vdd and Vpp to desired value. Press any key to continue\r\n");
  
  // TODO: show measured voltages
  while(!rx_fifo_try_get(&c))
  {
    printf("Vcc=%d.%03d  ", (5*(*hw_pin_voltage_ordered[M_UP_VSENSE_VCC + 1]) / 1000), ((5*(*hw_pin_voltage_ordered[M_UP_VSENSE_VCC + 1])) % 1000));
    printf("Vpp=%d.%03d \r", (5*(*hw_pin_voltage_ordered[M_UP_VSENSE_VPP + 1]) / 1000), ((5*(*hw_pin_voltage_ordered[M_UP_VSENSE_VPP + 1])) % 1000));
  }

  printf("\r\n");

  up_setvpp(0);
  up_setvcc(0);
}

// new basic test
void up_test(void)
{
  int i;
  uint32_t dutin, dutout;
  char c;
 
  printf("Is no chip attached?\r\n");
  while(!rx_fifo_try_get(&c));
  if(c!='y')
  {
    printf("Aborted!!\r\n");
    system_config.error = 1;
    return;
  }

  // output test
  printf("1. Output test\r\n");
  up_setvcc(0);
  up_setvpp(0);
  up_setpullups(0);
  up_setdirection(0);
  up_pins(0);
  
  for(i=0; i<32; i++)
  {
    up_pins(matrix[i]);
    printf(" IO%02d = 1\r", i+1);
    while(!rx_fifo_try_get(&c));
  }
  printf("\r\n");
  
  // input test
  printf("2. Input test (pullups on, use GND)\r\n");
  up_setdirection(0xFFFFFFFFl);
  up_setpullups(0xFFFFFFFFl);
  while(!rx_fifo_try_get(&c))
  {
    dutout=up_pins(0);
    for(i=0; i<32; i++)
    {
      if(!(dutout&matrix[i])) printf(" IO%02d low\r", i+1);
    }
    busy_wait_us(200);
  }
  printf("\r\n");

  // Vcc voltage rail test
  // TODO; wait for new hardware and test it
  printf("3a. Vcc Voltagerail test\r\n");
  
  printf("Vcc=0\r\n");
  up_setvcc(0);
  while(!rx_fifo_try_get(&c));
  
  printf("Vcc=Vcc\r\n");
  up_setvcc(1);
  while(!rx_fifo_try_get(&c));
  
  printf("Vcc=Vcch\r\n");
  up_setvcc(2);
  while(!rx_fifo_try_get(&c));
  
  up_setvcc(0);
  
  // Vpp voltage rail test
  // TODO; wait for new hardware and test it
  printf("3b. Vpp Voltagerail test\r\n");
  
  printf("Vpp=0\r\n");
  up_setvpp(0);
  while(!rx_fifo_try_get(&c));
  
  printf("Vpp=Vcc\r\n");
  up_setvpp(1);
  while(!rx_fifo_try_get(&c));
  
  printf("Vcc=Vcch\r\n");
  up_setvpp(2);
  while(!rx_fifo_try_get(&c));
  
  up_setvpp(0);
  
  // Vcch, VppH measurement
  // TODO: wait for new hardware
  printf("3c. voltages\r\n");
  while(!rx_fifo_try_get(&c))
  {
    printf("Vcc=%d.%03d  ", (5*(*hw_pin_voltage_ordered[M_UP_VSENSE_VCC + 1]) / 1000), (5*(*hw_pin_voltage_ordered[M_UP_VSENSE_VCC + 1]) % 1000));
    printf("Vpp=%d.%03d \r", (5*(*hw_pin_voltage_ordered[M_UP_VSENSE_VPP + 1]) / 1000), (5*(*hw_pin_voltage_ordered[M_UP_VSENSE_VPP + 1]) % 1000));
  }
}

