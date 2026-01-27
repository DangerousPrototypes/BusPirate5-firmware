/*
    hardware 'driver' for the universal programmer 'plank' (https:// xx) 
    
    (C) Chris van Dongen 2025-26

*/

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
  
  if(debug)
  {
    printf(" pull() ");
    printbin(pullups);
    printf("\r\n");
  }
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
  
  if(debug)
  {
    printf(" dir()  ");
    printbin(iodir);
    printf("\r\n");
  }
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
  
  if(debug)
  {
    printf(" pins() ");
    printbin(pinin);
    printf("\r\n");
  }
  
  return pinin; 
}

static void setextrapinic1(bool pin)
{
  hwspi_select();
  hwspi_write((uint8_t) 0x40);  // ic1
  hwspi_write((uint8_t) 0x0A);
  hwspi_write((uint8_t) 0x08|(pin?0x00:0x02));
  hwspi_deselect();
}

static void setextrapinic2(bool pin)
{
  hwspi_select();
  hwspi_write((uint8_t) 0x42);  // ic2
  hwspi_write((uint8_t) 0x0A);
  hwspi_write((uint8_t) 0x08|(pin?0x00:0x02));
  hwspi_deselect();
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
  else setextrapinic2(voltage);
}

static void setvdd(uint8_t voltage)
{
  bio_put(PIN_VCCH, 0);
  
  if(voltage==2) bio_put(PIN_VCCH, 1);    // the schotky diode takes care of the other states 
  if(voltage>=1) setextrapinic1(0);       // power the DUT
  else setextrapinic1(1);
}

// displays how the IC should be placed in the programmer
static void icprint(int pins, int vcc, int gnd, int vpp)
{
  int i;
  char left[4], right[4];

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

void printbin(uint32_t d)
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
uint32_t setcs(uint32_t otherpins, uint32_t cs, int mode, int active)
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

  pins(dutin);
  
  return dutin;
}

uint32_t sendspi (uint32_t otherpins, uint32_t mosi, uint32_t miso, uint32_t clk, uint32_t cs, int numbits, int mode, uint32_t datain)
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
  
  pins(dutin);                          // everything is idle

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
  
  pins(dutin);                          // assert cs and new data (if necessary)
  
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

    dutout=pins(dutin);
    
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
    
    dutout=pins(dutin);
    
    if(mode&UP_SPI_CPHAMASK)          // cpha=1: sample miso
    {
      dataout<<=1;
      if(dutout&miso) dataout|=1;
    }
  }
  
  pins(dutin);                          // everything is idle

  if(mode&UP_SPI_BITORDER)
  {
    dataout=lsb_convert(dataout, numbits);
  }

  return dataout;
}

