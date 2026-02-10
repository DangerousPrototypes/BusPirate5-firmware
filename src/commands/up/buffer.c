/*
    memorybuffer functions for the universal programmer 'plank' (https:// xx) 
    
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
#include "lib/crc/crc.h"

#include "mode/up.h"
#include "commands/up/universalprogrammer_pinout.h"
#include "commands/up/test.h"

static void dumpbuffer(uint32_t start, uint32_t len);
static void crcbuffer(uint32_t start, uint32_t len);
static void clearbuffer(uint32_t start, uint32_t len, uint8_t fillbyte);
static void readbuffer(uint32_t start, uint32_t fstart, uint32_t len, char *fname);
static void hexreadbuffer(char *fname);
static void writebuffer(uint32_t start, uint32_t len, char *fname);


enum uptest_actions_enum {
    UP_BUFFER_READ,
    UP_BUFFER_WRITE,
    UP_BUFFER_CRC,
    UP_BUFFER_CLEAR,
    UP_BUFFER_SHOW,
    UP_BUFFER_HEXREAD,

};

static const struct cmdln_action_t uptest_actions[] = {
    {UP_BUFFER_READ, "read"},
    {UP_BUFFER_WRITE, "write"},
    {UP_BUFFER_CRC, "crc"},
    {UP_BUFFER_CLEAR, "clear"},
    {UP_BUFFER_SHOW, "show"},
    {UP_BUFFER_HEXREAD, "hexread"},
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

void up_buffer_handler(struct command_result* res)
{
    uint32_t action;
    uint32_t boffset, foffset, doffset, length, value;
    uint8_t clearbyte;
    command_var_t arg;
    char fname[13];  
    
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
    
    if(cmdln_args_find_flag_uint32('o', &arg, &value)) boffset=value; // bufferoffset
    else boffset=0;
    
    if(cmdln_args_find_flag_uint32('O', &arg, &value)) foffset=value; // fileoffset
    else foffset=0;
    
    if(cmdln_args_find_flag_uint32('l', &arg, &value)) length=value; // length
    else length=128*1024;

    if(cmdln_args_find_flag_uint32('b', &arg, &value)) clearbyte=(value&0x0FF); // clearbyte
    else clearbyte=0x00;
    
    if((action==UP_BUFFER_READ|action==UP_BUFFER_WRITE|action==UP_BUFFER_HEXREAD)&&(!cmdln_args_find_flag_string('f', &arg, 13, fname)))
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

    switch(action)
    {
      case UP_BUFFER_READ:
        readbuffer(boffset, foffset, length, fname);
        break;
      case UP_BUFFER_WRITE:
        writebuffer(boffset, length, fname);
        break;
      case UP_BUFFER_CRC:
        crcbuffer(boffset, length);
        break;
      case UP_BUFFER_SHOW:
        dumpbuffer(boffset, length);
        break;
      case UP_BUFFER_CLEAR:
        readbuffer(boffset, foffset, length, fname);
        break;
      case UP_BUFFER_HEXREAD:
        hexreadbuffer(fname);
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
      printf("%02X ", up_buffer[start+j+i]);
    }

    printf("  ");

    for(i=0; i<16; i++)
    {
      printf("%c", ((up_buffer[start+j+i]>=0x20)&(up_buffer[start+j+i]<0x7F))?up_buffer[start+j+i]:'.');
    }
    
    printf("\r\n");
  }
}

static void crcbuffer(uint32_t start, uint32_t len)
{
  printf("Checksum of the buffer from 0x%05X, len %d\r\n", start, len);
  printf("CRC16 (CCITT) = 0x%04X\r\n", ccitt16_updcrc(-1, up_buffer+start, len));
  printf("CRC32 (CCITT) = 0x%08X\r\n", ccitt32_updcrc(-1, up_buffer+start, len));
  printf("ZIPCRC = 0x%08X\r\n", zip_updcrc(-1, up_buffer+start, len));
}

static void clearbuffer(uint32_t start, uint32_t len, uint8_t fillbyte)
{
  int i;
  
  for(i=start; i<start+len; i++) up_buffer[i]=fillbyte;
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
  if (f_read(&file_handle, up_buffer+start, len, &bytes_read) == FR_OK)
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

// reads intelhex file

static uint32_t parsehex(char c)
{
  if((c>='0')&&(c<='9')) return (c-0x30);
  else if((c>='a')&&(c<='f')) return ((c-0x61)+10);
  else if((c>='A')&&(c<='F')) return ((c-0x41)+10);
  else if(c==':') return 0;
  
  return -1;
}

static void hexreadbuffer(char *fname)
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

  // read the file
  bool end, error;
  uint32_t baseaddr, offset;
  int i,j, linenr,linelength, len;
  //char line[1+2+4+2+64+2+1];    // semicolon(1), length(2), address(4), opcode(2), data(32*2), crc(2), 0x00(1)
  char line[76];
  char c;
  uint8_t checksum,recordtype;
  
  end=false;
  error=false;
  linenr=0;
  baseaddr=0;
  
  while((!end)&&(!error))
  {
    i=0;
    len=0;
    checksum=0;
    
    printf("Reading line %d %c\r\n", linenr, rotate[linenr&0x07]);
    
    while(!error)
    {
      //error=(f_read(&file_handle, &c, 1, &bytes_read) != FR_OK);
      if(f_read(&file_handle, &c, 1, &bytes_read) == FR_OK)
      {
        if(bytes_read!=0)
        {
          if(((c=='\r')||(c=='\n'))&&i==0) continue;         // try to handle linux and windows line ending
          if((c=='\r')||(c=='\n')) break;
          
          line[i++]=c;
          line[i]=0;

          if(i>(1+2+4+2+64+2+1)) error=true;            // boundry check
          if(parsehex(c)==-1) error=true;               // only 0-9a-fA-F and : are allowed
        }
        else break;
      }
      else
      {
        error=true;
      }
    }
    
    // so much can go wrong :)
    if(error)
    {
      printf("i=%d br=%d ", i, bytes_read);
      printf("Error reading from file\r\n");
      break;
    }
    
    i=0;
    
    // should start with :
    if(line[i++]!=':')
    {
      printf("No :\r\n");
      error=true;
      break;
    }
    
    // check if at least the len field is in the buffer
    linelength=strlen(line);
    if(linelength<5)      // should be at least 5  
    {
      printf("Shortline\r\n");
      error=true;
      break;
    }
    
    // parse len field
    len=((parsehex(line[i++])<<4)|(parsehex(line[i++])));
    
    printf("ll=%d l=%d %s\r\n", linelength, len, line);
    
    // check if the linelength is the same as len field
    if(linelength>((2*len)+11))
    {
      printf("\r\nlength doesnt match\r\n");
      error=true;
      break;
    }
    
    // parse offset
    offset=((parsehex(line[i++])<<12)|(parsehex(line[i++])<<8)|(parsehex(line[i++])<<4)|(parsehex(line[i++])));
    
    if((baseaddr+offset+len)>128*1024)
    {
      printf("\r\nAddress beyond buffer\r\n");
      error=true;
      break;
    }
    
    //parse recordtype
    recordtype=((parsehex(line[i++])<<4)|(parsehex(line[i++])));
    
    if(recordtype==0x00) // DATA
    {
      printf("data len=%d\r\n",len);
      for(j=0; j<len ;j++)
      {
        up_buffer[baseaddr+offset+j]=((parsehex(line[i++])<<4)|(parsehex(line[i++])));
        checksum+=up_buffer[baseaddr+offset+j];
      }
    }
    else if(recordtype==0x01) // end of file
    {
      if(len!=0) error=true;
      else end=true;
    }
    else if(recordtype==0x02) // extended segment address
    {
      baseaddr=((parsehex(line[i++])<<12)|(parsehex(line[i++])<<8)|(parsehex(line[i++])<<4)|(parsehex(line[i++])))<<4;
      checksum+=(baseaddr>>12)&0x0FF;
      checksum+=(baseaddr>>4)&0x0FF;
    }
    else if(recordtype==0x03) // start segment address
    {
      printf("\r\nIgnoring 0x03 jump\r\n");
      for(j=0; j<len ;j++)
      {
        checksum+=((parsehex(line[i++])<<4)|(parsehex(line[i++])));
      }
    }
    else if(recordtype==0x04) // extended linear address
    {
      baseaddr=((parsehex(line[i++])<<12)|(parsehex(line[i++])<<8)|(parsehex(line[i++])<<4)|(parsehex(line[i++])))<<16;
      checksum+=(baseaddr>>24)&0x0FF;
      checksum+=(baseaddr>>16)&0x0FF;

    }
    else if(recordtype==0x05) // start linear address
    {
      printf("\r\nIgnoring 0x05 jump\r\n");
      for(j=0; j<len ;j++)
      {
        checksum+=((parsehex(line[i++])<<4)|(parsehex(line[i++])));
      }
     }
    else
    {
      error=true;
    }
    

    // checksum
    checksum+=len;
    checksum+=(offset>>8);
    checksum+=offset&0x0FF;
    checksum+=recordtype;
    checksum+=((parsehex(line[i++])<<4)|(parsehex(line[i++])));

    if(checksum)
    {
      printf("Checksum error \r\n");
    }

    if(error)
    {
      printf("\r\nError parsing HEX file\r\n");
    }
  
    if((bytes_read==0)&&(!end))
    {
      printf("Premature end of file\r\n");
      error=true;
    }
    
    linenr++;
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

  if (f_write(&file_handle, up_buffer+start, len, &bytes_written))
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

