/*
    handler for the UP command. needs the universal programmer 'plank' (https:// xx) 
    
    (C) Chris van Dongen 2025-26

*/

void spi_up_handler(struct command_result* res);

#define EPROM_READ    0
#define EPROM_BLANK   1
#define EPROM_VERIFY  2

// spi defines
#define UP_SPIMODE0_CS0 0   // /cs, cpol=0, cpha=0
#define UP_SPIMODE1_CS0 1   // /cs, cpol=0, cpha=1
#define UP_SPIMODE2_CS0 2   // /cs, cpol=1, cpha=0
#define UP_SPIMODE3_CS0 3   // /cs, cpol=1, cpha=1

#define UP_SPIMODE0_CS1 4   // cs, cpol=0, cpha=0
#define UP_SPIMODE1_CS1 5   // cs, cpol=0, cpha=1
#define UP_SPIMODE2_CS1 6   // cs, cpol=1, cpha=0
#define UP_SPIMODE3_CS1 7   // cs, cpol=1, cpha=1

#define UP_SPI_CSMASK   0x04    // hi=cs
#define UP_SPI_CPOLMASK 0x02
#define UP_SPI_CPHAMASK 0x01

#define UP_CS_ACTIVE         1
#define UP_CS_INACTIVE       0





