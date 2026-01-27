/*
    ram tests for the universal programmer 'plank' (https:// xx) 
    
    (C) Chris van Dongen 2025-26

*/

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

static void testdram41(uint32_t variant)
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

uint8_t sramtest[] = { 0x00, 0xFF, 0xAA, 0x55 };

static void testsram62(uint32_t variant)
{
  int i, j, kbit, pass, test;
  uint8_t testchar;
  uint32_t sramaddress, dutin, dutout, starttime, ce2, we;
  bool ok=true;
  char c;
  
  switch(variant)
  {
    case UP_SRAM_6264:    kbit=64;
                          ce2=UP_62XX_CE2_28;
                          icprint(28, 28, 14, 33);
                          break;
    case UP_SRAM_62256:   kbit=256;
                          ce2=0;
                          icprint(28, 28, 14, 33);
                          break;
    case UP_SRAM_621024:  kbit=1024;
                          ce2=UP_62XX_CE2_32;
                          icprint(32, 32, 16, 33);
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

  setpullups(UP_27XX_PU);
  setdirection(UP_27XX_DIR);
  setvpp(0);
  setvcc(1);

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
        
        dutin|=lut_27xx_dat[testchar];
        
        pins(dutin|UP_62XX_CE1|UP_62XX_OE|UP_62XX_WE);      // deselect ic
        setdirection(0);                                    // datapins to output
        pins(dutin|UP_62XX_OE|ce2);                         // write
        pins(dutin|UP_62XX_CE1|UP_62XX_OE|UP_62XX_WE);      // deselect
        setdirection(UP_27XX_DIR);                          // datapins to input
        dutout=pins(dutin|UP_62XX_WE|ce2);                  // read back
        
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
  
  setvpp(0);
  setvcc(0);
}


