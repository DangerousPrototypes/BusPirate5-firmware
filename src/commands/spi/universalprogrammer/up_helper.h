/*
    hardware 'driver' for the universal programmer 'plank' (https:// xx) 
    
    (C) Chris van Dongen 2025-26

*/

// hardware
static void init_up(void);
static void setpullups(uint32_t pullups);
static void setdirection(uint32_t iodir);
static uint32_t pins(uint32_t pinout);
static void claimextrapins(void);
static void unclaimextrapins(void);
static void setvpp(uint8_t voltage);
static void setvdd(uint8_t voltage);
#define setvcc(x) setvdd(x);
static uint32_t sendspi (uint32_t otherpins, uint32_t mosi, uint32_t miso, uint32_t clk, uint32_t cs, int numbits, int mode, uint32_t datain);
static uint32_t setcs(uint32_t otherpins, uint32_t cs, int mode, int active);
static void setextrapinic1(bool pin);
static void setextrapinic2(bool pin);

// usefull info for the user
static void icprint(int pins, int vcc, int gnd, int vpp);
void printbin(uint32_t d);


// spi defines
// MSB
#define UP_SPIMODE0_CS0_MSB 0   // /cs, cpol=0, cpha=0
#define UP_SPIMODE1_CS0_MSB 1   // /cs, cpol=0, cpha=1
#define UP_SPIMODE2_CS0_MSB 2   // /cs, cpol=1, cpha=0
#define UP_SPIMODE3_CS0_MSB 3   // /cs, cpol=1, cpha=1

#define UP_SPIMODE0_CS1_MSB 4   // cs, cpol=0, cpha=0
#define UP_SPIMODE1_CS1_MSB 5   // cs, cpol=0, cpha=1
#define UP_SPIMODE2_CS1_MSB 6   // cs, cpol=1, cpha=0
#define UP_SPIMODE3_CS1_MSB 7   // cs, cpol=1, cpha=1

// LSB
#define UP_SPIMODE0_CS0_LSB 8   // /cs, cpol=0, cpha=0
#define UP_SPIMODE1_CS0_LSB 9   // /cs, cpol=0, cpha=1
#define UP_SPIMODE2_CS0_LSB 10  // /cs, cpol=1, cpha=0
#define UP_SPIMODE3_CS0_LSB 11  // /cs, cpol=1, cpha=1

#define UP_SPIMODE0_CS1_LSB 12  // cs, cpol=0, cpha=0
#define UP_SPIMODE1_CS1_LSB 13  // cs, cpol=0, cpha=1
#define UP_SPIMODE2_CS1_LSB 14  // cs, cpol=1, cpha=0
#define UP_SPIMODE3_CS1_LSB 15  // cs, cpol=1, cpha=1

#define UP_SPI_BITORDER 0x08    // 0=msb first, 0=lsb first
#define UP_SPI_CSMASK   0x04    // hi=cs
#define UP_SPI_CPOLMASK 0x02
#define UP_SPI_CPHAMASK 0x01

#define UP_CS_ACTIVE         1
#define UP_CS_INACTIVE       0

// extra buspirate pins we require
static const char pin_labels[][5] = { "Vcc", "Vpp", "VccH", "VppH" };
#define PIN_VSENSE_VCC  BIO0
#define PIN_VSENSE_VPP  BIO1
#define PIN_VCCH        BIO2
#define PIN_VPPH        BIO3

