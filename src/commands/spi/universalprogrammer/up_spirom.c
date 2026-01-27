/*
    spirom stuff for the universal programmer 'plank' (https:// xx) 
    
    (C) Chris van Dongen 2025-26

*/

/// --------------------------------------------------------------------- spirom helpers
void spiromreadid(void)
{
  uint32_t dutin, id, id2;
  char c;
  
  icprint(8, 8, 4, 33);
  
  printf("Is this correct? y to continue\r\n");
  while(!rx_fifo_try_get(&c));
  if(c!='y')
  {
    printf("Aborted!!\r\n");
    system_config.error = 1;
    return;
  }
  
  setpullups(UP_SPIROM_8PIN_PU);
  setdirection(UP_SPIROM_8PIN_DIR);
  setvcc(1);
  setvpp(0);
  
  dutin=(UP_SPIROM_8PIN_CS|UP_SPIROM_8PIN_WP|UP_SPIROM_8PIN_HOLD);
  pins(dutin);

  // command 0x90
  dutin=setcs(dutin, UP_SPIROM_8PIN_CS, UP_SPIMODE0_CS0_MSB, UP_CS_ACTIVE);
     sendspi(dutin, UP_SPIROM_8PIN_MOSI, UP_SPIROM_8PIN_MISO, UP_SPIROM_8PIN_CLK, 0,  8, UP_SPIMODE0_CS0_MSB, 0x90);
     sendspi(dutin, UP_SPIROM_8PIN_MOSI, UP_SPIROM_8PIN_MISO, UP_SPIROM_8PIN_CLK, 0, 24, UP_SPIMODE0_CS0_MSB, 0);
  id=sendspi(dutin, UP_SPIROM_8PIN_MOSI, UP_SPIROM_8PIN_MISO, UP_SPIROM_8PIN_CLK, 0, 16, UP_SPIMODE0_CS0_MSB, 0);
  dutin=setcs(dutin, UP_SPIROM_8PIN_CS, UP_SPIMODE0_CS0_MSB, UP_CS_INACTIVE);
  
  // command 0x9F
  dutin=setcs(dutin, UP_SPIROM_8PIN_CS, UP_SPIMODE0_CS0_MSB, UP_CS_ACTIVE);
      sendspi(dutin, UP_SPIROM_8PIN_MOSI, UP_SPIROM_8PIN_MISO, UP_SPIROM_8PIN_CLK, 0,  8, UP_SPIMODE0_CS0_MSB, 0x9F);
  id2=sendspi(dutin, UP_SPIROM_8PIN_MOSI, UP_SPIROM_8PIN_MISO, UP_SPIROM_8PIN_CLK, 0, 24, UP_SPIMODE0_CS0_MSB, 0);
  dutin=setcs(dutin, UP_SPIROM_8PIN_CS, UP_SPIMODE0_CS0_MSB, UP_CS_INACTIVE);
  
  printf(" id-0x90=%08X id-0x9F=%08X\r\n", id, id2);
  
  setvcc(0);
  setvpp(0);
}


