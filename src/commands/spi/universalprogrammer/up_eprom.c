/*
    eprom functions for the universal programmer 'plank' (https:// xx) 
    
    (C) Chris van Dongen 2025-26

*/


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
  
  printf("Vpp=%d.%03d \r\n", (5*(*hw_pin_voltage_ordered[PIN_VSENSE_VPP + 1]) / 1000), (5*(*hw_pin_voltage_ordered[PIN_VSENSE_VPP + 1]) % 1000));
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
  {UP_EPROM_2764,    64,    0, UP_27XX_PGM28, UP_27XX_PGM28|UP_27XX_VPP28, 28, 28, 14, 1},
  {UP_EPROM_27128,  128,    0, UP_27XX_PGM28, UP_27XX_PGM28|UP_27XX_VPP28, 28, 28, 14, 1},
  {UP_EPROM_27256,  256,    0, 0            , 0                          , 28, 28, 14, 1},
  {UP_EPROM_27512,  512,    0, 0            , 0                          , 28, 28, 14, 1},
  {UP_EPROM_27010, 1024,    0, UP_27XX_PGM32, UP_27XX_PGM32|UP_27XX_VPP32, 32, 32, 16, 1},
  {UP_EPROM_27020, 2048, 1024, UP_27XX_PGM32, UP_27XX_PGM32|UP_27XX_VPP32, 32, 32, 16, 1},
  {UP_EPROM_27040, 4096, 1024, 0            , 0                          , 32, 32, 16, 1},
  {UP_EPROM_27080, 8192, 1024, 0            , 0                          , 32, 32, 16, 1}

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
  
  printf("Current Vdd=%d.%03d", (5*(*hw_pin_voltage_ordered[PIN_VSENSE_VCC + 1]) / 1000), (5*(*hw_pin_voltage_ordered[PIN_VSENSE_VCC + 1]) % 1000));
  printf(", Vpp=%d.%03d", (5*(*hw_pin_voltage_ordered[PIN_VSENSE_VPP + 1]) / 1000), (5*(*hw_pin_voltage_ordered[PIN_VSENSE_VPP + 1]) % 1000));
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
    
      uint8_t decoded_byte;      
      dutout=pins(dutin);
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
        case EPROM_READ:    upbuffer[(epromaddress&0x1FFFF)]=decoded_byte;
                            break;
        case EPROM_BLANK:   if(decoded_byte!=0xFF) blank=false;
                            break;
        case EPROM_VERIFY:  if(upbuffer[(epromaddress&0x1FFFF)]!=decoded_byte) verify=false;
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


