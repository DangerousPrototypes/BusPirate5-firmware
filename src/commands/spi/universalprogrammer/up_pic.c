/*
    microchip pic functions for the universal programmer 'plank' (https:// xx) 
    
    (C) Chris van Dongen 2025-26

*/


/// --------------------------------------------------------------------- microchip helpers
// move to inline?
void picsendcmd(uint32_t cmd)
{
  sendspi(0, UP_PIC_8PIN_PDAT, 0, UP_PIC_8PIN_PCLK, 0, 6, UP_SPIMODE1_CS0_LSB, cmd);
}

void picsenddata(uint32_t dat)
{
  sendspi(0, UP_PIC_8PIN_PDAT, 0, UP_PIC_8PIN_PCLK, 0, 16, UP_SPIMODE1_CS0_LSB, dat);
}

uint32_t picreaddata(void)
{
  uint32_t data;
  
  setdirection(UP_PIC_8PIN_DIR|UP_PIC_8PIN_PDAT);
  data=sendspi(0, 0, UP_PIC_8PIN_PDAT, UP_PIC_8PIN_PCLK, 0, 16, UP_SPIMODE1_CS0_LSB, 0);
  setdirection(UP_PIC_8PIN_DIR);

  return data;
}


// only tested with 12f629
// TODO: find the pics in the icinf.xml and export
void picreadids(void)
{
  uint32_t id1, id2, id3, id4, id5, devid, revid;
  char c;

  icprint(8, 1, 8, 4);
  
  printf("Is this correct? y to continue\r\n");
  while(!rx_fifo_try_get(&c));
  if(c!='y')
  {
    printf("Aborted!!\r\n");
    system_config.error = 1;
    return;
  }

  setvcc(0);
  setvpp(0);

  setpullups(UP_PIC_8PIN_PU);
  setdirection(UP_PIC_8PIN_DIR);
  pins(0);
  setvpp(2);
  busy_wait_us(1000);
  setvcc(1);  
  
  // TODO: remove later
  while(!rx_fifo_try_get(&c));
  
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

