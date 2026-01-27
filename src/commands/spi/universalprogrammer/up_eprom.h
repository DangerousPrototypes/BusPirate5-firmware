/*
    eprom functions for the universal programmer 'plank' (https:// xx) 
    
    (C) Chris van Dongen 2025-26

*/

// (flash)eprom types
#define UP_EPROM_2764     0
#define UP_EPROM_27128    1
#define UP_EPROM_27256    2
#define UP_EPROM_27512    3
#define UP_EPROM_27010    4
#define UP_EPROM_27020    5
#define UP_EPROM_27040    6
#define UP_EPROM_27080    7

// eprom modes
#define EPROM_READ    0
#define EPROM_BLANK   1
#define EPROM_VERIFY  2

// pinout for 27xxx EPROMs
// comparison of several eprom pinouts: https://i.sstatic.net/w8kpB.png
#define UP_27XX_A0    UP_IO12
#define UP_27XX_A1    UP_IO11
#define UP_27XX_A2    UP_IO10
#define UP_27XX_A3    UP_IO09
#define UP_27XX_A4    UP_IO08
#define UP_27XX_A5    UP_IO07
#define UP_27XX_A6    UP_IO06
#define UP_27XX_A7    UP_IO05
#define UP_27XX_A8    UP_IO27
#define UP_27XX_A9    UP_IO26
#define UP_27XX_A10   UP_IO23
#define UP_27XX_A11   UP_IO25
#define UP_27XX_A12   UP_IO04
#define UP_27XX_A13   UP_IO28
#define UP_27XX_A14   UP_IO29
#define UP_27XX_A15   UP_IO03
#define UP_27XX_A16   UP_IO02
#define UP_27XX_A17   UP_IO30
#define UP_27XX_A18   UP_IO31
#define UP_27XX_A19   UP_IO01

#define UP_27XX_D0    UP_IO13
#define UP_27XX_D1    UP_IO14
#define UP_27XX_D2    UP_IO15
#define UP_27XX_D3    UP_IO17
#define UP_27XX_D4    UP_IO18
#define UP_27XX_D5    UP_IO19
#define UP_27XX_D6    UP_IO20
#define UP_27XX_D7    UP_IO21

#define UP_27XX_VPP28 UP_IO03   // vpp signal on 28pin devices
#define UP_27XX_VPP32 UP_IO01   // vpp signal on 32pin devices
#define UP_27XX_CE    UP_IO22
#define UP_27XX_OE    UP_IO24
#define UP_27XX_PGM28 UP_IO29   // /PGM signal on 28pin devices
#define UP_27XX_PGM32 UP_IO31   // /PGM signal on 32pin devices

#define UP_27XX_PU  (UP_IO13|UP_IO14|UP_IO15|UP_IO17|UP_IO18|UP_IO19|UP_IO20|UP_IO21)
#define UP_27XX_DIR  (UP_IO13|UP_IO14|UP_IO15|UP_IO17|UP_IO18|UP_IO19|UP_IO20|UP_IO21)

typedef struct {
  const char* name;
  uint32_t ictype;
} up_eprom_alias_t;

static const up_eprom_alias_t up_eprom_aliases[] = {
  {"2764",   UP_EPROM_2764},  {"27C64",  UP_EPROM_2764},
  {"27128",  UP_EPROM_27128}, {"27C128", UP_EPROM_27128},
  {"27256",  UP_EPROM_27256}, {"27C256", UP_EPROM_27256},
  {"27512",  UP_EPROM_27512}, {"27C512", UP_EPROM_27512},
  {"27010",  UP_EPROM_27010}, {"27C010", UP_EPROM_27010},
  {"27020",  UP_EPROM_27020}, {"27C020", UP_EPROM_27020},
  {"27040",  UP_EPROM_27040}, {"27C040", UP_EPROM_27040},
  {"27080",  UP_EPROM_27080}, {"27C080", UP_EPROM_27080},
};


static void writeeprom(uint32_t ictype, uint32_t page, int pulse);
static void readeprom(uint32_t ictype, uint32_t page, uint8_t mode);
static void readepromid(int pins);
static bool up_eprom_find_type(const char* name, uint32_t* out_ictype);


