#include "pirate.h"
#include "pirate/bio.h"

#define  MAX_DEVICES_LEN    32                             // Maximum number of devices allowed in a single JTAG chain
#define  MIN_IR_LEN          2                             // Minimum length of instruction register per IEEE Std. 1149.1
#define  MAX_IR_LEN         32                             // Maximum length of instruction register
#define  MAX_IR_CHAIN_LEN   MAX_DEVICES_LEN * MAX_IR_LEN   // Maximum total length of JTAG chain w/ IR selected
#define  MAX_DR_LEN         4096                           // Maximum length of data register
#define ARRAY_SIZE(array) (sizeof(array) / sizeof(*array))
#define CR		    13
#define LF		    10

//#define ONBOARD_LED 25 // If not defined, onboard LED will not be used
#define BTAG_UNUSED_GPIO 28 // Unused in source
#define BTAG_MAX_NUM_JTAG 32 // Unused in source
#define BTAG_START_CHANNEL 8 // First GPIO pin to use 0 - 16 by default
#define BTAG_MAX_CHANNELS 8 // Max number of channels supported by Pico  (upto 16 on a bare board)
#define BTAG_EOL "\r\n"

struct jtagScan_t
{
    bool volatile foundPinout;
    bool jPulsePins;
    uint jTDI;           
    uint jTDO;
    uint jTCK;
    uint jTMS;
    uint jTRST;
    uint xTDI;           
    uint xTDO;
    uint xTCK;
    uint xTMS;
    uint xTRST;
    uint channelCount;
    uint progressCount;
    uint maxPermutations;
    uint jDeviceCount;
    uint32_t deviceIDs[MAX_DEVICES_LEN]; // Array to store identified device IDs
};


struct swdScan_t
{
    uint xSwdClk;
    uint xSwdIO;
    uint channelCount;
    uint progressCount;
    uint maxPermutations;
    bool swdDeviceFound;
};

void bluetag_jPulsePins_set(bool jPulsePins);
void bluetag_progressbar_cleanup(uint maxPermutations);
bool jtagScan(struct jtagScan_t *jtag);
bool swdScan(struct swdScan_t *swd);
extern char *version;

//bio helper functions
static inline uint gpio2bio(uint gpio)
{
    assert(gpio>=8 && gpio<=16);
    return gpio - 8;
}

// JTAG IO functions

// Function that sets all used channels to output high
static inline void setPinsHigh(uint startChannel, uint channelCount)
{
    for(int x = startChannel; x < (channelCount+startChannel); x++)
    {
        gpio_put(x, 1);
    }
}

static inline void setPinsLoW(uint startChannel, uint channelCount)
{
    for(int x = startChannel; x < (channelCount+startChannel); x++)
    {
        gpio_put(x, 0);
    }
}


// Initialize all available channels & set them as output
static inline void initChannels(uint startChannel, uint maxChannels)
{
    for(int x=startChannel; x < (maxChannels+startChannel) ; x++)
    {
        gpio_init(x);
        //gpio_set_dir(x, GPIO_OUT);
        bio_output(gpio2bio(x));
    }   
}

static inline void jtagConfig(uint tdiPin, uint tdoPin, uint tckPin, uint tmsPin)
{
    // Output
    //gpio_set_dir(tdiPin, GPIO_OUT);
    bio_output(gpio2bio(tdiPin));
    //gpio_set_dir(tckPin, GPIO_OUT);
    bio_output(gpio2bio(tckPin));
    //gpio_set_dir(tmsPin, GPIO_OUT);
    bio_output(gpio2bio(tmsPin));

    // Input
    //gpio_set_dir(tdoPin, GPIO_IN);
    bio_input(gpio2bio(tdoPin));
    gpio_put(tckPin, 0);
}

// Generate one TCK pulse. Read TDO inside the pulse.
// Expects TCK to be low upon being called.
static inline bool tdoRead(uint jTCK, uint jTDO)
{
    bool volatile tdoStatus;
    gpio_put(jTCK, 1);
    tdoStatus=gpio_get(jTDO);
    gpio_put(jTCK, 0);
    return(tdoStatus);
}

static inline void tdiHigh(uint jTDI)
{
    gpio_put(jTDI, 1);
}

static inline void tdiLow(uint jTDI)
{
    gpio_put(jTDI, 0);
}

static inline void tmsHigh(uint jTMS)
{
    gpio_put(jTMS, 1);
}

static inline void tmsLow(uint jTMS)
{
    gpio_put(jTMS, 0);
}

static inline void trstHigh(uint jTRST)
{
    gpio_put(jTRST, 1);
}

static inline void trstLow(uint jTRST)
{
    gpio_put(jTRST, 0);
}

// SWD IO functions
#define BUFDIR_INPUT 0
#define BUFDIR_OUTPUT 1
static inline void initSwdPins(uint xSwdClk, uint xSwdIO)
{
    //gpio_set_dir(xSwdClk,GPIO_OUT);
    bio_output(gpio2bio(xSwdClk));
    //gpio_set_dir(xSwdIO,GPIO_OUT);
    bio_output(gpio2bio(xSwdIO));
#if 0
    // first set the buffer to output
    gpio_put(gpio2bio(xSwdClk), BUFDIR_OUTPUT);
    // now set pin to output
    gpio_set_dir(gpio2bio(xSwdClk), GPIO_OUT);
    // first set the buffer to output
    gpio_put(gpio2bio(xSwdIO), BUFDIR_OUTPUT);
    // now set pin to output
    gpio_set_dir(gpio2bio(xSwdIO), GPIO_OUT);    
#endif

}

static inline void swdClockPulse(uint xSwdClk, uint swd_delay)
{
    gpio_put(xSwdClk, 0);
    sleep_us(swd_delay);
    gpio_put(xSwdClk, 1);
    sleep_us(swd_delay);
}

static inline void swdSetReadMode(uint xSwdIO)
{
    //gpio_set_dir(xSwdIO,GPIO_IN);
    bio_input(gpio2bio(xSwdIO));
    // first set the pin to input
    //gpio_set_dir(gpio2bio(xSwdIO), GPIO_IN);
    // now set buffer to input
    //gpio_put(gpio2bio(xSwdIO), BUFDIR_INPUT); 
}

static inline void swdSetWriteMode(uint xSwdIO)
{
    //gpio_set_dir(xSwdIO,GPIO_OUT);
    bio_output(gpio2bio(xSwdIO));
    // first set the buffer to output
    //gpio_put(gpio2bio(xSwdIO), BUFDIR_OUTPUT);
    // now set pin to output
    //gpio_set_dir(gpio2bio(xSwdIO), GPIO_OUT);      
}

static inline void swdIOHigh(uint xSwdIO)
{
    gpio_put(xSwdIO, 1);
}

static inline void swdIOLow(uint xSwdIO)
{
    gpio_put(xSwdIO, 0);
}

static inline void swdWriteHigh(uint xSwdClk,uint xSwdIO, uint swd_delay)
{
    gpio_put(xSwdIO, 1);
    swdClockPulse(xSwdClk, swd_delay);
}

static inline void swdWriteLow(uint xSwdClk,uint xSwdIO, uint swd_delay)
{
    gpio_put(xSwdIO, 0);
    swdClockPulse(xSwdClk, swd_delay);
}

static inline void swdWriteBit(uint xSwdIO, uint xSwdClk, bool value, uint swd_delay)
{
    gpio_put(xSwdIO, value);
    //swdClockPulse();
    swdClockPulse(xSwdClk, swd_delay);
}

static inline bool swdReadBit(uint xSwdClk, uint xSwdIO, uint swd_delay)
{
    bool value=gpio_get(xSwdIO);
    swdClockPulse(xSwdClk, swd_delay);
    return(value);
}

