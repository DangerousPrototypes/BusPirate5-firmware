


#include <stdio.h>
#include <string.h>
#include "pico/stdlib.h"
#include "pirate.h"
#include "system_config.h"
#include "command_struct.h"
#include "bytecode.h"   // Bytecode structure for data IO
#include "pirate/bio.h" // Buffered pin IO functions
#include "ui/ui_help.h"
#include "ui/ui_term.h"

#include "hardware/spi.h"
#include "pirate/hwspi.h"
//#include "mode/hwspi.h"
#include "pirate/mem.h"
#include "pirate/lsb.h"

#include "commands/global/w_psu.h"
#include "pirate/psu.h"

#include "commands/up/test.h"
#include "commands/up/logicic.h"
#include "commands/up/pic.h"
#include "commands/up/ram.h"
#include "commands/up/display.h"
#include "commands/up/spirom.h"
#include "commands/up/eprom.h"
#include "commands/up/buffer.h"
#include "up.h"


uint8_t *up_buffer=NULL;
bool up_verbose=true;
bool up_debug=false;      // print the bits on the ZIF socket
const char rotate[] = "|/-\\|/-\\";


// command configuration
const struct _mode_command_struct up_commands[] = {
{   .command="test", 
        .func=&up_test_handler, 
        .description_text=T_HELP_SPI_UP, 
        .supress_fala_capture=true
    },
    {   .command="logicic", 
        .func=&up_logic_handler, 
        .description_text=T_HELP_SPI_UP, 
        .supress_fala_capture=true
    },
    {   .command="pic", 
        .func=&up_pic_handler, 
        .description_text=T_HELP_SPI_UP, 
        .supress_fala_capture=true
    },
    {   .command="ram", 
        .func=&up_ram_handler, 
        .description_text=T_HELP_SPI_UP, 
        .supress_fala_capture=true
    },
    {   .command="display", 
        .func=&up_display_handler, 
        .description_text=T_HELP_SPI_UP, 
        .supress_fala_capture=true
    },
    {   .command="spirom", 
        .func=&up_spirom_handler, 
        .description_text=T_HELP_SPI_UP, 
        .supress_fala_capture=true
    },
    {   .command="eprom", 
        .func=&up_eprom_handler, 
        .description_text=T_HELP_SPI_UP, 
        .supress_fala_capture=true
    },
    {   .command="buffer", 
        .func=&up_buffer_handler, 
        .description_text=T_HELP_SPI_UP, 
        .supress_fala_capture=true
    },
};

const uint32_t up_commands_count = count_of(up_commands);

static const char pin_labels[][5] = { "Vcc", "Vpp", "VccH", "VppH", "CLK", "MOSI", "MISO", "CS" };

// Pre-setup step. Show user menus for any configuration options.
// The Bus Pirate hardware is not "clean" and reset at this point.
// Any previous mode may still be running. This is only a configuration step,
// the user may cancel out of the menu and return to the previous mode.
// Don't touch hardware yet, save the settings in variables for later.
uint32_t up_setup(void) {
    // nothing to do?
    return 1;
}

// Setup execution. This is where we actually configure any hardware.
uint32_t up_setup_exc(void) {

    uint32_t psu_result;

    // setup spi
    spi_init(M_SPI_PORT, UP_SPISPEED);
    hwspi_init(UP_NBITS, UP_CPOL, UP_CPHA);
    
    // setup BP pins
    system_bio_update_purpose_and_label(true, M_UP_VSENSE_VCC, BP_PIN_MODE, pin_labels[0]);
    system_bio_update_purpose_and_label(true, M_UP_VSENSE_VPP, BP_PIN_MODE, pin_labels[1]);
    system_bio_update_purpose_and_label(true, M_UP_VCCH, BP_PIN_MODE, pin_labels[2]);
    system_bio_update_purpose_and_label(true, M_UP_VPPH, BP_PIN_MODE, pin_labels[3]);
    system_bio_update_purpose_and_label(true, M_SPI_CLK, BP_PIN_MODE, pin_labels[4]);
    system_bio_update_purpose_and_label(true, M_SPI_CDO, BP_PIN_MODE, pin_labels[5]);
    system_bio_update_purpose_and_label(true, M_SPI_CDI, BP_PIN_MODE, pin_labels[6]);
    system_bio_update_purpose_and_label(true, M_SPI_CS, BP_PIN_MODE, pin_labels[7]);
    hwspi_deselect();
    
    bio_input(M_UP_VSENSE_VCC);
    system_set_active(true, M_UP_VSENSE_VCC, &system_config.aux_active);

    bio_input(M_UP_VSENSE_VPP);
    system_set_active(true, M_UP_VSENSE_VPP, &system_config.aux_active);
  
    bio_output(M_UP_VCCH);
    bio_put(M_UP_VCCH, 0);
    system_set_active(true, M_UP_VCCH, &system_config.aux_active);
  
    bio_output(M_UP_VPPH);
    bio_put(M_UP_VPPH, 0); 
    system_set_active(true, M_UP_VPPH, &system_config.aux_active);

    // psu stuff
    psu_result = psucmd_enable(5.0f, 300.0f, false, 10);
    
    switch(psu_result)
    {
      case PSU_ERROR_FUSE_TRIPPED:
          ui_help_error(T_PSU_CURRENT_LIMIT_ERROR);
          break;    
      case PSU_ERROR_VOUT_LOW:
          ui_help_error(T_PSU_SHORT_ERROR);
          break;
      case PSU_ERROR_BACKFLOW:
          printf("%s\r\nError: Vout > on-board power supply. Backflow prevention activated\r\n\tIs an external voltage connected to Vout/Vref pin?\r\n%s",
            ui_term_color_warning(),
            ui_term_color_reset()); 
          break;
      default: 
    }

    if(psu_result==PSU_OK)
    {
      printf ("Warning PSU is on!!\r\n");
    }
    else
    {
      system_config.error = 1;
      return 0;
    }

    // allocate 128k buffer 
    printf("trying to allocate big buffer\r\n"); 
    up_buffer=mem_alloc(128*1024, BP_BIG_BUFFER_UP);
    if(!up_buffer)
    {
      printf("Can't allocate 128kb buffer\r\n");
      system_config.error = 1;
      return 0;
    }
    
    up_init();

    return 1;
}

// Cleanup any configuration on exit.
void up_cleanup(void) {

    up_init();
    // disable peripheral
    hwspi_deinit();
    // release pin claims
    system_bio_update_purpose_and_label(false, M_UP_VSENSE_VCC, BP_PIN_MODE, 0);
    system_bio_update_purpose_and_label(false, M_UP_VSENSE_VPP, BP_PIN_MODE, 0);
    system_bio_update_purpose_and_label(false, M_UP_VCCH, BP_PIN_MODE, 0);
    system_bio_update_purpose_and_label(false, M_UP_VPPH, BP_PIN_MODE, 0);
    system_bio_update_purpose_and_label(false, M_SPI_CLK, BP_PIN_MODE, 0);
    system_bio_update_purpose_and_label(false, M_SPI_CDO, BP_PIN_MODE, 0);
    system_bio_update_purpose_and_label(false, M_SPI_CDI, BP_PIN_MODE, 0);
    system_bio_update_purpose_and_label(false, M_SPI_CS, BP_PIN_MODE, 0);
    
    mem_free(up_buffer);
    up_buffer=NULL;
    
    psucmd_disable();

}

// Handler for any numbers the user enters (1, 0x01, 0b1) or string data "string"
// This function generally writes data out to the IO pins or a peripheral
void up_write(struct _bytecode* result, struct _bytecode* next) {
    // The result struct has data about the command the user entered.
    // next is the same, but the next command in the sequence (if any).
    // next is used to predict when to ACK/NACK in I2C mode for example
    /*
    result->out_data; Data value the user entered, up to 32 bits long
    result->bits; The bit count configuration of the command (or system default) eg 0xff.4 = 4 bits. Can be useful for
    some protocols. result->number_format; The number format the user entered df_bin, df_hex, df_dec, df_ascii, mostly
    used for post processing the results result->data_message; A reference to null terminated char string to show the
    user. result->error; Set to true to halt execution, the results will be printed up to this step
    result->error_message; Reference to char string with error message to show the user.
    result->in_data; 32 bit value returned from the mode (eg read from SPI), will be shown to user
    result->repeat; THIS IS HANDLED IN THE LAYER ABOVE US, do not implement repeats in mode functions
    */
    static const char message[] = "--UP- write()";

    // your code
    for (uint8_t i = 0; i < 8; i++) {
        // user data is in result->out_data
        bio_put(BIO5, result->out_data & (0b1 << i));
    }

    // example error
    static const char err[] = "Halting: 0xff entered";
    if (result->out_data == 0xff) {
        /*
        Error result codes.
        SERR_NONE
        SERR_DEBUG Displays error_message, does not halt execution
        SERR_INFO Displays error_message, does not halt execution
        SERR_WARN Displays error_message, does not halt execution
        SERR_ERROR Displays error_message, halts execution
        */
        result->error = SERR_ERROR; // mode error halts execution
        result->error_message = err;
        return;
    }

    // Can add a text decoration if you like (optional)
    // This is for passing ACK/NACK for I2C mode and similar
    result->data_message = message;
}

// This function is called when the user enters 'r' to read data
void up_read(struct _bytecode* result, struct _bytecode* next) {
    static const char message[] = "-UP- read()";

    // your code
    uint32_t data = bio_get(BIO7);

    result->in_data = data;         // put the read value in in_data (up to 32 bits)
    result->data_message = message; // add a text decoration if you like
}

// Handler for mode START when user enters the '[' key
void up_start(struct _bytecode* result, struct _bytecode* next) {
    static const char message[] = "-UP- start()"; // The message to show the user

    bio_put(BIO4, 1); // your code

    result->data_message = message; // return a reference to the message to show the user
}

// Handler for mode STOP when user enters the ']' key
void up_stop(struct _bytecode* result, struct _bytecode* next) {
    static const char message[] = "-UP- stop()"; // The message to show the user

    bio_put(BIO4, 0); // your code

    result->data_message = message; // return a reference to the message to show the user
}

// modes can have useful macros activated by (1) (eg macro 1)
// macros are passed from the command line directly, not through the syntax system
void up_macro(uint32_t macro) {
    printf("-UP- macro(%d)\r\n", macro);
    // your code
    switch (macro) {
        // macro (0) is always a menu of macros
        case 0:
            printf(" 0. This menu\r\n 1. Print \"Hello World!\"\r\n");
            break;
        // rick rolled!
        case 1:
            printf("Never gonna give you up\r\nNever gonna let you down\r\nNever gonna run around and desert you\r\n");
            break;
    }
}

void up_settings(void) {
    printf("-");
}

void up_help(void) {
    ui_help_mode_commands(up_commands, up_commands_count);
}

/// --------------------------------------------------------------------- Hardware helpers
// setup mcp23s17 chips
void up_init(void)
{
  // enable addressing in SPI
  hwspi_select();
  hwspi_write((uint8_t) 0x40);
  hwspi_write((uint8_t) 0x0A);
  hwspi_write((uint8_t) 0x08);
  hwspi_deselect();
  
  up_setpullups(0x00000000l);    // disable pullups
  up_setdirection(0xFFFFFFFFl);  // all inputs
  
  up_setvpp(0);
  up_setvcc(0);
}

// setup the pullups 
void up_setpullups(uint32_t pullups)
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
  
  if(up_debug)
  {
    printf(" pull() ");
    up_printbin(pullups);
    printf("\r\n");
  }
}

// setup the io-direction 
void up_setdirection(uint32_t iodir)
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
  
  if(up_debug)
  {
    printf(" dir()  ");
    up_printbin(iodir);
    printf("\r\n");
  }
}

// TODO: add read and write only functions to speed things up.
// write/read pins 
uint32_t up_pins(uint32_t pinout)
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
  
  if(up_debug)
  {
    printf(" pins() ");
    up_printbin(pinin);
    printf("\r\n");
  }
  
  return pinin; 
}

void up_setextrapinic1(bool pin)
{
  hwspi_select();
  hwspi_write((uint8_t) 0x40);  // ic1
  hwspi_write((uint8_t) 0x0A);
  hwspi_write((uint8_t) 0x08|(pin?0x00:0x02));
  hwspi_deselect();
}

void up_setextrapinic2(bool pin)
{
  hwspi_select();
  hwspi_write((uint8_t) 0x42);  // ic2
  hwspi_write((uint8_t) 0x0A);
  hwspi_write((uint8_t) 0x08|(pin?0x00:0x02));
  hwspi_deselect();
}

void up_setvpp(uint8_t voltage)
{
  bio_put(M_UP_VPPH, 0);
  
  if(voltage==2) bio_put(M_UP_VPPH, 1);    // the schotky diode takes care of the other states 
  else up_setextrapinic2(voltage);
}

void up_setvdd(uint8_t voltage)
{
  bio_put(M_UP_VCCH, 0);
  
  if(voltage==2) bio_put(M_UP_VCCH, 1);    // the schotky diode takes care of the other states 
  if(voltage>=1) up_setextrapinic1(0);       // power the DUT
  else up_setextrapinic1(1);
}

// displays how the IC should be placed in the programmer
void up_icprint(int pins, int vcc, int gnd, int vpp)
{
  int i;
  char left[4], right[4];

  vcc=((32-pins)/2)+vcc;
  gnd=((32-pins)/2)+gnd;
  vpp=((32-pins)/2)+vpp;

  if(up_verbose)
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

void up_printbin(uint32_t d)
{
  int i;
  uint32_t mask;
  
  mask=0x80000000l;
  
  for(i=0; i<32; i++)
  {
    if(d&mask) printf("1");
    else printf("0");
    
    mask>>=1;
  }
}

// spi functions
uint32_t up_setcs(uint32_t otherpins, uint32_t cs, int mode, int active)
{
  uint32_t dutin;
  
  dutin=otherpins;
  
  if(active)
  {
    if(mode&UP_SPI_CSMASK) dutin|=cs;     // cs=1
    else dutin&=~cs;                      // cs=0
  }
  else
  {
    if(mode&UP_SPI_CSMASK) dutin&=~cs;    // cs=0
    else dutin|=cs;                       // cs=1
  }

  up_pins(dutin);
  
  return dutin;
}

uint32_t up_sendspi (uint32_t otherpins, uint32_t mosi, uint32_t miso, uint32_t clk, uint32_t cs, int numbits, int mode, uint32_t datain)
{
  int i;
  uint32_t dutin, dutout, mask, dataout;

  dutin=otherpins;
  dataout=0;

  if(mode&UP_SPI_BITORDER)  //lsb_convert(uint32_t d, uint8_t num_bits);
  {
    datain=lsb_convert(datain, numbits);
  }

  mask=0x80000000l;
  mask>>=(32-numbits);

  // clk idle
  if(mode&UP_SPI_CPOLMASK) dutin|=clk;  // clk=1
  else dutin&=~clk;                     // clk=0
  
  up_pins(dutin);                          // everything is idle

  // CS asserted
  if(mode&UP_SPI_CSMASK) dutin|=cs;     // cs=1
  else dutin&=~cs;                      // cs=0
  
  // cpha=0 put new data on bus
  if(!(mode&UP_SPI_CPHAMASK))        // cpha=0 
  {
    if(datain&mask) dutin|=mosi;
    else dutin&=~mosi;
    mask>>=1;
  }
  
  up_pins(dutin);                          // assert cs and new data (if necessary)
  
  // clock numbits in and out
  for(i=0; i<numbits; i++)
  {
    // flip clock
    dutin^=clk;
    
    if(mode&UP_SPI_CPHAMASK)          // cpha=1: new data on mosi
    {
      if(datain&mask) dutin|=mosi;        // mosi=1
      else dutin&=~mosi;                  // mosi=0
      mask>>=1;
    }

    dutout=up_pins(dutin);
    
    if(!(mode&UP_SPI_CPHAMASK))       // cpha=0: sample miso
    {
      dataout<<=1;
      if(dutout&miso) dataout|=1;
    }
    
    // flip clock again
    dutin^=clk;
    
    if(!(mode&UP_SPI_CPHAMASK))       // cpha=0: new data on mosi
    {
      if(datain&mask) dutin|=mosi;
      else dutin&=~mosi;
      mask>>=1;
    }
    
    dutout=up_pins(dutin);
    
    if(mode&UP_SPI_CPHAMASK)          // cpha=1: sample miso
    {
      dataout<<=1;
      if(dutout&miso) dataout|=1;
    }
  }
  
  up_pins(dutin);                          // everything is idle

  if(mode&UP_SPI_BITORDER)
  {
    dataout=lsb_convert(dataout, numbits);
  }

  return dataout;
}



