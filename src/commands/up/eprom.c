/*
    eprom functions for the universal programmer 'plank' (https:// xx) 
    
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
#include "ui/ui_prompt.h" // User prompts and menu system
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
#include "commands/up/universalprogrammer_device.h"
#include "commands/up/up_eproms.h"
#include "commands/up/up_lut27xx.h"
#include "commands/up/up_lut23xx.h"
#include "commands/up/eprom.h"

static void write27eprom(uint32_t ictype, uint32_t page, int pulse);
static void read23eprom(uint32_t ictype, uint8_t mode);
static void read27eprom(uint32_t ictype, uint32_t page, uint8_t mode);
static void read27epromid(int pins);

static bool up_eprom_find_type(const char* name, uint32_t* out_ictype);

enum uptest_actions_enum {
    UP_EPROM_READ,
    UP_EPROM_READID,
    UP_EPROM_WRITE,
    UP_EPROM_BLANK,
    UP_EPROM_VERIFY,
};

static const struct cmdln_action_t uptest_actions[] = {
    {UP_EPROM_READ, "read"},
    {UP_EPROM_READID, "readid"},
    {UP_EPROM_WRITE, "write"},
    {UP_EPROM_BLANK, "blank"},
    {UP_EPROM_VERIFY, "verify"},
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

void up_eprom_handler(struct command_result* res)
{
    uint32_t action, value, ictype;
    uint32_t pins, page, pulse;
    command_var_t arg; 
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

    if(action!=UP_EPROM_READID && !cmdln_args_find_flag_string('t', &arg, 10, type))
    {
      printf("Use -t to specify EPROM type\r\n");
      system_config.error = 1;
      return;
    }       
    
    if(action!=UP_EPROM_READID && !up_eprom_find_type(type, &ictype))
    {
      printf("EPROM type unknown\r\n");
      printf(" available are: 2764, 27128, 27256, 27512, 27010, 27020, 27040, 27080\r\n");
      printf("              27c64, 27c128, 27c256, 27c512, 27c010, 27c020, 27c040, 27c080\r\n");
      printf("              2332ll, 2332lh, 2332hl, 2332hh, 2364l, 2364h (l=/CSx, h=CSx)\r\n");
      system_config.error = 1;
      return;
    }
        
    if(cmdln_args_find_flag_uint32('p', &arg, &value))
    {
      page=value;     // page to read, page=128k for read
      pulse=value;    // pulse for write
      pins=value;     // pins for readid
    }
    else page=0;

    switch(action)
    {
      case UP_EPROM_READ:
        if((ictype>=UP_EPROM_2764)&&(ictype<=UP_EPROM_27080))
          read27eprom(ictype, page, EPROM_READ);
        else if((ictype>=UP_EPROM_2332LL)&&(ictype<=UP_EPROM_2364H))
          read23eprom(ictype, EPROM_READ);
        break;
      case UP_EPROM_BLANK:
        if((ictype>=UP_EPROM_2764)&&(ictype<=UP_EPROM_27080))
          read27eprom(ictype, page, EPROM_BLANK);
        else if((ictype>=UP_EPROM_2332LL)&&(ictype<=UP_EPROM_2364H))
          read23eprom(ictype, EPROM_BLANK);
        break;
      case UP_EPROM_VERIFY:
        if((ictype>=UP_EPROM_2764)&&(ictype<=UP_EPROM_27080))
          read27eprom(ictype, page, EPROM_VERIFY);
        else if((ictype>=UP_EPROM_2332LL)&&(ictype<=UP_EPROM_2364H))
          read23eprom(ictype, EPROM_VERIFY);
        break;  
      case UP_EPROM_READID:
        if((ictype>=UP_EPROM_2764)&&(ictype<=UP_EPROM_27080))
          read27epromid(pins);
        else if((ictype>=UP_EPROM_2332LL)&&(ictype<=UP_EPROM_2364H))
        {
          printf("Not supported!!\r\n");
          system_config.error=1;
          return;
        }
        break;  
      case UP_EPROM_WRITE:
        if((ictype>=UP_EPROM_2764)&&(ictype<=UP_EPROM_27080))
          write27eprom(ictype, page, pulse);
        else if((ictype>=UP_EPROM_2764)&&(ictype<=UP_EPROM_27080))
        {
          printf("Not supported!!\r\n");
          system_config.error=1;
          return;
        }
        break;  
      default: //should never get here, should throw help
        printf("No action defined (test, vtest, dram, logic, buffer, eprom)\r\n");
        system_config.error = 1;  
        return;
    }
    
    up_init();     // to be sure
    up_setvcc(0);  // to be sure
    up_setvpp(0);  // to be sure
}



/// --------------------------------------------------------------------- EPROM 27xxx functions

#if 0
typedef struct {
  uint32_t pin_mask;
  uint8_t bit_value;
} up_bit_map_t;

static const up_bit_map_t up_data_bus_map[] = {
  {UP_27XX_D0, 0x01},
  {UP_27XX_D1, 0x02},
  {UP_27XX_D2, 0x04},
  {UP_27XX_D3, 0x08},
  {UP_27XX_D4, 0x10},
  {UP_27XX_D5, 0x20},
  {UP_27XX_D6, 0x40},
  {UP_27XX_D7, 0x80},
};

static uint8_t up_decode_bits(uint32_t value, const up_bit_map_t *map, size_t count)
{
  uint8_t out = 0;
  for (size_t i = 0; i < count; i++) {
    if (value & map[i].pin_mask) {
      out |= map[i].bit_value;
    }
  }
  return out;
}

#endif

static void up_decode_bits(uint32_t dutout, uint8_t* temp)
{
  *temp = 0;
  if(dutout&UP_27XX_D0) *temp|=0x01;
  if(dutout&UP_27XX_D1) *temp|=0x02;
  if(dutout&UP_27XX_D2) *temp|=0x04;
  if(dutout&UP_27XX_D3) *temp|=0x08;
  if(dutout&UP_27XX_D4) *temp|=0x10;
  if(dutout&UP_27XX_D5) *temp|=0x20;
  if(dutout&UP_27XX_D6) *temp|=0x40;
  if(dutout&UP_27XX_D7) *temp|=0x80;
}


// read the epromid
static void read27epromid(int numpins)
{
  char c;
  uint32_t temp1, temp2;
  uint8_t id1, id2;
  int i;
  bool found;
  

  // does this work on non eproms (flash,eeprom)?
  if(numpins==24) up_icprint(24, 24, 12, 22);  // not 2732!!
  else if(numpins==28) up_icprint(28, 28, 14, 24); // not 27512! 
  else if(numpins==32) up_icprint(32, 32, 16, 26); // not 27080!
  else
  {
    printf("wrong number of pins\r\n");
    system_config.error = 1;
    return;
  }
  
  printf("Vpp=%d.%03d \r\n", (5*(*hw_pin_voltage_ordered[M_UP_VSENSE_VPP + 1]) / 1000), (5*(*hw_pin_voltage_ordered[M_UP_VSENSE_VPP + 1]) % 1000));
  printf("Is this correct? y to continue\r\n");
  while(!rx_fifo_try_get(&c));
  if(c!='y')
  {
    printf("Aborted!!\r\n");
    system_config.error = 1;
    return;
  }
  
  // TODO: check if vpp=12.5, vcc=5?
  // setup for 27xxx eprom 
  up_setpullups(UP_27XX_PU);
  up_setdirection(UP_27XX_DIR);
  up_pins(UP_27XX_OE|UP_27XX_CE); //vpp?
  
  // apply 12.5V to A9, vcc/vdd=5
  up_setvdd(1);
  up_setvpp(2);

  // read idcodes
  temp1=up_pins(UP_27XX_VPP28);            // manufacturer
  temp2=up_pins(UP_27XX_VPP28|UP_27XX_A0); // device

  // put device to sleep
  up_setvpp(0);
  up_pins(UP_27XX_OE|UP_27XX_CE);  // vpp??

  //decode id1 (manufacturer)
  //id1 = up_decode_bits(temp1, up_data_bus_map, count_of(up_data_bus_map));
  up_decode_bits(temp1, &id1);

  //decode id2 (device)
  //id2 = up_decode_bits(temp2, up_data_bus_map, count_of(up_data_bus_map));
  up_decode_bits(temp2, &id2);

  #if 0
  //decode
  id1=0;
  id2=0;
  
  //decode id1 (manufacturer)
  if(temp1&UP_27XX_D0) id1|=0x01;
  if(temp1&UP_27XX_D1) id1|=0x02;
  if(temp1&UP_27XX_D2) id1|=0x04;
  if(temp1&UP_27XX_D3) id1|=0x08;
  if(temp1&UP_27XX_D4) id1|=0x10;
  if(temp1&UP_27XX_D5) id1|=0x20;
  if(temp1&UP_27XX_D6) id1|=0x40;
  if(temp1&UP_27XX_D7) id1|=0x80;

  //decode id2 (device)
  if(temp2&UP_27XX_D0) id2|=0x01;
  if(temp2&UP_27XX_D1) id2|=0x02;
  if(temp2&UP_27XX_D2) id2|=0x04;
  if(temp2&UP_27XX_D3) id2|=0x08;
  if(temp2&UP_27XX_D4) id2|=0x10;
  if(temp2&UP_27XX_D5) id2|=0x20;
  if(temp2&UP_27XX_D6) id2|=0x40;
  if(temp2&UP_27XX_D7) id2|=0x80;

  #endif

  printf("manufacturerID = %02X, deviceID = %02X\r\n", id1, id2);
  
  found=false;

  for(i=0; i<(sizeof(up_devices)/sizeof(up_device)); i++)
  {
    if((up_devices[i].id1==id1)&&(up_devices[i].id2==id2))
    {
      found=true;
      printf("Found: %s %s ",manufacturers[up_devices[i].mnameid], up_devices[i].name);
      printf(" Vcc=%d.%02dV,", (up_devices[i].Vcc>>2), (up_devices[i].Vcc&0x03)*25);
      printf(" Vdd=%d.%02dV,", (up_devices[i].Vdd>>2), (up_devices[i].Vdd&0x03)*25);
      printf(" Vpp=%d.%02dV,", (up_devices[i].Vpp>>2), (up_devices[i].Vpp&0x03)*25);
      printf(" pulse=%d\r\n",up_devices[i].pulse);
    }
  }
  
  // did we found anything?
  if(found)
  {
    printf("Please crosscheck with datasheet !!\r\n");
  }
  
  up_setvcc(0); //depower device
  up_setvpp(0);
}


struct epromconfig
{
  uint8_t ictype;
  uint32_t kbit;
  uint32_t page;
  uint32_t pgm_write;
  uint32_t pgm_read;
  uint8_t pins;
  uint8_t vcc;
  uint8_t gnd;
  uint8_t vpp;
};

static const struct epromconfig upeeprom[]={
  {UP_EPROM_2764,    64,    0, UP_27XX_PGM28, UP_27XX_PGM28|UP_27XX_VPP28, 28, 28, 14, 1},
  {UP_EPROM_27128,  128,    0, UP_27XX_PGM28, UP_27XX_PGM28|UP_27XX_VPP28, 28, 28, 14, 1},
  {UP_EPROM_27256,  256,    0, 0            , 0                          , 28, 28, 14, 1},
  {UP_EPROM_27512,  512,    0, 0            , 0                          , 28, 28, 14, 1},
  {UP_EPROM_27010, 1024,    0, UP_27XX_PGM32, UP_27XX_PGM32|UP_27XX_VPP32, 32, 32, 16, 1},
  {UP_EPROM_27020, 2048, 1024, UP_27XX_PGM32, UP_27XX_PGM32|UP_27XX_VPP32, 32, 32, 16, 1},
  {UP_EPROM_27040, 4096, 1024, 0            , 0                          , 32, 32, 16, 1},
  {UP_EPROM_27080, 8192, 1024, 0            , 0                          , 32, 32, 16, 1},
  {UP_EPROM_2332LL,  32,    0, 0            , 0                          , 24, 24, 12, 1},
  {UP_EPROM_2332LH,  32,    0, 0            , 0                          , 24, 24, 12, 1},
  {UP_EPROM_2332HL,  32,    0, 0            , 0                          , 24, 24, 12, 1},
  {UP_EPROM_2332HH,  32,    0, 0            , 0                          , 24, 24, 12, 1},
  {UP_EPROM_2364L,   64,    0, 0            , 0                          , 24, 24, 12, 1},
  {UP_EPROM_2364H,   64,    0, 0            , 0                          , 24, 24, 12, 1},

};

// write buffer to eprom
static void write27eprom(uint32_t ictype, uint32_t page, int pulse)
{
  int i,j,retry, kbit;
  uint32_t epromaddress, dutin, dutout, pgm;
  char c;
  

  if(ictype >= count_of(upeeprom))
  {
    printf("unknown EPROM\r\n");
    system_config.error = 1;
    return;
  }

  kbit=upeeprom[ictype].kbit;
  page*=upeeprom[ictype].page;
  pgm=upeeprom[ictype].pgm_write;
  up_icprint(upeeprom[ictype].pins, upeeprom[ictype].vcc, upeeprom[ictype].gnd, upeeprom[ictype].vpp);
  
  
  #if 0
  switch(ictype)
  {
    case UP_EPROM_2764:   kbit=64;                      // not tested
                          page=0;
                          pgm=UP_27XX_PGM28;
                          icprint(28, 28, 14, 1);
                          break;
    case UP_EPROM_27128:  kbit=128;                     // not in partsbin
                          page=0;
                          pgm=UP_27XX_PGM28;
                          icprint(28, 28, 14, 1);
                          break;
    case UP_EPROM_27256:  kbit=256;                     // not tested
                          page=0;
                          pgm=0;
                          icprint(28, 28, 14, 1);
                          break;
    case UP_EPROM_27512:  kbit=512;                     // not tested
                          page=0;
                          pgm=0;
                          icprint(28, 28, 14, 1);
                          break;
    case UP_EPROM_27010:  kbit=1024;                    // not tested
                          page=0;
                          pgm=UP_27XX_PGM32;
                          icprint(32, 32, 16, 1);
                          break;
    case UP_EPROM_27020:  kbit=2048;                    // not tested
                          page*=1024;
                          pgm=UP_27XX_PGM32;
                          icprint(32, 32, 16, 1);
                          break;
    case UP_EPROM_27040:  kbit=4096;                    // not tested
                          page*=1024;
                          pgm=0;
                          icprint(32, 32, 16, 1);
                          break;
    case UP_EPROM_27080:  kbit=8192;                    // not tested, not in partsbin
                          page*=1024;
                          pgm=0;
                          icprint(32, 32, 16, 1);
                          break;
    default:              printf("unknown EPROM\r\n");
                          system_config.error = 1;
                          return;
  }
  #endif
  // warning for big eproms
  if(kbit>1024)
  {
    printf("WARNING: only 1024Kbit/128Kbyte will be written, page %d selected\r\n", (page/1024));
    kbit=1024;
  }
  
  printf("Current Vdd=%d.%03d", (5*(*hw_pin_voltage_ordered[M_UP_VSENSE_VCC + 1]) / 1000), (5*(*hw_pin_voltage_ordered[M_UP_VSENSE_VCC + 1]) % 1000));
  printf(", Vpp=%d.%03d", (5*(*hw_pin_voltage_ordered[M_UP_VSENSE_VPP + 1]) / 1000), (5*(*hw_pin_voltage_ordered[M_UP_VSENSE_VPP + 1]) % 1000));
  printf("and pulse=%d\r\n", pulse);
  printf("Is this correct? y to continue\r\n");
  while(!rx_fifo_try_get(&c));
  if(c!='y')
  {
    printf("Aborted!!\r\n");
    system_config.error = 1;
    return;
  }

  // setup hardware
  up_setvpp(1);
  up_setvcc(1);

  // setup for eprom 
  up_setpullups(UP_27XX_PU);
  up_setdirection(UP_27XX_DIR);
  up_pins(UP_27XX_OE|UP_27XX_CE);
  
  // apply Vcchi Vpp
  up_setvpp(2);
  up_setvcc(2);

  for(j=page; j<(page+kbit); j++)
  {
    printf("Programming 0x%05X %c\r", j*128, rotate[j&0x07]);

    for(i=0; i<128; i++)
    {
      epromaddress=j*128+i;

      dutin=lut_27xx_lo[(epromaddress&0x0FF)];
      dutin|=lut_27xx_mi[((epromaddress>>8)&0x0FF)];
      dutin|=lut_27xx_hi[((epromaddress>>16)&0x0FF)];    
      
      dutin|=lut_27xx_dat[up_buffer[(epromaddress&0x1FFFF)]];
      
      //for(retry=device.retries; retry>0; retry--)
      for(retry=10; retry>0; retry--)
      {
        up_pins(dutin|UP_27XX_CE|UP_27XX_OE);      // inhibit
        up_setdirection(0);                        // datapins to output
        up_pins(dutin|UP_27XX_OE);                 // program
        busy_wait_us(pulse);                    // pulse
        up_pins(dutin|UP_27XX_CE|UP_27XX_OE|pgm);  // inhibit
        up_setdirection(UP_27XX_DIR);              // datapins to input
        dutout=up_pins(dutin|UP_27XX_CE);          // read
        
        if(dutout==(dutin|UP_27XX_CE)) break;
      }
      
      if(retry==0)
      {
        printf("\r\nVerify error. device is broken\r\n");
        up_setdirection(UP_27XX_DIR);
        up_pins(dutin|UP_27XX_CE|UP_27XX_OE);
        break;
      }
    }
    if(retry==0)  break;
  }
  
  printf("\r\nDone. disconnect Vpp\r\n");
  up_setdirection(UP_27XX_DIR);
  up_pins(dutin|UP_27XX_CE|UP_27XX_OE);
  up_setvpp(0);
  up_setvcc(0);
}

// read eprom
static void read27eprom(uint32_t ictype, uint32_t page, uint8_t mode)
{
  uint32_t dutin, dutout, epromaddress, pgm, temp;
  uint32_t starttime;
  int i, j, kbit;
  char c, device;
  bool blank=true, verify=true;

  if(ictype >= count_of(upeeprom))
  {
    printf("unknown EPROM\r\n");
    system_config.error = 1;
    return;
  }
  kbit=upeeprom[ictype].kbit;
  page*=upeeprom[ictype].page;
  pgm=upeeprom[ictype].pgm_read;
  up_icprint(upeeprom[ictype].pins, upeeprom[ictype].vcc, upeeprom[ictype].gnd, 33);
  
  // warning for big eproms
  if((mode!=EPROM_BLANK)&&(kbit>1024))
  {
    printf("WARNING: only 1024Kbit/128Kbyte will be read, page %d selected\r\n", (page/1024));
    kbit=1024;
  }
  
  // blankcheck the whole eprom
  if(mode==EPROM_BLANK)
  {
    page=0;
  }
 
  printf("Is this correct? y to continue\r\n");
  while(!rx_fifo_try_get(&c));
  if(c!='y')
  {
    printf("Aborted!!\r\n");
    system_config.error = 1;
    return;
  }
 
  // setup hardware 
  up_setvpp(1);
  up_setvcc(1);

  // setup for eprom
  // TODO: if Vpp is a pin make it high
  up_setpullups(UP_27XX_PU);
  up_setdirection(UP_27XX_DIR);
  up_pins(UP_27XX_OE|UP_27XX_CE);

 
  // TODO: check Vpp, Vdd
  
  // benchmark it!
  starttime=time_us_32();
  
  for(j=page; j<(page+kbit); j++)
  {
    printf("Reading EPROM 0x%05X %c\r", j*128, rotate[j&0x07]);
    
    for(i=0; i<128; i++)
    {
      epromaddress=j*128+i;
      
      dutin=pgm;
      dutin|=lut_27xx_lo[(epromaddress&0x0FF)];
      dutin|=lut_27xx_mi[((epromaddress>>8)&0x0FF)];
      dutin|=lut_27xx_hi[((epromaddress>>16)&0x0FF)];
    
      uint8_t decoded_byte;      
      dutout=up_pins(dutin);
      //temp = up_decode_bits(dutout, up_data_bus_map, count_of(up_data_bus_map));
      up_decode_bits(dutout, &decoded_byte);
      #if 0
      if(dutout&UP_27XX_D0) temp|=0x01;
      if(dutout&UP_27XX_D1) temp|=0x02;
      if(dutout&UP_27XX_D2) temp|=0x04;
      if(dutout&UP_27XX_D3) temp|=0x08;
      if(dutout&UP_27XX_D4) temp|=0x10;
      if(dutout&UP_27XX_D5) temp|=0x20;
      if(dutout&UP_27XX_D6) temp|=0x40;
      if(dutout&UP_27XX_D7) temp|=0x80;
      #endif
      
      switch(mode)
      {
        case EPROM_READ:    up_buffer[(epromaddress&0x1FFFF)]=decoded_byte;
                            break;
        case EPROM_BLANK:   if(decoded_byte!=0xFF) blank=false;
                            break;
        case EPROM_VERIFY:  if(up_buffer[(epromaddress&0x1FFFF)]!=decoded_byte) verify=false;
                            break;
        default:            break;
      }

      up_pins(dutin|UP_27XX_OE);
    }  
  }
  printf("\r\n");
  
  up_pins(UP_27XX_OE|UP_27XX_CE);
  
  switch(mode)
  {
    case EPROM_BLANK:   if(blank) printf("Device is blank\r\n");
                        else printf("Device is not blank!\r\n");
                        break;
    case EPROM_VERIFY:  if(verify) printf("Device is verified OK\r\n");
                        else printf("Device is verified not OK!\r\n");
                        break;
    default:            break;
  }

  
  printf("Took %d ms to execute\r\n", ((time_us_32()-starttime)/1000));
}

static void read23eprom(uint32_t ictype, uint8_t mode)
{
  uint32_t dutin, dutout, epromaddress, temp;
  uint32_t starttime;
  uint32_t csactive, csdeactive;
  int i, j, kbit;
  char c, device;
  bool blank=true, verify=true;

  if(ictype >= count_of(upeeprom))
  {
    printf("unknown EPROM\r\n");
    system_config.error = 1;
    return;
  }
  
  kbit=upeeprom[ictype].kbit;
  up_icprint(upeeprom[ictype].pins, upeeprom[ictype].vcc, upeeprom[ictype].gnd, 33);
 
  printf("Is this correct? y to continue\r\n");
  while(!rx_fifo_try_get(&c));
  if(c!='y')
  {
    printf("Aborted!!\r\n");
    system_config.error = 1;
    return;
  }
  
  // setup hardware 
  up_setvpp(1);
  up_setvcc(1);

  // setup for eprom
  up_setpullups(UP_23XX_PU);
  up_setdirection(UP_23XX_DIR);
  
  // cs magic. the 23xx have a configurable CS or /CS (at factory)
  csactive=0;
  csdeactive=0;
  
  // not asserted
  if((ictype==UP_EPROM_2332LL)||(ictype==UP_EPROM_2332HL)||(ictype==UP_EPROM_2364L)) csdeactive|=UP_23XX_CS1;
  if((ictype==UP_EPROM_2332LL)||(ictype==UP_EPROM_2332LH)) csdeactive|=UP_23XX_CS2;
  
  //asserted
  if((ictype==UP_EPROM_2332LH)||(ictype==UP_EPROM_2332HH)||(ictype==UP_EPROM_2364H)) csactive|=UP_23XX_CS1;
  if((ictype==UP_EPROM_2332HL)||(ictype==UP_EPROM_2332HH)) csactive|=UP_23XX_CS2;
  
  up_pins(csdeactive);
 
  // TODO: check Vpp, Vdd
  
  // benchmark it!
  starttime=time_us_32();
  
  for(j=0; j<kbit; j++)
  {
    printf("Reading EPROM 0x%05X %c\r", j*128, rotate[j&0x07]);
    
    for(i=0; i<128; i++)
    {
      epromaddress=j*128+i;
      
      dutin =lut_23xx_lo[(epromaddress&0x0FF)];
      dutin|=lut_23xx_mi[((epromaddress>>8)&0x0FF)];
    
      uint8_t decoded_byte;      
      dutout=up_pins(dutin|csactive);
      //temp = up_decode_bits(dutout, up_data_bus_map, count_of(up_data_bus_map));
      up_decode_bits(dutout, &decoded_byte);

      switch(mode)
      {
        case EPROM_READ:    up_buffer[(epromaddress&0x1FFFF)]=decoded_byte;
                            break;
        case EPROM_BLANK:   if(decoded_byte!=0xFF) blank=false;
                            break;
        case EPROM_VERIFY:  if(up_buffer[(epromaddress&0x1FFFF)]!=decoded_byte) verify=false;
                            break;
        default:            break;
      }

      up_pins(dutin|csdeactive);
    }  
  }
  printf("\r\n");
  
  up_pins(csdeactive);
  
  switch(mode)
  {
    case EPROM_BLANK:   if(blank) printf("Device is blank\r\n");
                        else printf("Device is not blank!\r\n");
                        break;
    case EPROM_VERIFY:  if(verify) printf("Device is verified OK\r\n");
                        else printf("Device is verified not OK!\r\n");
                        break;
    default:            break;
  }

  
  printf("Took %d ms to execute\r\n", ((time_us_32()-starttime)/1000));
}

// TODO: caseinsensitive
static bool up_eprom_find_type(const char* name, uint32_t* out_ictype)
{
  for (size_t i = 0; i < count_of(up_eprom_aliases); i++) {
    if (strcmp(name, up_eprom_aliases[i].name) == 0) {
      *out_ictype = up_eprom_aliases[i].ictype;
      return true;
    }
  }
  return false;
}


