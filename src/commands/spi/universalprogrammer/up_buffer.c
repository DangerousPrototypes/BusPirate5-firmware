/*
    memorybuffer functions for the universal programmer 'plank' (https:// xx) 
    
    (C) Chris van Dongen 2025-26

*/

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
        upbuffer[baseaddr+offset+j]=((parsehex(line[i++])<<4)|(parsehex(line[i++])));
        checksum+=upbuffer[baseaddr+offset+j];
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

