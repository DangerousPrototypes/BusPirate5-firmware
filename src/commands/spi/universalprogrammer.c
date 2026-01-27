/*

    handler for the UP command. needs the universal programmer 'plank' (https:// xx) 
    
    (C) Chris van Dongen 2025-26

*/


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
#include "pirate/hwspi.h"
#include "pirate/lsb.h"
#include "ui/ui_term.h"

#include "pirate/bio.h"
#include "usb_rx.h"
#include "pirate/mem.h"

// 
#include "commands/spi/universalprogrammer.h"
#include "commands/spi/universalprogrammer_pinout.h"
#include "commands/spi/universalprogrammer_device.h"

// tables
#include "commands/spi/universalprogrammer/up_lut27xx.h"      // lookup tables for 27xx eproms
#include "commands/spi/universalprogrammer/up_eproms.h"       // eprom name database
#include "commands/spi/universalprogrammer/up_lut62xx.h"      // lookup tables for 62xx ram
#include "commands/spi/universalprogrammer/up_lutdram41xx.h"  // lookup tables for 41xx dram 
#include "commands/spi/universalprogrammer/up_lutdl1414.h"    // lookup tables for displays (currently one)
#include "commands/spi/universalprogrammer/up_logicic.h"      // logic chip database with test vectors

// move to a generic crc.c/crc.h ??
#include "commands/spi/universalprogrammer/crc_crc16.c"
#include "commands/spi/universalprogrammer/crc_crc32.c"
#include "commands/spi/universalprogrammer/crc_zip.c"

// This array of strings is used to display help USAGE examples for the dummy command
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
    0
};

// local vars/consts
static uint8_t *upbuffer=NULL;
static const char rotate[] = "|/-\\|/-\\";
static bool verbose=true;
static bool debug=false;      // print the bits on the ZIF socket

//forward declarations
// helpers
#include "commands/spi/universalprogrammer/up_helper.h"
#include "commands/spi/universalprogrammer/up_helper.c"    // want to keep the functions static not to clash with other function names

// hardware test
#include "commands/spi/universalprogrammer/up_hwtest.h"
#include "commands/spi/universalprogrammer/up_hwtest.c"    // want to keep the functions static not to clash with other function names

// eproms
#include "commands/spi/universalprogrammer/up_eprom.h"
#include "commands/spi/universalprogrammer/up_eprom.c"    // want to keep the functions static not to clash with other function names

// buffer functions
#include "commands/spi/universalprogrammer/up_buffer.h"
#include "commands/spi/universalprogrammer/up_buffer.c"    // want to keep the functions static not to clash with other function names

// logic tests
#include "commands/spi/universalprogrammer/up_logic.h"
#include "commands/spi/universalprogrammer/up_logic.c"    // want to keep the functions static not to clash with other function names

// dram/sram tests
#include "commands/spi/universalprogrammer/up_ram.h"
#include "commands/spi/universalprogrammer/up_ram.c"    // want to keep the functions static not to clash with other function names

// display fun
#include "commands/spi/universalprogrammer/up_display.h"
#include "commands/spi/universalprogrammer/up_display.c"    // want to keep the functions static not to clash with other function names

// spirom
#include "commands/spi/universalprogrammer/up_spirom.h"
#include "commands/spi/universalprogrammer/up_spirom.c"    // want to keep the functions static not to clash with other function names

// microchip pic
#include "commands/spi/universalprogrammer/up_pic.h"
#include "commands/spi/universalprogrammer/up_pic.c"    // want to keep the functions static not to clash with other function names



enum up_actions_enum {
    UP_TEST,
    UP_VTEST,
    UP_DRAM,
    UP_LOGIC,
    UP_BUFFER,
    UP_EEPROM,
    UP_SPIROM,
    UP_DISPLAY,
    UP_RAM,
    UP_PIC
};

static const struct cmdln_action_t eeprom_actions[] = {
    {UP_TEST, "test"},
    {UP_VTEST, "vtest"},
    {UP_DRAM, "dram"},  
    {UP_LOGIC, "logic"},
    {UP_BUFFER, "buffer"},
    {UP_EEPROM, "eprom"},
    {UP_SPIROM, "spirom"},
    {UP_DISPLAY, "display"},
    {UP_RAM, "ram"},
    {UP_PIC, "pic"},
};    





// magic starts here
void spi_up_handler(struct command_result* res) {
    uint32_t value; // somewhere to keep an integer value
    char fname[13];  // somewhere to keep a string value (8.3 filename + 0x00 = 13 characters max)

    // the help -h flag can be serviced by the command line parser automatically, or from within the command
    // the action taken is set by the help_text variable of the command struct entry for this command
    // 1. a single T_ constant help entry assigned in the commands[] struct in commands.c will be shown automatically
    // 2. if the help assignment in commands[] struct is 0x00, it can be handled here (or ignored)
    // res.help_flag is set by the command line parser if the user enters -h
    // we can use the ui_help_show function to display the help text we configured above
    if (ui_help_show(res->help_flag, usage, count_of(usage), &options[0], count_of(options))) {
        return;
    }

    // ui_help_check_vout_vref() checks for a valid voltage on the Vout pin
    // if the voltage is too low, the command print an error message and return false
    // checking for a voltage, if needed, and issuing an error or reminder saves a lot of frustration for everyone
    if(!ui_help_check_vout_vref())
    {
        return;
    }

    // you can have multiple parameters in multiple positions
    // we have two possible parameters: init and test, which
    // our parameter is the first argument following the command itself, but you can use it however works best
    char action_str[9];              // somewhere to store the parameter string
    command_var_t arg; 
    char type[10];
    uint32_t pins=0, kbit=0, ictype=0, pulse;
    uint32_t boffset, foffset, doffset, length, id, page;
    bool read=false, write=false, readid=false, blank=false, verify=false;    // eprom flags
    bool clear=false, crc=false, show=false, hexread=false;
    int i;
    uint8_t clearbyte;
    
    int numpins;
    uint16_t starttest, endtest;
    
    // allocate 128k buffer if not done already
    if(!upbuffer)
    {
      printf("trying to allocate big buffer\r\n"); 
      upbuffer=mem_alloc(128*1024, BP_BIG_BUFFER_UP);
      if(!upbuffer)
      {
        printf("Can't allocate 128kb buffer\r\n");
        system_config.error = 1;
        return;
      }
    }
    
    // init up
    init_up();
    claimextrapins();
    setvpp(0);
    setvdd(0);

    // common function to parse the command line verb or action
    uint32_t action;
    if(cmdln_args_get_action(eeprom_actions, count_of(eeprom_actions), &action)){
        ui_help_show(true, usage, count_of(usage), &options[0], count_of(options)); // show help if requested
        return;
    }
    
    // universal commands
    verbose = !cmdln_args_find_flag('q'); // quiet flag
    debug = cmdln_args_find_flag('d'); // debug flag
    
    switch(action){
      case UP_TEST:
        up_test();
        break;

      case UP_VTEST:
        up_vtest();
        break;

      case UP_DRAM:
        if(!cmdln_args_find_flag_string('t', &arg, 10, type))
        {
          printf("Use -t to specify DRAM type\r\n");
          system_config.error = 1;
          return;
        }


        if(strcmp(type, "4164")==0) ictype=UP_DRAM_4164;
        else if(strcmp(type, "41256")==0) ictype=UP_DRAM_41256;
        else
        {
          printf("DRAM type unknown\r\n");
          printf(" available are: 4164, 41256\r\n");
          system_config.error = 1;
          return;
        }
        testdram41(ictype);
        break;

      case UP_SPIROM:
          spiromreadid();
          break;

      case UP_PIC:
          picreadids();
          break;

      case UP_DISPLAY:
          if(!cmdln_args_find_flag_string('t', &arg, 10, type))
          {
            printf("Use -t to specify DISPLAY type\r\n");
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

      case UP_RAM:
        if(!cmdln_args_find_flag_string('t', &arg, 10, type))
        {
          printf("Use -t to specify RAM type\r\n");
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

      case UP_LOGIC:     
        numpins=0;
        
        if(!cmdln_args_find_flag_string('t', &arg, 10, type))
        {
          printf("Use -t to specify LOGIC IC type\r\n");
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
      
      case UP_BUFFER:
        if (!cmdln_args_string_by_position(2, sizeof(action_str), action_str))
        {
          goto up_buffer_error;
        }

        if (strcmp(action_str, "read") == 0) read=true;
        else if (strcmp(action_str, "write") == 0) write=true;  
        else if (strcmp(action_str, "crc") == 0) crc=true;
        else if (strcmp(action_str, "clear") == 0) clear=true;
        else if (strcmp(action_str, "show") == 0) show=true;
        else if (strcmp(action_str, "hexread") == 0) hexread=true;
        else
        {
up_buffer_error:
          printf("no action defined (read, write, crc, blank)\r\n");
          system_config.error = 1;
          return;
        }
        
        if(cmdln_args_find_flag_uint32('o', &arg, &value)) boffset=value; // bufferoffset
        else boffset=0;
        
        if(cmdln_args_find_flag_uint32('O', &arg, &value)) foffset=value; // fileoffset
        else foffset=0;
        
        if(cmdln_args_find_flag_uint32('l', &arg, &value)) length=value; // length
        else length=128*1024;

        if(cmdln_args_find_flag_uint32('b', &arg, &value)) clearbyte=(value&0x0FF); // clearbyte
        else clearbyte=0x00;
        
        if((read|write|hexread)&&(!cmdln_args_find_flag_string('f', &arg, 13, fname)))
        {
          printf("No filename given\r\n");
          system_config.error = 1;  
          return;
        }
        
        //sanity checks
        if(length>128*1024)
        {
          printf("length should be less then 131072\r\n");
          system_config.error = 1;
          return;
        }

        if(boffset>128*1024)
        {
          printf("buffer offset should be less then 131072\r\n");
          system_config.error = 1;
          return;
        }

        if((boffset+length)>128*1024)
        {
          printf("buffer offset+length should be less then 131072\r\n");
          system_config.error = 1;
          return;
        }

        
        if(show) dumpbuffer(boffset, length);
        else if(crc) crcbuffer(boffset, length);
        else if(clear) clearbuffer(boffset, length, clearbyte);
        else if(write) writebuffer(boffset, length, fname);
        else if(read) readbuffer(boffset, foffset, length, fname);
        else if(hexread) hexreadbuffer(fname);
        break;

      case UP_EEPROM:
        if (!cmdln_args_string_by_position(2, sizeof(action_str), action_str))
        {
          goto up_eeprom_error;
        }

        if (strcmp(action_str, "read") == 0) read=true;
        else if (strcmp(action_str, "readid") == 0) readid=true;
        else if (strcmp(action_str, "blank") == 0) blank=true;
        else if (strcmp(action_str, "write") == 0) write=true;  
        else if (strcmp(action_str, "verify") == 0) verify=true;  
        else
        {
up_eeprom_error:
          printf("no action defined (read, readid, write or blank)\r\n");
          system_config.error = 1;
          return;
        }     
          
        if(!readid && !cmdln_args_find_flag_string('t', &arg, 10, type))
        {
          printf("Use -t to specify EPROM type\r\n");
          system_config.error = 1;
          return;
        }              
        if(!readid && !up_eprom_find_type(type, &ictype))
        {
          printf("EPROM type unknown\r\n");
          printf(" available are: 2764, 27128, 27256, 27512, 27010, 27020, 27040, 27080\r\n");
          printf("              27c64, 27c128, 27c256, 27c512, 27c010, 27c020, 27c040, 27c080\r\n");
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
           
        if(read) readeprom(ictype, page, EPROM_READ);
        else if(readid) readepromid(pins);
        else if(write) writeeprom(ictype, page, pulse);
        else if(blank) readeprom(ictype, page, EPROM_BLANK);
        else if(verify) readeprom(ictype, page, EPROM_VERIFY);
        break;

      default: //should never get here, should throw help
        printf("No action defined (test, vtest, dram, logic, buffer, eprom)\r\n");
        system_config.error = 1;  
        return;
    }
    
    // 
    init_up();
    setvcc(0);  // to be sure
    setvpp(0);  // to be sure
}








