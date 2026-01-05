

#define MCP23S17_REG_IODIR  0x00

// standard pinout
#define UP_IO01  0x01000000l
#define UP_IO02  0x02000000l
#define UP_IO03  0x04000000l
#define UP_IO04  0x08000000l
#define UP_IO05  0x10000000l
#define UP_IO06  0x20000000l
#define UP_IO07  0x40000000l
#define UP_IO08  0x80000000l
#define UP_IO09  0x00010000l
#define UP_IO10  0x00020000l
#define UP_IO11  0x00040000l
#define UP_IO12  0x00080000l
#define UP_IO13  0x00100000l
#define UP_IO14  0x00200000l
#define UP_IO15  0x00400000l
#define UP_IO16  0x00800000l
#define UP_IO17  0x00000100l
#define UP_IO18  0x00000200l
#define UP_IO19  0x00000400l
#define UP_IO20  0x00000800l
#define UP_IO21  0x00001000l
#define UP_IO22  0x00002000l
#define UP_IO23  0x00004000l
#define UP_IO24  0x00008000l
#define UP_IO25  0x00000001l
#define UP_IO26  0x00000002l
#define UP_IO27  0x00000004l
#define UP_IO28  0x00000008l
#define UP_IO29  0x00000010l
#define UP_IO30  0x00000020l
#define UP_IO31  0x00000040l
#define UP_IO32  0x00000080l

static const uint32_t matrix[] = {
UP_IO01, UP_IO02, UP_IO03, UP_IO04, UP_IO05, UP_IO06, UP_IO07, UP_IO08,
UP_IO09, UP_IO10, UP_IO11, UP_IO12, UP_IO13, UP_IO14, UP_IO15, UP_IO16,
UP_IO17, UP_IO18, UP_IO19, UP_IO20, UP_IO21, UP_IO22, UP_IO23, UP_IO24,
UP_IO25, UP_IO26, UP_IO27, UP_IO28, UP_IO29, UP_IO30, UP_IO31, UP_IO32
};


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

#define UP_27XX_VPP28 UP_IO03
#define UP_27XX_VPP32 UP_IO01
#define UP_27XX_CE    UP_IO22
#define UP_27XX_OE    UP_IO24
#define UP_27XX_PGM28 UP_IO29   // /PGM signal on 28pin devices
#define UP_27XX_PGM32 UP_IO29   // /PGM signal on 28pin devices

#define UP_27XX_PU  (UP_IO13|UP_IO14|UP_IO15|UP_IO17|UP_IO18|UP_IO19|UP_IO20|UP_IO21)
#define UP_27XX_DIR  (UP_IO13|UP_IO14|UP_IO15|UP_IO17|UP_IO18|UP_IO19|UP_IO20|UP_IO21)


// pinout for 4164/41256
// https://www.silicon-ark.co.uk/datasheets/TMS4164-datasheet-texas-instruments.pdf
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


