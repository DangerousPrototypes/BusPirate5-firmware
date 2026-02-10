/*
    microchip pic functions for the universal programmer 'plank' (https:// xx) 
    
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
#include "commands/up/pic.h"

static void picsendcmd(uint32_t cmd);
static void picsenddata(uint32_t dat);
static uint32_t picreaddata(void);
static void picreadids(void);

enum uptest_actions_enum {
    UP_PIC_READID,
};

static const struct cmdln_action_t uptest_actions[] = {
    {UP_PIC_READID, "readid"},
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

void up_pic_handler(struct command_result* res)
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
      case UP_PIC_READID:
        picreadids();
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




/// --------------------------------------------------------------------- microchip helpers
// move to inline?
static void picsendcmd(uint32_t cmd)
{
  up_sendspi(0, UP_PIC_8PIN_PDAT, 0, UP_PIC_8PIN_PCLK, 0, 6, UP_SPIMODE1_CS0_LSB, cmd);
}

static void picsenddata(uint32_t dat)
{
  up_sendspi(0, UP_PIC_8PIN_PDAT, 0, UP_PIC_8PIN_PCLK, 0, 16, UP_SPIMODE1_CS0_LSB, dat);
}

static uint32_t picreaddata(void)
{
  uint32_t data;
  
  up_setdirection(UP_PIC_8PIN_DIR|UP_PIC_8PIN_PDAT);
  data=up_sendspi(0, 0, UP_PIC_8PIN_PDAT, UP_PIC_8PIN_PCLK, 0, 16, UP_SPIMODE1_CS0_LSB, 0);
  up_setdirection(UP_PIC_8PIN_DIR);

  return data;
}


// only tested with 12f629
// TODO: find the pics in the icinf.xml and export
static void picreadids(void)
{
  uint32_t id1, id2, id3, id4, id5, devid, revid;
  char c;

  up_icprint(8, 1, 8, 4);
  
  printf("Is this correct? y to continue\r\n");
  while(!rx_fifo_try_get(&c));
  if(c!='y')
  {
    printf("Aborted!!\r\n");
    system_config.error = 1;
    return;
  }

  up_setvcc(0);
  up_setvpp(0);

  up_setpullups(UP_PIC_8PIN_PU);
  up_setdirection(UP_PIC_8PIN_DIR);
  up_pins(0);
  up_setvpp(2);
  busy_wait_us(1000);
  up_setvcc(1);  
  
  // TODO: remove later
//  while(!rx_fifo_try_get(&c));
  
  picsendcmd(PIC_CMD_LOADCFG);      // 0x2000
  picsenddata(0);
  
  picsendcmd(PIC_CMD_READD_PM);
  id1=picreaddata();
  id1=(id1>>1)&0x3FFF;
  
  picsendcmd(PIC_CMD_INCRADR);      // 0x2001
  picsendcmd(PIC_CMD_READD_PM);
  id2=picreaddata();
  id2=(id2>>1)&0x3FFF;
  
  picsendcmd(PIC_CMD_INCRADR);      // 0x2002
  picsendcmd(PIC_CMD_READD_PM);
  id3=picreaddata();
  id3=(id3>>1)&0x3FFF;

  picsendcmd(PIC_CMD_INCRADR);      // 0x2003
  picsendcmd(PIC_CMD_READD_PM);
  id4=picreaddata();
  id4=(id4>>1)&0x3FFF;
  
  picsendcmd(PIC_CMD_INCRADR);      // 0x2004
  picsendcmd(PIC_CMD_INCRADR);      // 0x2005
  picsendcmd(PIC_CMD_INCRADR);      // 0x2006

  picsendcmd(PIC_CMD_READD_PM);
  id5=picreaddata();
  id5=(id5>>1)&0x3FFF;
  
  devid=(id5>>5);
  revid=(id5&0x001f);
  
  printf("userids: 0x%08X 0x%08X 0x%08X 0x%08X\r\n", id1, id2, id3, id4);
  printf("dev 0x%03X rev 0x%02X\r\n", devid, revid);
}

