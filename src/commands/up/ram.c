/*
    ram tests for the universal programmer 'plank' (https:// xx) 
    
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
#include "commands/up/ram.h"
#include "commands/up/up_lut62xx.h"
#include "commands/up/up_lutdram41xx.h"

static void initdram41(void);
static void writedram41(uint32_t col, uint32_t row, bool data);
static bool readdram41(uint32_t col, uint32_t row);
static void testdram41(uint32_t variant);
static void testsram62(uint32_t variant);

enum uptest_actions_enum {
    UP_RAM_TEST,
};

static const struct cmdln_action_t uptest_actions[] = {
    {UP_RAM_TEST, "test"},
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

void up_ram_handler(struct command_result* res)
{
    uint32_t action;
    char type[16];
    uint32_t ictype;

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
      case UP_RAM_TEST:
        if (!cmdln_args_string_by_position(2, sizeof(type), type))
        {
          printf("you need to specify the logic ic\r\n");
          system_config.error = 1;
          return;
        }
        
        if(strcmp(type, "4164")==0) ictype=UP_DRAM_4164;
          else if(strcmp(type, "41256")==0) ictype=UP_DRAM_41256;
          else if(strcmp(type, "6264")==0) ictype=UP_SRAM_6264;
          else if(strcmp(type, "62256")==0) ictype=UP_SRAM_62256;
          else if(strcmp(type, "621024")==0) ictype=UP_SRAM_621024;
          else
          {
            printf("RAM type unknown\r\n");
            printf(" available are: 4164, 41256, 6264, 62256, 621024\r\n");
            system_config.error = 1;
            return;
          }
        
        if(ictype<UP_SRAM_6264) testdram41(ictype);
        else testsram62(ictype);
        break;
        
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


/// --------------------------------------------------------------------- dramtest helpers
static void initdram41(void)
{
  busy_wait_us(1000);
  
  up_pins(UP_DRAM41_W|UP_DRAM41_RAS|UP_DRAM41_CAS                       );
  up_pins(UP_DRAM41_W              |UP_DRAM41_CAS                       );
  up_pins(UP_DRAM41_W|UP_DRAM41_RAS|UP_DRAM41_CAS                       );
  up_pins(UP_DRAM41_W              |UP_DRAM41_CAS                       );
  up_pins(UP_DRAM41_W|UP_DRAM41_RAS|UP_DRAM41_CAS                       );
  up_pins(UP_DRAM41_W              |UP_DRAM41_CAS                       );
  up_pins(UP_DRAM41_W|UP_DRAM41_RAS|UP_DRAM41_CAS                       );
  up_pins(UP_DRAM41_W              |UP_DRAM41_CAS                       );
  up_pins(UP_DRAM41_W|UP_DRAM41_RAS|UP_DRAM41_CAS                       );
  up_pins(UP_DRAM41_W              |UP_DRAM41_CAS                       );
  up_pins(UP_DRAM41_W|UP_DRAM41_RAS|UP_DRAM41_CAS                       );
}

static void writedram41(uint32_t col, uint32_t row, bool data)
{
  uint32_t dat;

  if(data) dat=UP_DRAM41_DI;
  else dat=0;
  
//  up_pins(UP_DRAM41_W|UP_DRAM41_RAS|UP_DRAM41_CAS                       );    // w=1, ras=1, cas=1 
  up_pins(UP_DRAM41_W|UP_DRAM41_RAS|UP_DRAM41_CAS|lut_dram41xx[row]    );    // w=1, ras=1, cas=1, row addres 
  up_pins(UP_DRAM41_W              |UP_DRAM41_CAS|lut_dram41xx[row]    );    // w=0, ras=0, cas=1, ras=hi address , data on the bus
  up_pins(UP_DRAM41_W              |UP_DRAM41_CAS|lut_dram41xx[col]    );    // w=0, ras=0, cas=1, ras=hi address , data on the bus
  up_pins(UP_DRAM41_W                            |lut_dram41xx[col]|dat);    // w=1, ras=0, cas=0, cas=low address, data on the bus
  up_pins(                                                          dat);    // w=1, ras=1, cas=1, data on the bus
  up_pins(UP_DRAM41_W|UP_DRAM41_RAS|UP_DRAM41_CAS                      );    // w=1, ras=1, cas=0
}

static bool readdram41(uint32_t col, uint32_t row)
{
  uint32_t dat;
  
      up_pins(UP_DRAM41_W|UP_DRAM41_RAS|UP_DRAM41_CAS|lut_dram41xx[row]     );    // w=1, ras=1, cas=1, row addres 
      up_pins(UP_DRAM41_W              |UP_DRAM41_CAS|lut_dram41xx[row]     );    // w=0, ras=0, cas=1, ras=hi address , data on the bus
      up_pins(UP_DRAM41_W              |UP_DRAM41_CAS|lut_dram41xx[col]     );    // w=0, ras=0, cas=1, ras=hi address , data on the bus
      up_pins(UP_DRAM41_W                            |lut_dram41xx[col]     );    // w=1, ras=0, cas=0, cas=low address, data on the bus
  dat=up_pins(UP_DRAM41_W                                                   );    // w=1, ras=1, cas=1, data on the bus
      up_pins(UP_DRAM41_W|UP_DRAM41_RAS|UP_DRAM41_CAS                       );    // w=1, ras=1, cas=0

  return (dat&UP_DRAM41_DO);
}

static void testdram41(uint32_t variant)
{
  unsigned int row,col, rowend, colend;
  bool d;
  char c;
  bool ok;
  uint32_t starttime;
  
  switch(variant)
  {
    case UP_DRAM_4164:  rowend=0x100;
                        colend=0x100;
                        break;
    case UP_DRAM_41256: rowend=0x200;
                        colend=0x200;
                        break;
    default:            printf("Unknown DRAM !!\r\n");
                        system_config.error = 1;
                        return;
  }
  
  up_icprint(16, 8, 16, 33);
  
  printf("Is this correct? y to continue\r\n");
  while(!rx_fifo_try_get(&c));
  if(c!='y')
  {
    printf("Aborted!!\r\n");
    system_config.error = 1;
    return;
  }
  
  starttime=time_us_32();

  up_setpullups(UP_DRAM41_PU);
  up_setdirection(UP_DRAM41_DIR);
  up_setvpp(0);
  up_setvcc(1);
  initdram41();
  
  // All 0s
  ok=true;
  for(row=0; row<rowend; row++)
  {
    printf("\rAll 0s .. row 0x%03X %c ", row, rotate[(row)&0x07]);
    for(col=0; col<colend; col++)
    {
      writedram41(row, col, 0);
      if(readdram41(row, col)!=0)
      {
        ok=false;
        break;
      }
    }
  }
 
  if(ok) printf("OK\r\n");
  else printf("NOK\r\n");

  // All 1s
  ok=true;
  for(row=0; row<rowend; row++)
  {
    printf("\rAll 1s .. row 0x%03X %c ", row, rotate[(row)&0x07]);
    for(col=0; col<colend; col++)
    {
      writedram41(row, col, 1);
      if(readdram41(row, col)!=1)
      {
        ok=false;
        break;
      }
    }
  }
 
  if(ok) printf("OK\r\n");
  else printf("NOK\r\n");

  // All 0101s
  ok=true;
  d=0;
  for(row=0; row<rowend; row++)
  {
    printf("\rAll 0101s .. row 0x%03X %c ", row, rotate[(row)&0x07]);
    for(col=0; col<colend; col++)
    {
      writedram41(row, col, d);
      if(readdram41(row, col)!=d)
      {
        ok=false;
        break;
      }
      d^=1;
    }
  }
 
  if(ok) printf("OK\r\n");
  else printf("NOK\r\n");

  // All 1010s
  d=1;
  ok=true;
  for(row=0; row<rowend; row++)
  {
    printf("\rAll 1010s .. row 0x%03X %c ", row, rotate[(row)&0x07]);
    for(col=0; col<colend; col++)
    {
      writedram41(row, col, d);
      if(readdram41(row, col)!=d)
      {
        ok=false;
        break;
      }
      d^=1;
    }
  }
 
  if(ok) printf("OK\r\n");
  else printf("NOK\r\n");

  printf("Took %d ms to execute\r\n", ((time_us_32()-starttime)/1000));
  
  up_setvpp(0);
  up_setvcc(0);
}

static void testsram62(uint32_t variant)
{
  int i, j, kbit, pass, test;
  uint8_t testchar;
  uint32_t sramaddress, dutin, dutout, starttime, ce2, we;
  bool ok=true;
  char c;
  
  uint8_t sramtest[] = { 0x00, 0xFF, 0xAA, 0x55 };
  
  switch(variant)
  {
    case UP_SRAM_6264:    kbit=64;
                          ce2=UP_62XX_CE2_28;
                          up_icprint(28, 28, 14, 33);
                          break;
    case UP_SRAM_62256:   kbit=256;
                          ce2=0;
                          up_icprint(28, 28, 14, 33);
                          break;
    case UP_SRAM_621024:  kbit=1024;
                          ce2=UP_62XX_CE2_32;
                          up_icprint(32, 32, 16, 33);
                          break;
    default:              printf("Unknown SRAM !!\r\n");
                          system_config.error = 1;
                          return;
  }

  printf("Is this correct? y to continue\r\n");
  while(!rx_fifo_try_get(&c));
  if(c!='y')
  {
    printf("Aborted!!\r\n");
    system_config.error = 1;
    return;
  }

  starttime=time_us_32();

  up_setpullups(UP_62XX_PU);
  up_setdirection(UP_62XX_DIR);
  up_setvpp(0);
  up_setvcc(1);

  for(test=0; test<sizeof(sramtest); test++)
  {
    testchar=sramtest[test];
    for(j=0; j<kbit; j++)
    {
      printf("\r test #%d (0x%02X) SRAM 0x%05X %c ", test, sramtest[test], j*128, rotate[j&0x07]);

      for(i=0; i<128; i++)
      {
        sramaddress=j*128+i;

        dutin=lut_62xx_lo[(sramaddress&0x0FF)];
        dutin|=lut_62xx_mi[((sramaddress>>8)&0x0FF)];
        dutin|=lut_62xx_hi[((sramaddress>>16)&0x0FF)];    
        
        dutin|=lut_62xx_dat[testchar];
        
        up_pins(dutin|UP_62XX_CE1|UP_62XX_OE|UP_62XX_WE);      // deselect ic
        up_setdirection(0);                                    // datapins to output
        up_pins(dutin|UP_62XX_OE|ce2);                         // write
        up_pins(dutin|UP_62XX_CE1|UP_62XX_OE|UP_62XX_WE);      // deselect
        up_setdirection(UP_62XX_DIR);                          // datapins to input
        dutout=up_pins(dutin|UP_62XX_WE|ce2);                  // read back
        
        if(dutout!=(dutin|UP_62XX_WE|ce2))
        {
          ok=false; 
          break;
        }
      }
      testchar^=0xFF; //invert the byte
    }
    if(ok) printf("OK\r\n");
    else printf("NOK\r\n");
    
    ok=true;
  }
  
  printf("Took %d ms to execute\r\n", ((time_us_32()-starttime)/1000));
  
  up_setvpp(0);
  up_setvcc(0);
}


