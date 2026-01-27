/*
    logic test for 74xx and 40xx chip for the universal programmer 'plank' (https:// xx) 
    
    (C) Chris van Dongen 2025-26

*/

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

static void print_logic_types(void)
{
  printf("Supported logic IC types:\r\n");
  for (size_t t = 0; t < count_of(up_logic_tables); t++) {
    const up_logic_table_desc_t* lt = &up_logic_tables[t];
    printf("%s%d-pin types:%s\r\n", ui_term_color_info(), lt->numpins, ui_term_color_reset());
    for (size_t i = 0; i < lt->count; i++) {
      printf("%s\t", lt->table[i].name);
    }
    printf("\r\n\r\n");
  }
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


