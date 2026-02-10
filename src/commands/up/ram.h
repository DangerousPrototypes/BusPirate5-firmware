/*
    ram tests for the universal programmer 'plank' (https:// xx) 
    
    (C) Chris van Dongen 2025-26

*/

// dram/sram types 0-0xff dram, 0x100+ sram 
#define UP_DRAM_4164    0
#define UP_DRAM_41256   1
#define UP_SRAM_6264    0x100
#define UP_SRAM_62256   0x101
#define UP_SRAM_621024  0x102

// pinout for 4164/41256
// https://www.silicon-ark.co.uk/datasheets/TMS4164-datasheet-texas-instruments.pdf
// also handy reference: https://pcbjunkie.net/index.php/resources/ram-info-and-cross-reference-page/
//
#define UP_DRAM41_A0    UP_IO13
#define UP_DRAM41_A1    UP_IO15
#define UP_DRAM41_A2    UP_IO14
#define UP_DRAM41_A3    UP_IO20
#define UP_DRAM41_A4    UP_IO19
#define UP_DRAM41_A5    UP_IO18
#define UP_DRAM41_A6    UP_IO21
#define UP_DRAM41_A7    UP_IO17
#define UP_DRAM41_A8    UP_IO09 // nc on 4164
#define UP_DRAM41_DI    UP_IO10
#define UP_DRAM41_DO    UP_IO22
#define UP_DRAM41_W     UP_IO11
#define UP_DRAM41_RAS   UP_IO12
#define UP_DRAM41_CAS   UP_IO23

#define UP_DRAM41_DIR   (UP_DRAM41_DO)
#define UP_DRAM41_PU    (UP_DRAM41_DO)

//pinout for 62xxx sram (databits are the same as 27xxx)
#define UP_62XX_A0    UP_IO12
#define UP_62XX_A1    UP_IO11
#define UP_62XX_A2    UP_IO10
#define UP_62XX_A3    UP_IO09
#define UP_62XX_A4    UP_IO08
#define UP_62XX_A5    UP_IO07
#define UP_62XX_A6    UP_IO06
#define UP_62XX_A7    UP_IO05
#define UP_62XX_A8    UP_IO27
#define UP_62XX_A9    UP_IO26
#define UP_62XX_A10   UP_IO23
#define UP_62XX_A11   UP_IO25
#define UP_62XX_A12   UP_IO04
#define UP_62XX_A13   UP_IO28
#define UP_62XX_A14   UP_IO03
#define UP_62XX_A15   UP_IO31
#define UP_62XX_A16   UP_IO02

#define UP_62XX_D0    UP_IO13
#define UP_62XX_D1    UP_IO14
#define UP_62XX_D2    UP_IO15
#define UP_62XX_D3    UP_IO17
#define UP_62XX_D4    UP_IO18
#define UP_62XX_D5    UP_IO19
#define UP_62XX_D6    UP_IO20
#define UP_62XX_D7    UP_IO21

#define UP_62XX_CE1     UP_IO22
#define UP_62XX_CE2_28  UP_IO28   // CE2 signal on 28pin devices
#define UP_62XX_CE2_32  UP_IO30   // CE2 signal on 32pin devices
#define UP_62XX_OE      UP_IO24
#define UP_62XX_WE      UP_IO29

#define UP_62XX_PU  (UP_IO13|UP_IO14|UP_IO15|UP_IO17|UP_IO18|UP_IO19|UP_IO20|UP_IO21)
#define UP_62XX_DIR  (UP_IO13|UP_IO14|UP_IO15|UP_IO17|UP_IO18|UP_IO19|UP_IO20|UP_IO21)

void up_ram_handler(struct command_result* res);



