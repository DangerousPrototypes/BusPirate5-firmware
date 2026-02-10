/*
    handler for the UP command. needs the universal programmer 'plank' (https:// xx) 
    
    (C) Chris van Dongen 2025-26

*/

// no volt 
#define UP_VOLT_0000  ((0<<2)|0)

// vcc/vdd
#define UP_VOLT_0300  ((3<<2)|0)
#define UP_VOLT_0325  ((3<<2)|1)
#define UP_VOLT_0350  ((3<<2)|2)
#define UP_VOLT_0375  ((3<<2)|3)
#define UP_VOLT_0400  ((4<<2)|0)
#define UP_VOLT_0425  ((4<<2)|1)
#define UP_VOLT_0450  ((4<<2)|2)
#define UP_VOLT_0475  ((4<<2)|3)
#define UP_VOLT_0500  ((5<<2)|0)
#define UP_VOLT_0525  ((5<<2)|1)
#define UP_VOLT_0550  ((5<<2)|2)
#define UP_VOLT_0575  ((5<<2)|3)
#define UP_VOLT_0600  ((6<<2)|0)
#define UP_VOLT_0625  ((6<<2)|1)
#define UP_VOLT_0650  ((6<<2)|2)
#define UP_VOLT_0675  ((6<<2)|3)

//vpp
#define UP_VOLT_1200  ((12<<2)|0)
#define UP_VOLT_1225  ((12<<2)|1)
#define UP_VOLT_1250  ((12<<2)|2)
#define UP_VOLT_1275  ((12<<2)|3)
#define UP_VOLT_1300  ((13<<2)|0)
#define UP_VOLT_1325  ((13<<2)|1)
#define UP_VOLT_1350  ((13<<2)|2)
#define UP_VOLT_1375  ((13<<2)|3)
#define UP_VOLT_1400  ((14<<2)|0)
#define UP_VOLT_1425  ((14<<2)|1)
#define UP_VOLT_1450  ((14<<2)|2)
#define UP_VOLT_1475  ((14<<2)|3)
#define UP_VOLT_1500  ((15<<2)|0)
#define UP_VOLT_1525  ((15<<2)|1)
#define UP_VOLT_1550  ((15<<2)|2)
#define UP_VOLT_1575  ((15<<2)|3)
#define UP_VOLT_1600  ((16<<2)|0)
#define UP_VOLT_1625  ((16<<2)|1)
#define UP_VOLT_1650  ((16<<2)|2)
#define UP_VOLT_1675  ((16<<2)|3)
#define UP_VOLT_1700  ((17<<2)|0)
#define UP_VOLT_1725  ((17<<2)|1)
#define UP_VOLT_1750  ((17<<2)|2)
#define UP_VOLT_1775  ((17<<2)|3)
#define UP_VOLT_1800  ((18<<2)|0)
#define UP_VOLT_1825  ((18<<2)|1)
#define UP_VOLT_1850  ((18<<2)|2)
#define UP_VOLT_1875  ((18<<2)|3)
#define UP_VOLT_1900  ((19<<2)|0)
#define UP_VOLT_1925  ((19<<2)|1)
#define UP_VOLT_1950  ((19<<2)|2)
#define UP_VOLT_1975  ((19<<2)|3)
#define UP_VOLT_2000  ((20<<2)|0)
#define UP_VOLT_2025  ((20<<2)|1)
#define UP_VOLT_2050  ((20<<2)|2)
#define UP_VOLT_2075  ((20<<2)|3)
#define UP_VOLT_2100  ((21<<2)|0)
#define UP_VOLT_2125  ((21<<2)|1)
#define UP_VOLT_2150  ((21<<2)|2)
#define UP_VOLT_2175  ((21<<2)|3)
#define UP_VOLT_2200  ((22<<2)|0)
#define UP_VOLT_2225  ((22<<2)|1)
#define UP_VOLT_2250  ((22<<2)|2)
#define UP_VOLT_2275  ((22<<2)|3)
#define UP_VOLT_2300  ((23<<2)|0)
#define UP_VOLT_2325  ((23<<2)|1)
#define UP_VOLT_2350  ((23<<2)|2)
#define UP_VOLT_2375  ((23<<2)|3)
#define UP_VOLT_2400  ((24<<2)|0)
#define UP_VOLT_2425  ((24<<2)|1)
#define UP_VOLT_2450  ((24<<2)|2)
#define UP_VOLT_2475  ((24<<2)|3)
#define UP_VOLT_2500  ((25<<2)|0)

#define UP_TYPE_27XXX  0
#define UP_TYPE_25XXX  1


// eprom only for now
// todo: extend with other stuff
typedef struct up_device {
  char      name[16];
  uint16_t  mnameid;
  uint8_t   id1;
  uint8_t   id2;
  uint8_t   pins;
  uint8_t   type;
  uint8_t   Vdd;
  uint8_t   Vcc;
  uint8_t   Vpp;
  uint16_t  pulse;
  uint8_t   retries;
  uint32_t  size;           //optional?
} up_device;




