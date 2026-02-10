
// ditch these? 
void up_write(struct _bytecode* result, struct _bytecode* next);
void up_read(struct _bytecode* result, struct _bytecode* next);
void up_start(struct _bytecode* result, struct _bytecode* next);
void up_stop(struct _bytecode* result, struct _bytecode* next);

// passes the number in (1) = 1 for mode based macros
void up_macro(uint32_t macro);

// setup functions
uint32_t up_setup(void);
uint32_t up_setup_exc(void);
void up_cleanup(void);

// displayed in the 'i' command
void up_settings(void);
void up_help(void);

extern const struct _mode_command_struct up_commands[];
extern const uint32_t up_commands_count;

// hw settings
#define UP_SPISPEED 10500000 // 10416Khz; 10000->8928Khz
#define UP_NBITS  8
#define UP_CPOL   1
#define UP_CPHA   1

// defines for spi functions
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

// exposed functions
void up_init(void);
void up_setpullups(uint32_t pullups);
void up_setdirection(uint32_t iodir);
uint32_t up_pins(uint32_t pinout);
//void claimextrapins(void);
//void unclaimextrapins(void);
void up_setvpp(uint8_t voltage);
void up_setvdd(uint8_t voltage);
#define up_setvcc(x) up_setvdd(x);
void up_setextrapinic1(bool pin);
void up_setextrapinic2(bool pin);

// spi bitbang
uint32_t up_sendspi (uint32_t otherpins, uint32_t mosi, uint32_t miso, uint32_t clk, uint32_t cs, int numbits, int mode, uint32_t datain);
uint32_t up_setcs(uint32_t otherpins, uint32_t cs, int mode, int active);


// usefull info for the user
void up_icprint(int pins, int vcc, int gnd, int vpp);

void up_printbin(uint32_t d);


extern uint8_t *up_buffer;
extern bool up_verbose;
extern bool up_debug;      // print the bits on the ZIF socket
extern const char rotate[];


