/*
    hardware 'driver' for the universal programmer 'plank' (https:// xx) 
    
    (C) Chris van Dongen 2025-26

*/

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
  
  printf("Tweak Vdd and Vpp to desired value. Press any key to continue\r\n");
  
  // TODO: show measured voltages
  while(!rx_fifo_try_get(&c))
  {
    printf("Vcc=%d.%03d  ", (5*(*hw_pin_voltage_ordered[PIN_VSENSE_VCC + 1]) / 1000), ((5*(*hw_pin_voltage_ordered[PIN_VSENSE_VCC + 1])) % 1000));
    printf("Vpp=%d.%03d \r", (5*(*hw_pin_voltage_ordered[PIN_VSENSE_VPP + 1]) / 1000), ((5*(*hw_pin_voltage_ordered[PIN_VSENSE_VPP + 1])) % 1000));
  }

  printf("\r\n");

  setvpp(0);
  setvcc(0);
}

// new basic test
static void up_test(void)
{
  int i;
  uint32_t dutin, dutout;
  char c;
 
  printf("Is no chip attached?\r\n");
  while(!rx_fifo_try_get(&c));
  if(c!='y')
  {
    printf("Aborted!!\r\n");
    system_config.error = 1;
    return;
  }

  // output test
  printf("1. Output test\r\n");
  setvcc(0);
  setvpp(0);
  setpullups(0);
  setdirection(0);
  pins(0);
  
  for(i=0; i<32; i++)
  {
    pins(matrix[i]);
    printf(" IO%02d = 1\r", i+1);
    while(!rx_fifo_try_get(&c));
  }
  printf("\r\n");
  
  // input test
  printf("2. Input test (pullups on, use GND)\r\n");
  setdirection(0xFFFFFFFFl);
  setpullups(0xFFFFFFFFl);
  while(!rx_fifo_try_get(&c))
  {
    dutout=pins(0);
    for(i=0; i<32; i++)
    {
      if(!(dutout&matrix[i])) printf(" IO%02d low\r", i+1);
    }
    busy_wait_us(200);
  }
  printf("\r\n");

  // Vcc voltage rail test
  // TODO; wait for new hardware and test it
  printf("3a. Vcc Voltagerail test\r\n");
  
  printf("Vcc=0\r\n");
  setvcc(0);
  while(!rx_fifo_try_get(&c));
  
  printf("Vcc=Vcc\r\n");
  setvcc(1);
  while(!rx_fifo_try_get(&c));
  
  printf("Vcc=Vcch\r\n");
  setvcc(2);
  while(!rx_fifo_try_get(&c));
  
  setvcc(0);
  
  // Vpp voltage rail test
  // TODO; wait for new hardware and test it
  printf("3b. Vpp Voltagerail test\r\n");
  
  printf("Vpp=0\r\n");
  setvpp(0);
  while(!rx_fifo_try_get(&c));
  
  printf("Vpp=Vcc\r\n");
  setvpp(1);
  while(!rx_fifo_try_get(&c));
  
  printf("Vcc=Vcch\r\n");
  setvpp(2);
  while(!rx_fifo_try_get(&c));
  
  setvpp(0);
  
  // Vcch, VppH measurement
  // TODO: wait for new hardware
  printf("3c. voltages\r\n");
  while(!rx_fifo_try_get(&c))
  {
    printf("Vcc=%d.%03d  ", (5*(*hw_pin_voltage_ordered[PIN_VSENSE_VCC + 1]) / 1000), (5*(*hw_pin_voltage_ordered[PIN_VSENSE_VCC + 1]) % 1000));
    printf("Vpp=%d.%03d \r", (5*(*hw_pin_voltage_ordered[PIN_VSENSE_VPP + 1]) / 1000), (5*(*hw_pin_voltage_ordered[PIN_VSENSE_VPP + 1]) % 1000));
  }
}

