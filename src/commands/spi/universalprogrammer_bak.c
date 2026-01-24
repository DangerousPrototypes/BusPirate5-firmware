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
#include "pirate/hwspi.h"

#include "pirate/bio.h"
#include "usb_rx.h"
#include "pirate/mem.h"
#include "commands/spi/universalprogrammer.h"
#include "commands/spi/universalprogrammer_pinout.h"
#include "commands/spi/universalprogrammer_device.h"

#include "commands/spi/up_lut27xx.h"
#include "commands/spi/up_lutdram41xx.h"
#include "commands/spi/up_logicic.h"
#include "commands/spi/up_eproms.h"

// This array of strings is used to display help USAGE examples for the dummy command
static const char* const usage[] = { "dummy [init|test]\r\n\t[-b(utton)] [-i(nteger) <value>] [-f <file>]",
                                     "Initialize:%s dummy init",
                                     "Test:%s dummy test",
                                     "Test, require button press:%s dummy test -b",
                                     "Integer, value required:%s dummy -i 123",
                                     "Create/write/read file:%s dummy -f dummy.txt",
                                     "Kitchen sink:%s dummy test -b -i 123 -f dummy.txt" };

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

//forward declarations
// hardware
static void init_up(void);
static void setpullups(uint32_t pullups);
static void setdirection(uint32_t iodir);
static uint32_t pins(uint32_t pinout);
static void claimextrapins(void);
static void unclaimextrapins(void);
static void setvpp(uint8_t voltage);
static void setvdd(uint8_t voltage);
#define setvcc(x) setvdd(x);

// print how to connect
static void icprint(int pins, int vcc, int gnd, int vpp);

// tests
static void up_test(void);
static void up_vtest(void);

// eproms
static void writeeprom(uint32_t ictype, uint32_t page, int pulse);
static void readeprom(uint32_t ictype, uint32_t page, uint8_t mode);
static void readepromid(int pins);

// buffer functions
static void dumpbuffer(uint32_t start, uint32_t len);
static void crcbuffer(uint32_t start, uint32_t len);
static void clearbuffer(uint32_t start, uint32_t len, uint8_t fillbyte);
static void readbuffer(uint32_t start, uint32_t fstart, uint32_t len, char *fname); 
static void writebuffer(uint32_t start, uint32_t len, char *fname);

// logic tests
static void testlogicic(int numpins, uint16_t logicteststart, uint16_t logictestend);

// dram tests
static void writedram41(uint32_t col, uint32_t row, bool data);
static bool readdram41(uint32_t col, uint32_t row);
static void initdram41(void);
static void testdram41(uint8_t variant);

// til305 fun
static void testtil305(void);

// move to a generic crc.c/crc.h ??
#include "commands/spi/universalprogrammer/ccitt.c"
#include "commands/spi/universalprogrammer/ccitt32.c"
#include "commands/spi/universalprogrammer/zip.c"


// local vars/consts
static uint8_t *upbuffer=NULL;
static const char rotate[] = "|/-\\|/-\\";
static bool verbose=true;

static void systickeltest(void);

static const char pin_labels[][5] = { "Vcc", "Vpp", "VccH", "VppH" };
#define PIN_VSENSE_VCC  BIO0
#define PIN_VSENSE_VPP  BIO1
#define PIN_VCCH        BIO2
#define PIN_VPPH        BIO3


enum up_actions_enum {
    UP_TEST,
    UP_VTEST,
    UP_TIL305,
    UP_DRAM,
    UP_LOGIC,
    UP_BUFFER,
    UP_EEPROM,
};

static const struct cmdln_action_t eeprom_actions[] = {
    {UP_TEST, "test"},
    {UP_VTEST, "vtest"},
    {UP_TIL305, "til305"},
    {UP_DRAM, "dram"},  
    {UP_LOGIC, "logic"},
    {UP_BUFFER, "buffer"},
    {UP_EEPROM, "eprom"},
};    

// Single descriptor for all logic tables
typedef struct {
  const up_logic* table;
  size_t count;
  int numpins;
} up_logic_table_desc_t;

static const up_logic_table_desc_t up_logic_tables[] = {
  {logicic14, count_of(logicic14), 14},
  {logicic16, count_of(logicic16), 16},
  {logicic20, count_of(logicic20), 20},
  {logicic24, count_of(logicic24), 24},
  {logicic28, count_of(logicic28), 28},
  {logicic40, count_of(logicic40), 40},
};

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

typedef struct {
  const char* name;
  uint32_t ictype;
} up_eprom_alias_t;

static const up_eprom_alias_t up_eprom_aliases[] = {
  {"2764",   UP_EPROM_2764},  {"27C64",  UP_EPROM_2764},
  {"27128",  UP_EPROM_27128}, {"27C128", UP_EPROM_27128},
  {"27256",  UP_EPROM_27256}, {"27C256", UP_EPROM_27256},
  {"27512",  UP_EPROM_27512}, {"27C512", UP_EPROM_27512},
  {"27010",  UP_EPROM_27010}, {"27C010", UP_EPROM_27010},
  {"27020",  UP_EPROM_27020}, {"27C020", UP_EPROM_27020},
  {"27040",  UP_EPROM_27040}, {"27C040", UP_EPROM_27040},
  {"27080",  UP_EPROM_27080}, {"27C080", UP_EPROM_27080},
};

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
    bool eprom = false, test = false, logic=false, vtest=false, dram=false;             // some flags to keep track of what we want to do
    command_var_t arg; 
    char type[10];
    uint32_t pins=0, kbit=0, ictype=0, pulse;
    uint32_t boffset, foffset, doffset, length, id, page;
    bool read=false, write=false, readid=false, blank=false, verify=false;    // eprom flags
    bool clear=false, crc=false, show=false;
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
    
    //if (cmdln_args_string_by_position(1, sizeof(action_str), action_str)) {

    // universal
    if(cmdln_args_find_flag('q')) verbose=false;
        else verbose=true;

    switch(action){
      case UP_TEST:
        test = true;
        up_test();
        break;
      case UP_VTEST:
        vtest = true;
        up_vtest();
        break;
      case UP_TIL305:
        testtil305();
        break;
      case UP_DRAM:
        dram = true;
        
        if(cmdln_args_find_flag_string('t', &arg, 10, type))
        {
          if(strcmp(type, "4164")==0) ictype=UP_DRAM_4164;
          else if(strcmp(type, "41256")==0) ictype=UP_DRAM_41256;
          else
          {
            printf("DRAM type unknown\r\n");
            printf(" available are: 4164, 41256\r\n");
            system_config.error = 1;
            return;
          }
        }
        else
        {
          printf("Use -t to specify DRAM type\r\n");
          system_config.error = 1;
          return;
        }
        
        testdram41(ictype);
        break;
      case UP_LOGIC:
        logic = true;          
        numpins=0;
        
        if(cmdln_args_find_flag_string('t', &arg, 10, type))
        {
          if(!up_logic_find(type, &numpins, &starttest, &endtest)){
            printf("Not found!");
            system_config.error = 1;
            return;
          }

          #if 0       
          for(i=0; i<(sizeof(logicic14)/sizeof(up_logic)); i++)
          {
            if (strcmp(type, logicic14[i].name) == 0)
            {
              numpins=14;
              starttest=logicic14[i].start;
              endtest=logicic14[i].end;
            }
          }
          for(i=0; i<(sizeof(logicic16)/sizeof(up_logic)); i++)
          {
            if (strcmp(type, logicic16[i].name) == 0)
            {
              numpins=16;
              starttest=logicic16[i].start;
              endtest=logicic16[i].end;
            }
          }
          for(i=0; i<(sizeof(logicic20)/sizeof(up_logic)); i++)
          {
            if (strcmp(type, logicic20[i].name) == 0)
            {
              numpins=20;
              starttest=logicic20[i].start;
              endtest=logicic20[i].end;
            }
          }
          for(i=0; i<(sizeof(logicic24)/sizeof(up_logic)); i++)
          {
            if (strcmp(type, logicic24[i].name) == 0)
            {
              numpins=24;
              starttest=logicic24[i].start;
              endtest=logicic24[i].end;
            }
          }
          for(i=0; i<(sizeof(logicic28)/sizeof(up_logic)); i++)
          {
            if (strcmp(type, logicic28[i].name) == 0)
            {
              numpins=28;
              starttest=logicic28[i].start;
              endtest=logicic28[i].end;
            }
          }
          for(i=0; i<(sizeof(logicic40)/sizeof(up_logic)); i++)
          {
            if (strcmp(type, logicic40[i].name) == 0)
            {
              numpins=40;
              starttest=logicic40[i].start;
              endtest=logicic40[i].end;
            }
          }
        }

        if(numpins==0)
        {
          printf("Not found!");
          system_config.error = 1;
          return;
        }
          #endif
        
          testlogicic(numpins, starttest, endtest);
        }
      break;
      case UP_BUFFER:
          if (cmdln_args_string_by_position(2, sizeof(action_str), action_str))
          {
            if (strcmp(action_str, "read") == 0) read=true;
            else if (strcmp(action_str, "write") == 0) write=true;  
            else if (strcmp(action_str, "crc") == 0) crc=true;
            else if (strcmp(action_str, "clear") == 0) clear=true;
            else if (strcmp(action_str, "show") == 0) show=true;
            else
            {
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
            
            if((read|write)&&(!cmdln_args_find_flag_string('f', &arg, 13, fname)))
            {
              printf("No filename given\r\n");
              system_config.error = 1;  printf(", Vpp=%d.%03d", (4*(*hw_pin_voltage_ordered[PIN_VSENSE_VPP + 1]) / 1000), (4*(*hw_pin_voltage_ordered[PIN_VSENSE_VPP + 1]) % 1000));

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
          }
        break;
      case UP_EEPROM:
          eprom = true;

          if (cmdln_args_string_by_position(2, sizeof(action_str), action_str))
          {
            if (strcmp(action_str, "read") == 0) read=true;
            else if (strcmp(action_str, "readid") == 0) readid=true;
            else if (strcmp(action_str, "blank") == 0) blank=true;
            else if (strcmp(action_str, "write") == 0) write=true;  
            else if (strcmp(action_str, "verify") == 0) verify=true;  
            else
            {
              printf("no action defined (read, readid, write or blank)\r\n");
              system_config.error = 1;
              return;
            }     
            
            if(cmdln_args_find_flag_string('t', &arg, 10, type))
            {
              if(!up_eprom_find_type(type, &ictype))
              {
                printf("EPROM type unknown\r\n");
                printf(" available are: 2764, 27128, 27256, 27512, 27010, 27020, 27040, 27080\r\n");
                printf("              27c64, 27c128, 27c256, 27c512, 27c010, 27c020, 27c040, 27c080\r\n");
                system_config.error = 1;
                return;
              }
              #if 0
              if(strcmp(type, "2764")==0) ictype=UP_EPROM_2764;
              else if(strcmp(type, "27c64")==0) ictype=UP_EPROM_2764;
              else if(strcmp(type, "27128")==0) ictype=UP_EPROM_27128;
              else if(strcmp(type, "27c128")==0) ictype=UP_EPROM_27128;
              else if(strcmp(type, "27256")==0) ictype=UP_EPROM_27256;
              else if(strcmp(type, "27c256")==0) ictype=UP_EPROM_27256;
              else if(strcmp(type, "27512")==0) ictype=UP_EPROM_27512;
              else if(strcmp(type, "27c512")==0) ictype=UP_EPROM_27512;
              else if(strcmp(type, "27010")==0) ictype=UP_EPROM_27010;
              else if(strcmp(type, "27c010")==0) ictype=UP_EPROM_27010;
              else if(strcmp(type, "27020")==0) ictype=UP_EPROM_27020;
              else if(strcmp(type, "27c020")==0) ictype=UP_EPROM_27020;
              else if(strcmp(type, "27040")==0) ictype=UP_EPROM_27040;
              else if(strcmp(type, "27c040")==0) ictype=UP_EPROM_27040;
              else if(strcmp(type, "27080")==0) ictype=UP_EPROM_27080;
              else if(strcmp(type, "27c080")==0) ictype=UP_EPROM_27080;
              else
              {
                printf("EPROM type unknown\r\n");
                printf(" available are: 2764, 27128, 27256, 27512, 27010, 27020, 27040, 27080\r\n");
                printf("              27c64, 27c128, 27c256, 27c512, 27c010, 27c020, 27c040, 27c080\r\n");
                system_config.error = 1;
                return;
              }
              #endif

            }
            else if(!readid)
            {
                printf("Use -t to specify EPROM type\r\n");
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
          }
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

/// --------------------------------------------------------------------- Hardware helpers
// setup mcp23s17 chips
static void init_up(void)
{
  // enable addressing in SPI
  hwspi_select();
  hwspi_write((uint8_t) 0x40);
  hwspi_write((uint8_t) 0x0A);
  hwspi_write((uint8_t) 0x08);
  hwspi_deselect();
  
  setpullups(0x00000000l);    // disable pullups
  setdirection(0xFFFFFFFFl);  // all inputs
}

// setup the pullups 
static void setpullups(uint32_t pullups)
{
  // set pullups IC1
  hwspi_select();
  hwspi_write((uint8_t) 0x40);
  hwspi_write((uint8_t) 0x0C);
  hwspi_write((uint8_t) (pullups>>24)&0x0FF);
  hwspi_write((uint8_t) (pullups>>16)&0x0FF);
  hwspi_deselect();
  
  // set pullups IC2
  hwspi_select();
  hwspi_write((uint8_t) 0x42);
  hwspi_write((uint8_t) 0x0C);
  hwspi_write((uint8_t) (pullups>>8)&0x0FF);
  hwspi_write((uint8_t) pullups&0x0FF);
  hwspi_deselect();
}

// setup the io-direction 
static void setdirection(uint32_t iodir)
{
  // set direction on IC1
  hwspi_select();
  hwspi_write((uint8_t) 0x40);
  hwspi_write((uint8_t) 0x00);
  hwspi_write((uint8_t) (iodir>>24)&0x0FF);
  hwspi_write((uint8_t) (iodir>>16)&0x0FF);
  hwspi_deselect();
  
  // set direction on IC2
  hwspi_select();
  hwspi_write((uint8_t) 0x42);
  hwspi_write((uint8_t) 0x00);
  hwspi_write((uint8_t) (iodir>>8)&0x0FF);
  hwspi_write((uint8_t) iodir&0x0FF);
  hwspi_deselect();
}

// TODO: add read and write only functions to speed things up.
// write/read pins 
static uint32_t pins(uint32_t pinout)
{
  uint32_t pinin;
  
  // set pins on IC1
  hwspi_select();
  hwspi_write((uint8_t) 0x40);
  hwspi_write((uint8_t) 0x12);
  hwspi_write((uint8_t) (pinout>>24)&0x0FF);
  hwspi_write((uint8_t) (pinout>>16)&0x0FF);
  hwspi_deselect();

  // set pins IC2
  hwspi_select();
  hwspi_write((uint8_t) 0x42);
  hwspi_write((uint8_t) 0x12);
  hwspi_write((uint8_t) (pinout>>8)&0x0FF);
  hwspi_write((uint8_t) pinout&0x0FF);
  hwspi_deselect();
  
  // read pins on IC1
  hwspi_select();
  hwspi_write((uint8_t) 0x41);
  hwspi_write((uint8_t) 0x12);
  pinin=hwspi_read();
  pinin<<=8;
  pinin|=hwspi_read();
  pinin<<=8;
  hwspi_deselect();

  // read pins IC2
  hwspi_select();
  hwspi_write((uint8_t) 0x43);
  hwspi_write((uint8_t) 0x12);
  pinin|=hwspi_read();
  pinin<<=8;
  pinin|=hwspi_read();
  hwspi_deselect();

  return pinin; 
}

static void claimextrapins(void)
{
  system_bio_update_purpose_and_label(true, PIN_VSENSE_VCC, BP_PIN_MODE, pin_labels[0]);
  system_bio_update_purpose_and_label(true, PIN_VSENSE_VPP, BP_PIN_MODE, pin_labels[1]);
  system_bio_update_purpose_and_label(true, PIN_VCCH, BP_PIN_MODE, pin_labels[2]);
  system_bio_update_purpose_and_label(true, PIN_VPPH, BP_PIN_MODE, pin_labels[3]);
  
  bio_input(PIN_VSENSE_VCC);
  system_set_active(true, PIN_VSENSE_VCC, &system_config.aux_active);

  bio_input(PIN_VSENSE_VPP);
  system_set_active(true, PIN_VSENSE_VPP, &system_config.aux_active);
  
  bio_output(PIN_VCCH);
  bio_put(PIN_VCCH, 0);
  system_set_active(true, PIN_VCCH, &system_config.aux_active);
  
  bio_output(PIN_VPPH);
  bio_put(PIN_VPPH, 0); 
  system_set_active(true, PIN_VPPH, &system_config.aux_active);
  
}

static void unclaimextrapins(void)
{
  system_bio_update_purpose_and_label(false, PIN_VSENSE_VCC, BP_PIN_MODE, pin_labels[0]);
  system_bio_update_purpose_and_label(false, PIN_VSENSE_VPP, BP_PIN_MODE, pin_labels[1]);
  system_bio_update_purpose_and_label(false, PIN_VCCH, BP_PIN_MODE, pin_labels[2]);
  system_bio_update_purpose_and_label(false, PIN_VPPH, BP_PIN_MODE, pin_labels[3]);
  
  bio_input(PIN_VSENSE_VCC);
  system_set_active(false, PIN_VSENSE_VCC, &system_config.aux_active);
  bio_input(PIN_VSENSE_VPP);
  system_set_active(false, PIN_VSENSE_VPP, &system_config.aux_active);
  bio_input(PIN_VCCH);
  system_set_active(false, PIN_VCCH, &system_config.aux_active);
  bio_input(PIN_VPPH);
  system_set_active(false, PIN_VPPH, &system_config.aux_active);
}

static void setvpp(uint8_t voltage)
{
  bio_put(PIN_VPPH, 0);
  
  if(voltage==2) bio_put(PIN_VPPH, 1);    // the schotky diode takes care of the other states 
}

static void setvdd(uint8_t voltage)
{
  bio_put(PIN_VCCH, 0);
  
  if(voltage==2) bio_put(PIN_VCCH, 1);    // the schotky diode takes care of the other states 
}

// displays how the IC should be placed in the programmer
static void icprint(int pins, int vcc, int gnd, int vpp)
{
  int i;
  char left[4], right[4];

  // magic stuff happening here
  // pinoffset=(32-numpins)/2;
  //  

  vcc=((32-pins)/2)+vcc;
  gnd=((32-pins)/2)+gnd;
  vpp=((32-pins)/2)+vpp;

  if(verbose)
  {
    if(pins==32) printf("              --_--\r\n");
    else         printf("\r\n");
    
    for(i=0; i<16; i++)
    {
      if((pins/2)==(16-i)-1)
      {
        printf(" IO%02d      -  --_--  -      IO%02d\r\n", i+1, (32-i));
      }
      else if((pins/2)>=(16-i))
      {
        if(vcc==i+1) strcpy(left, "Vcc");
        else if(gnd==i+1) strcpy(left, "GND");
        else if(vpp==i+1) strcpy(left, "Vpp");
        else strcpy(left,"[=]");
        
        if(vcc==32-i) strcpy(right, "Vcc");
        else if(gnd==32-i) strcpy(right, "GND");
        else if(vpp==32-i) strcpy(right, "Vpp");
        else strcpy(right,"[=]");
        
        printf(" IO%02d  %s --|     |-- %s  IO%02d\r\n", i+1, left, right, (32-i));
      }
      else
      {
        printf(" IO%02d      -         -      IO%02d\r\n", i+1, (32-i));
      }
    }
    printf("              -----\r\n");
  }
  else
    printf("Connect Vcc to IO%02d, Vpp to IO%02d and GND to IO%02d, Jumper all other IOs\r\n", vcc, vpp, gnd);
    
}


/// --------------------------------------------------------------------- Hardware test
// let the user tweak the voltages
static void up_vtest(void)
{
  char c;
  
  setvpp(0);
  setvcc(0);
  setpullups(0);              // disable pullups
  setdirection(0xFFFFFFFFl);  // all inputs
  
  printf("IC disconnected?\r\n");
  
  while(!rx_fifo_try_get(&c));
  if(c!='y')
  {
    printf("Aborted!!\r\n");
    system_config.error = 1;
    return;
  }
  
  setvpp(2);
  setvcc(2);
  printf("Tweak Vdd and Vpp to desired value. Press any key to continue\r\n");
  
  while(!rx_fifo_try_get(&c));

  setvpp(0);
  setvcc(0);
}

// basic hardware test
static void up_test(void)
{
  int i;
  char c;
  uint32_t pin;
  
  setpullups(0xFFFFFFFFl);    // enable pullups
  setdirection(0xFFFFFFFFl);  // all inputs

  printf("initialized, all pin input, pullups are on\r\n");

  printf("Input test\r\n\r\n");
  while(1)
  {
    printf(" pins = %08X %c\r", pins(0x00000000l), rotate[i&0x07]);
    i++;
    
    busy_wait_us(100000);
    
    if (rx_fifo_try_get(&c))
    {
      break;
    }
  }

  printf("\r\nOutput test\r\n\r\n");
  pin=0x00000001l;
  setpullups(0);              // disable pullups
  setdirection(0x00000000l);  // all outputs
  while(1)
  {
    printf(" pins = %08X %c\r", pins(pin), rotate[i&0x07]);
    i++;
    pin<<=1;
    if(pin==0) pin=0x00000001l;
    
    busy_wait_us(100000);
    
    if (rx_fifo_try_get(&c))
    {
      break;
    }
  }

  printf("\r\nVpp+Vdd test\r\n\r\n");

  while(1)
  {
    setvpp(0);
    printf(" Vpp, Vdd = 0V       \r");
    busy_wait_us(2000000);
    setvpp(1);
    printf(" Vpp, Vdd = 5V       \r");
    busy_wait_us(2000000);
    setvpp(2);
    printf(" Vpp, Vdd = 5V++     \r");
    if (rx_fifo_try_get(&c))
    {
      break;
    }
    busy_wait_us(2000000);
  }
}


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
  temp = 0;
  if(dutout&UP_27XX_D0) *temp|=0x01;
  if(dutout&UP_27XX_D1) *temp|=0x02;
  if(dutout&UP_27XX_D2) *temp|=0x04;
  if(dutout&UP_27XX_D3) *temp|=0x08;
  if(dutout&UP_27XX_D4) *temp|=0x10;
  if(dutout&UP_27XX_D5) *temp|=0x20;
  if(dutout&UP_27XX_D6) *temp|=0x40;
  if(dutout&UP_27XX_D7) *temp|=0x80;
}

/// --------------------------------------------------------------------- EPROM 27xxx functions
// read the epromid
static void readepromid(int numpins)
{
  char c;
  uint32_t temp1, temp2;
  uint8_t id1, id2;
  int i;
  bool found;
  

  // does this work on non eproms (flash,eeprom)?
  if(numpins==24) icprint(24, 24, 12, 22);  // not 2732!!
  else if(numpins==28) icprint(28, 28, 14, 24); // not 27512! 
  else if(numpins==32) icprint(32, 32, 16, 26); // not 27080!
  else
  {
    printf("wrong number of pins\r\n");
    system_config.error = 1;
    return;
  }
  
  printf("Vpp=%d.%03d \r\n", (4*(*hw_pin_voltage_ordered[PIN_VSENSE_VPP + 1]) / 1000), (4*(*hw_pin_voltage_ordered[PIN_VSENSE_VPP + 1]) % 1000));
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
  setpullups(UP_27XX_PU);
  setdirection(UP_27XX_DIR);
  pins(UP_27XX_OE|UP_27XX_CE); //vpp?
  
  // apply 12.5V to A9, vcc/vdd=5
  setvdd(1);
  setvpp(2);

  // read idcodes
  temp1=pins(UP_27XX_VPP28);            // manufacturer
  temp2=pins(UP_27XX_VPP28|UP_27XX_A0); // device

  // put device to sleep
  setvpp(0);
  pins(UP_27XX_OE|UP_27XX_CE);  // vpp??

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
  
  setvcc(0); //depower device
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
  {UP_EPROM_2764,   64,   0, UP_27XX_PGM28, UP_27XX_PGM28|UP_27XX_VPP28, 28, 28, 14, 1},
  {UP_EPROM_27128,  128,  0, UP_27XX_PGM28, UP_27XX_PGM28|UP_27XX_VPP28, 28, 28, 14, 1},
  {UP_EPROM_27256,  256,  0, 0,              28, 28, 14, 1},
  {UP_EPROM_27512,  512,  0, 0,              28, 28, 14, 1},
  {UP_EPROM_27010, 1024, 0, UP_27XX_PGM32,UP_27XX_PGM32|UP_27XX_VPP32, 32, 32, 16, 1},
  {UP_EPROM_27020, 2048, 1024, UP_27XX_PGM32,UP_27XX_PGM32|UP_27XX_VPP32, 32, 32, 16, 1},
  {UP_EPROM_27040, 4096, 1024, 0,              32, 32, 16, 1},
  {UP_EPROM_27080, 8192, 1024, 0,              32, 32, 16, 1}

};

// write buffer to eprom
static void writeeprom(uint32_t ictype, uint32_t page, int pulse)
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
  icprint(upeeprom[ictype].pins, upeeprom[ictype].vcc, upeeprom[ictype].gnd, upeeprom[ictype].vpp);
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
  
  printf("Current Vdd=%d.%03d", (4*(*hw_pin_voltage_ordered[PIN_VSENSE_VCC + 1]) / 1000), (4*(*hw_pin_voltage_ordered[PIN_VSENSE_VCC + 1]) % 1000));
  printf(", Vpp=%d.%03d", (4*(*hw_pin_voltage_ordered[PIN_VSENSE_VPP + 1]) / 1000), (4*(*hw_pin_voltage_ordered[PIN_VSENSE_VPP + 1]) % 1000));
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
  setvpp(1);
  setvcc(1);

  // setup for eprom 
  setpullups(UP_27XX_PU);
  setdirection(UP_27XX_DIR);
  pins(UP_27XX_OE|UP_27XX_CE);
  
  // apply Vcchi Vpp
  setvpp(2);
  setvcc(2);

  for(j=page; j<(page+kbit); j++)
  {
    printf("Programming 0x%05X %c\r", j*128, rotate[j&0x07]);

    for(i=0; i<128; i++)
    {
      epromaddress=j*128+i;

      dutin=lut_27xx_lo[(epromaddress&0x0FF)];
      dutin|=lut_27xx_mi[((epromaddress>>8)&0x0FF)];
      dutin|=lut_27xx_hi[((epromaddress>>16)&0x0FF)];    
      
      dutin|=lut_27xx_dat[upbuffer[(epromaddress&0x1FFFF)]];
      
      //for(retry=device.retries; retry>0; retry--)
      for(retry=10; retry>0; retry--)
      {
        pins(dutin|UP_27XX_CE|UP_27XX_OE);      // inhibit
        setdirection(0);                        // datapins to output
        pins(dutin|UP_27XX_OE);                 // program
        busy_wait_us(pulse);                    // pulse
        pins(dutin|UP_27XX_CE|UP_27XX_OE|pgm);  // inhibit
        setdirection(UP_27XX_DIR);              // datapins to input
        dutout=pins(dutin|UP_27XX_CE);          // read
        
        if(dutout==(dutin|UP_27XX_CE)) break;
      }
      
      if(retry==0)
      {
        printf("\r\nVerify error. device is broken\r\n");
        setdirection(UP_27XX_DIR);
        pins(dutin|UP_27XX_CE|UP_27XX_OE);
        break;
      }
    }
    if(retry==0)  break;
  }
  
  printf("\r\nDone. disconnect Vpp\r\n");
  setdirection(UP_27XX_DIR);
  pins(dutin|UP_27XX_CE|UP_27XX_OE);
  setvpp(0);
  setvcc(0);
  while(!rx_fifo_try_get(&c));
}



// read eprom
static void readeprom(uint32_t ictype, uint32_t page, uint8_t mode)
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
  icprint(upeeprom[ictype].pins, upeeprom[ictype].vcc, upeeprom[ictype].gnd, 33);
  #if 0
  
  switch(ictype)
  {
    case UP_EPROM_2764:   kbit=64;                      // seems ok
                          page=0;
                          pgm=UP_27XX_PGM28|UP_27XX_VPP28;
                          icprint(28, 28, 14, 33);
                          break;
    case UP_EPROM_27128:  kbit=128;                     // not in partsbin
                          page=0;
                          pgm=UP_27XX_PGM28|UP_27XX_VPP28;
                          icprint(28, 28, 14, 33);
                          break;
    case UP_EPROM_27256:  kbit=256;                     // seems ok
                          page=0;
                          pgm=0;
                          icprint(28, 28, 14, 33);
                          break;
    case UP_EPROM_27512:  kbit=512;                     // seems ok
                          page=0;
                          pgm=0;
                          icprint(28, 28, 14, 33);
                          break;
    case UP_EPROM_27010:  kbit=1024;                    // seems ok
                          page=0;
                          pgm=UP_27XX_PGM32|UP_27XX_VPP32;
                          icprint(32, 32, 16, 33);
                          break;
    case UP_EPROM_27020:  kbit=2048;                    // seems ok
                          page*=1024;
                          pgm=UP_27XX_PGM32|UP_27XX_VPP32;
                          icprint(32, 32, 16, 33);
                          break;
    case UP_EPROM_27040:  kbit=4096;
                          page*=1024;
                          pgm=UP_27XX_VPP32;
                          icprint(32, 32, 16, 33);
                          break;
    case UP_EPROM_27080:  kbit=8192;
                          page*=1024;
                          pgm=0;
                          icprint(32, 32, 16, 33);
                          break;
    default:              printf("unknown EPROM\r\n");
                          system_config.error = 1;
                          return;
  }
#endif
  
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
  setvpp(1);
  setvcc(1);

  // setup for eprom
  // TODO: if Vpp is a pin make it high
  setpullups(UP_27XX_PU);
  setdirection(UP_27XX_DIR);
  pins(UP_27XX_OE|UP_27XX_CE);

 
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
    
      temp=0;
      
      dutout=pins(dutin);
      //temp = up_decode_bits(dutout, up_data_bus_map, count_of(up_data_bus_map));

      uint8_t dutin_decoded=0;
      up_decode_bits(dutout, &dutin_decoded);
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
        case EPROM_READ:    upbuffer[(epromaddress&0x1FFFF)]=dutin_decoded;
                            break;
        case EPROM_BLANK:   if(dutin_decoded!=0xFF) blank=false;
                            break;
        case EPROM_VERIFY:  if(upbuffer[(epromaddress&0x1FFFF)]!=dutin_decoded) verify=false;
                            break;
        default:            break;
      }

      pins(dutin|UP_27XX_OE);
    }  
  }
  printf("\r\n");
  
  pins(UP_27XX_OE|UP_27XX_CE);
  
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

/// --------------------------------------------------------------------- buffer helpers
// print the buffer to the screen
static void dumpbuffer(uint32_t start, uint32_t len)
{
  int i, j;
  
  // dump to screen
  for(j=0; j<len; j+=16)
  {
    printf(" 0x%05X: ", start+j);

    for(i=0; i<16; i++)
    {
      printf("%02X ", upbuffer[start+j+i]);
    }

    printf("  ");

    for(i=0; i<16; i++)
    {
      printf("%c", ((upbuffer[start+j+i]>=0x20)&(upbuffer[start+j+i]<0x7F))?upbuffer[start+j+i]:'.');
    }
    
    printf("\r\n");
  }
}

static void crcbuffer(uint32_t start, uint32_t len)
{
  printf("Checksum of the buffer from 0x%05X, len %d\r\n", start, len);
  printf("CRC16 (CCITT) = 0x%04X\r\n", ccitt16_updcrc(-1, upbuffer+start, len));
  printf("CRC32 (CCITT) = 0x%08X\r\n", ccitt32_updcrc(-1, upbuffer+start, len));
  printf("ZIPCRC = 0x%08X\r\n", zip_updcrc(-1, upbuffer+start, len));
}

static void clearbuffer(uint32_t start, uint32_t len, uint8_t fillbyte)
{
  int i;
  
  for(i=start; i<start+len; i++) upbuffer[i]=fillbyte;
}

static void readbuffer(uint32_t start, uint32_t fstart, uint32_t len, char *fname)
{
  FIL file_handle;
  unsigned int bytes_read;

  /* Open file and read */
  // open the file
  //result = f_open(&file_handle, fname, FA_READ); // open the file for reading
  if (f_open(&file_handle, fname, FA_READ)!=FR_OK)
  {
      printf("Error opening file %s for reading\r\n", fname);
      system_config.error = true; // set the error flag
      return;
  }
  // if the file was opened
  printf("File %s opened for reading\r\n", fname);

  // check filesize

  // seek offset
  if(f_lseek(&file_handle, fstart)==FR_OK)
  {
    printf("Skipped %d bytes\r\n", fstart);
  }
  else
  {                                    
      printf("Error skipping bytes file %s\r\n", fname);
      system_config.error = true; // set the error flag
  }

  // read the file
  
  //result = f_read(&file_handle, buffer, sizeof(buffer), &bytes_read); // read the data from the file
  if (f_read(&file_handle, upbuffer+start, len, &bytes_read) == FR_OK)
  {                                              // if the read was successful
      printf("Read %d bytes from file %s\r\n", bytes_read, fname);
      
  }
  else 
  {                                     // error reading file
      printf("Error reading file %s\r\n", fname);
      system_config.error = true; // set the error flag
  }

  // close the file
  //result = f_close(&file_handle); // close the file
  if (f_close(&file_handle) != FR_OK) 
  {
      printf("Error closing file %s\r\n", fname);
      system_config.error = true; // set the error flag
      return;
  }
  // if the file was closed
  printf("File %s closed\r\n", fname);
}

static void writebuffer(uint32_t start, uint32_t len, char *fname)
{
  FIL file_handle;
//  FRESULT result;
  unsigned int bytes_written; // somewhere to store the number of bytes written

  if (f_open(&file_handle, fname, FA_CREATE_NEW | FA_WRITE))
  {                                        
    printf("Error creating file %s\r\n", fname);
    system_config.error = true;
    return;
  }

  if (f_write(&file_handle, upbuffer+start, len, &bytes_written))
  {
    printf("Error writing to file %s\r\n", fname);
    if (f_close(&file_handle))
    {
      printf("Error closing file %s after error writing to file -- reboot recommended\r\n", fname);
    }
    system_config.error = true; // set the error flag
    return;
  }

  printf("Wrote %d bytes to file %s\r\n", bytes_written, fname);

  if (f_close(&file_handle))
  {
      printf("Error closing file %s\r\n", fname);
      system_config.error = true; // set the error flag
      return;
  }

  refresh_usbmsdrive();
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

  icprint(numpins, vcc, gnd, 33);

  printf("Is this correct? y to continue\r\n");
  while(!rx_fifo_try_get(&c));
  if(c!='y')
  {
    printf("Aborted!!\r\n");
    system_config.error = 1;
    return;
  }
  
  setvpp(0);
  setvcc(1);

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
    setpullups(pullups);	
    setdirection(direction);
    
    // do we have a clock signal?
    if(clock)
    {
      dutout=pins(dutin);
      dutout=pins(dutin|clock);
      dutout=pins(dutin);
    }
    else dutout=pins(dutin);
    
    // mask dont care pins
    dutout|=ignore;
    
    printf(" Pass %d ", ((i-logicteststart)+1));
    
    if(dutout==expected) printf("OK\r\n");
    else printf("Not OK\r\n");
  }
  
  printf("Took %d ms to execute\r\n", ((time_us_32()-starttime)/1000));
  
  setvpp(0);
  setvcc(0);
}

/// --------------------------------------------------------------------- dramtest helpers
static void initdram41(void)
{
  busy_wait_us(1000);
  
  pins(UP_DRAM41_W|UP_DRAM41_RAS|UP_DRAM41_CAS                       );
  pins(UP_DRAM41_W              |UP_DRAM41_CAS                       );
  pins(UP_DRAM41_W|UP_DRAM41_RAS|UP_DRAM41_CAS                       );
  pins(UP_DRAM41_W              |UP_DRAM41_CAS                       );
  pins(UP_DRAM41_W|UP_DRAM41_RAS|UP_DRAM41_CAS                       );
  pins(UP_DRAM41_W              |UP_DRAM41_CAS                       );
  pins(UP_DRAM41_W|UP_DRAM41_RAS|UP_DRAM41_CAS                       );
  pins(UP_DRAM41_W              |UP_DRAM41_CAS                       );
  pins(UP_DRAM41_W|UP_DRAM41_RAS|UP_DRAM41_CAS                       );
  pins(UP_DRAM41_W              |UP_DRAM41_CAS                       );
  pins(UP_DRAM41_W|UP_DRAM41_RAS|UP_DRAM41_CAS                       );
}

static void writedram41(uint32_t col, uint32_t row, bool data)
{
  uint32_t dat;

  if(data) dat=UP_DRAM41_DI;
  else dat=0;
  
//  pins(UP_DRAM41_W|UP_DRAM41_RAS|UP_DRAM41_CAS                       );    // w=1, ras=1, cas=1 
  pins(UP_DRAM41_W|UP_DRAM41_RAS|UP_DRAM41_CAS|lut_dram41xx[row]    );    // w=1, ras=1, cas=1, row addres 
  pins(UP_DRAM41_W              |UP_DRAM41_CAS|lut_dram41xx[row]    );    // w=0, ras=0, cas=1, ras=hi address , data on the bus
  pins(UP_DRAM41_W              |UP_DRAM41_CAS|lut_dram41xx[col]    );    // w=0, ras=0, cas=1, ras=hi address , data on the bus
  pins(UP_DRAM41_W                            |lut_dram41xx[col]|dat);    // w=1, ras=0, cas=0, cas=low address, data on the bus
  pins(                                                          dat);    // w=1, ras=1, cas=1, data on the bus
  pins(UP_DRAM41_W|UP_DRAM41_RAS|UP_DRAM41_CAS                      );    // w=1, ras=1, cas=0
}

static bool readdram41(uint32_t col, uint32_t row)
{
  uint32_t dat;
  
      pins(UP_DRAM41_W|UP_DRAM41_RAS|UP_DRAM41_CAS|lut_dram41xx[row]     );    // w=1, ras=1, cas=1, row addres 
      pins(UP_DRAM41_W              |UP_DRAM41_CAS|lut_dram41xx[row]     );    // w=0, ras=0, cas=1, ras=hi address , data on the bus
      pins(UP_DRAM41_W              |UP_DRAM41_CAS|lut_dram41xx[col]     );    // w=0, ras=0, cas=1, ras=hi address , data on the bus
      pins(UP_DRAM41_W                            |lut_dram41xx[col]     );    // w=1, ras=0, cas=0, cas=low address, data on the bus
  dat=pins(UP_DRAM41_W                                                   );    // w=1, ras=1, cas=1, data on the bus
      pins(UP_DRAM41_W|UP_DRAM41_RAS|UP_DRAM41_CAS                       );    // w=1, ras=1, cas=0

  return (dat&UP_DRAM41_DO);
}


static void testdram41(uint8_t variant)
{
  unsigned int row,col, rowend, colend;
  bool d;
  char c;
  bool ok=1;
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
  
  icprint(16, 8, 16, 33);
  
  printf("Is this correct? y to continue\r\n");
  while(!rx_fifo_try_get(&c));
  if(c!='y')
  {
    printf("Aborted!!\r\n");
    system_config.error = 1;
    return;
  }
  
  starttime=time_us_32();

  setpullups(UP_DRAM41_PU);
  setdirection(UP_DRAM41_DIR);
  setvpp(0);
  setvcc(1);
  initdram41();
  
  // All 0s
  for(row=0; row<rowend; row++)
  {
    printf("\rAll 0s .. row 0x%03X %c ", row, rotate[(row)&0x07]);
    for(col=0; col<colend; col++)
    {
      writedram41(row, col, 0);
      if(readdram41(row, col)!=0)
      {
        break;
      }
    }
  }
 
  if(ok) printf("OK\r\n");
  else printf("NOK\r\n");

   // All 1s
  for(row=0; row<rowend; row++)
  {
    printf("\rAll 1s .. row 0x%03X %c ", row, rotate[(row)&0x07]);
    for(col=0; col<colend; col++)
    {
      writedram41(row, col, 1);
      if(readdram41(row, col)!=1)
      {
        break;
      }
    }
  }
 
  if(ok) printf("OK\r\n");
  else printf("NOK\r\n");

  // All 0101s
  d=0;
  for(row=0; row<rowend; row++)
  {
    printf("\rAll 0101s .. row 0x%03X %c ", row, rotate[(row)&0x07]);
    for(col=0; col<colend; col++)
    {
      writedram41(row, col, d);
      if(readdram41(row, col)!=d)
      {
        break;
      }
      d^=1;
    }
  }
 
  if(ok) printf("OK\r\n");
  else printf("NOK\r\n");

  // All 1010s
  d=1;
  for(row=0; row<rowend; row++)
  {
    printf("\rAll 1010s .. row 0x%03X %c ", row, rotate[(row)&0x07]);
    for(col=0; col<colend; col++)
    {
      writedram41(row, col, d);
      if(readdram41(row, col)!=d)
      {
        break;
      }
      d^=1;
    }
  }
 
  if(ok) printf("OK\r\n");
  else printf("NOK\r\n");

  printf("Took %d ms to execute\r\n", ((time_us_32()-starttime)/1000));
  
  setvpp(0);
  setvcc(0);
}

/// --------------------------------------------------------------------- til305 helpers
static void testtil305(void)
{
  int i;
  
  setpullups(0);
  setdirection(0xFFFFFFFFl&(!(UP_TIL305_COL1|UP_TIL305_COL2|UP_TIL305_COL3|UP_TIL305_COL4|UP_TIL305_COL5|UP_TIL305_ROW1|UP_TIL305_ROW2|UP_TIL305_ROW3|UP_TIL305_ROW4|UP_TIL305_ROW5|UP_TIL305_ROW6|UP_TIL305_ROW7)));

  pins(UP_TIL305_COL1               |UP_TIL305_ROW2|UP_TIL305_ROW3|UP_TIL305_ROW4|UP_TIL305_ROW5|UP_TIL305_ROW6|UP_TIL305_ROW7);  
  busy_wait_us(100000);
  pins(UP_TIL305_COL1|UP_TIL305_ROW1               |UP_TIL305_ROW3|UP_TIL305_ROW4|UP_TIL305_ROW5|UP_TIL305_ROW6|UP_TIL305_ROW7);  
  busy_wait_us(100000);
  pins(UP_TIL305_COL1|UP_TIL305_ROW1|UP_TIL305_ROW2|UP_TIL305_ROW3               |UP_TIL305_ROW5|UP_TIL305_ROW6|UP_TIL305_ROW7);  
  busy_wait_us(100000);
  pins(UP_TIL305_COL1|UP_TIL305_ROW1|UP_TIL305_ROW2|UP_TIL305_ROW3|UP_TIL305_ROW4               |UP_TIL305_ROW6|UP_TIL305_ROW7);  
  busy_wait_us(100000);
  pins(UP_TIL305_COL1|UP_TIL305_ROW1|UP_TIL305_ROW2|UP_TIL305_ROW3|UP_TIL305_ROW4|UP_TIL305_ROW5               |UP_TIL305_ROW7);  
  busy_wait_us(100000);
  pins(UP_TIL305_COL1|UP_TIL305_ROW1|UP_TIL305_ROW2|UP_TIL305_ROW3|UP_TIL305_ROW4|UP_TIL305_ROW5|UP_TIL305_ROW6               );  
  busy_wait_us(100000);

  pins(UP_TIL305_COL2               |UP_TIL305_ROW2|UP_TIL305_ROW3|UP_TIL305_ROW4|UP_TIL305_ROW5|UP_TIL305_ROW6|UP_TIL305_ROW7);  
  busy_wait_us(100000);
  pins(UP_TIL305_COL2|UP_TIL305_ROW1               |UP_TIL305_ROW3|UP_TIL305_ROW4|UP_TIL305_ROW5|UP_TIL305_ROW6|UP_TIL305_ROW7);  
  busy_wait_us(100000);
  pins(UP_TIL305_COL2|UP_TIL305_ROW1|UP_TIL305_ROW2|UP_TIL305_ROW3               |UP_TIL305_ROW5|UP_TIL305_ROW6|UP_TIL305_ROW7);  
  busy_wait_us(100000);
  pins(UP_TIL305_COL2|UP_TIL305_ROW1|UP_TIL305_ROW2|UP_TIL305_ROW3|UP_TIL305_ROW4               |UP_TIL305_ROW6|UP_TIL305_ROW7);  
  busy_wait_us(100000);
  pins(UP_TIL305_COL2|UP_TIL305_ROW1|UP_TIL305_ROW2|UP_TIL305_ROW3|UP_TIL305_ROW4|UP_TIL305_ROW5               |UP_TIL305_ROW7);  
  busy_wait_us(100000);
  pins(UP_TIL305_COL2|UP_TIL305_ROW1|UP_TIL305_ROW2|UP_TIL305_ROW3|UP_TIL305_ROW4|UP_TIL305_ROW5|UP_TIL305_ROW6               );  
  busy_wait_us(100000);

  pins(UP_TIL305_COL3               |UP_TIL305_ROW2|UP_TIL305_ROW3|UP_TIL305_ROW4|UP_TIL305_ROW5|UP_TIL305_ROW6|UP_TIL305_ROW7);  
  busy_wait_us(100000);
  pins(UP_TIL305_COL3|UP_TIL305_ROW1               |UP_TIL305_ROW3|UP_TIL305_ROW4|UP_TIL305_ROW5|UP_TIL305_ROW6|UP_TIL305_ROW7);  
  busy_wait_us(100000);
  pins(UP_TIL305_COL3|UP_TIL305_ROW1|UP_TIL305_ROW2|UP_TIL305_ROW3               |UP_TIL305_ROW5|UP_TIL305_ROW6|UP_TIL305_ROW7);  
  busy_wait_us(100000);
  pins(UP_TIL305_COL3|UP_TIL305_ROW1|UP_TIL305_ROW2|UP_TIL305_ROW3|UP_TIL305_ROW4               |UP_TIL305_ROW6|UP_TIL305_ROW7);  
  busy_wait_us(100000);
  pins(UP_TIL305_COL3|UP_TIL305_ROW1|UP_TIL305_ROW2|UP_TIL305_ROW3|UP_TIL305_ROW4|UP_TIL305_ROW5               |UP_TIL305_ROW7);  
  busy_wait_us(100000);
  pins(UP_TIL305_COL3|UP_TIL305_ROW1|UP_TIL305_ROW2|UP_TIL305_ROW3|UP_TIL305_ROW4|UP_TIL305_ROW5|UP_TIL305_ROW6               );  
  busy_wait_us(100000);

  pins(UP_TIL305_COL4               |UP_TIL305_ROW2|UP_TIL305_ROW3|UP_TIL305_ROW4|UP_TIL305_ROW5|UP_TIL305_ROW6|UP_TIL305_ROW7);  
  busy_wait_us(100000);
  pins(UP_TIL305_COL4|UP_TIL305_ROW1               |UP_TIL305_ROW3|UP_TIL305_ROW4|UP_TIL305_ROW5|UP_TIL305_ROW6|UP_TIL305_ROW7);  
  busy_wait_us(100000);
  pins(UP_TIL305_COL4|UP_TIL305_ROW1|UP_TIL305_ROW2|UP_TIL305_ROW3               |UP_TIL305_ROW5|UP_TIL305_ROW6|UP_TIL305_ROW7);  
  busy_wait_us(100000);
  pins(UP_TIL305_COL4|UP_TIL305_ROW1|UP_TIL305_ROW2|UP_TIL305_ROW3|UP_TIL305_ROW4               |UP_TIL305_ROW6|UP_TIL305_ROW7);  
  busy_wait_us(100000);
  pins(UP_TIL305_COL4|UP_TIL305_ROW1|UP_TIL305_ROW2|UP_TIL305_ROW3|UP_TIL305_ROW4|UP_TIL305_ROW5               |UP_TIL305_ROW7);  
  busy_wait_us(100000);
  pins(UP_TIL305_COL4|UP_TIL305_ROW1|UP_TIL305_ROW2|UP_TIL305_ROW3|UP_TIL305_ROW4|UP_TIL305_ROW5|UP_TIL305_ROW6               );  
  busy_wait_us(100000);

  pins(UP_TIL305_COL5               |UP_TIL305_ROW2|UP_TIL305_ROW3|UP_TIL305_ROW4|UP_TIL305_ROW5|UP_TIL305_ROW6|UP_TIL305_ROW7);  
  busy_wait_us(100000);
  pins(UP_TIL305_COL5|UP_TIL305_ROW1               |UP_TIL305_ROW3|UP_TIL305_ROW4|UP_TIL305_ROW5|UP_TIL305_ROW6|UP_TIL305_ROW7);  
  busy_wait_us(100000);
  pins(UP_TIL305_COL5|UP_TIL305_ROW1|UP_TIL305_ROW2|UP_TIL305_ROW3               |UP_TIL305_ROW5|UP_TIL305_ROW6|UP_TIL305_ROW7);  
  busy_wait_us(100000);
  pins(UP_TIL305_COL5|UP_TIL305_ROW1|UP_TIL305_ROW2|UP_TIL305_ROW3|UP_TIL305_ROW4               |UP_TIL305_ROW6|UP_TIL305_ROW7);  
  busy_wait_us(100000);
  pins(UP_TIL305_COL5|UP_TIL305_ROW1|UP_TIL305_ROW2|UP_TIL305_ROW3|UP_TIL305_ROW4|UP_TIL305_ROW5               |UP_TIL305_ROW7);  
  busy_wait_us(100000);
  pins(UP_TIL305_COL5|UP_TIL305_ROW1|UP_TIL305_ROW2|UP_TIL305_ROW3|UP_TIL305_ROW4|UP_TIL305_ROW5|UP_TIL305_ROW6               );  
  busy_wait_us(100000);

}




