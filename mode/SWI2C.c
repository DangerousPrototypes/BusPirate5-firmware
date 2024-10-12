#include <stdint.h>
#include <libopencm3/stm32/gpio.h>
#include "buspirateNG.h"
#include "UI.h"
#include "SWI2C.h"
#include "cdcacm.h"
#include "bitbang.h"

static uint32_t period;
static uint8_t speed;
static uint8_t ackPending;

void SWI2C_start(void) {
    if (ackPending) {
        printf("NACK");
        bbI2Cnack();
        ackPending = 0;
    }

    if (bbI2Cstart()) { // bus contention
        printf("Short or no pullups!");
    }

    printf("START");

    // todo:reset read/write mode
}

void SWI2C_stop(void) {
    if (ackPending) {
        printf("NACK");
        bbI2Cnack();
        ackPending = 0;
    }

    bbI2Cstop();
    printf("STOP");
}

uint32_t SWI2C_write(uint32_t d) {

    // TODO:if read/write mode tracker reset, use last bit to set read/write mode
    // TODO:if read mode is set warn write mode may be invalid

    if (ackPending) {
        printf("NACK");
        bbI2Cnack();
        ackPending = 0;
    }

    bbWrite(d);

    if (bbReadBit() == 0) { // bpWmessage(MSG_ACK);
        printf("ACK");
        return 0x300; // bit 9=ack
    } else {          // bpWmessage(MSG_NACK);
        printf("No ACK received!");
        return 0x100; // bit 9=ack
    }
}

uint32_t SWI2C_read(void) {
    uint32_t c;

    if (ackPending) {
        printf("ACK");
        bbI2Cack();
        ackPending = 0;
    }

    c = bbRead();
    ackPending = 1;
    return c;
}

void SWI2C_macro(uint32_t macro) {
    switch (macro) {
        case 0:
            printf(" 1. I2C Address search\r\n");
            //				printf(" 2. I2C sniffer\r\n";
            break;
        case 1:
            I2C_search();
            break;
        default:
            printf("Macro not defined");
            modeConfig.error = 1;
    }
}

void SWI2C_setup(void) {
    // speed
    if (cmdtail != cmdhead) {
        cmdtail = (cmdtail + 1) & (CMDBUFFSIZE - 1);
    }
    consumewhitechars();
    speed = getint();
    if ((speed > 0) && (speed <= 2)) {
        speed -= 1;
    } else {
        modeConfig.error = 1;
    }

    // did the user did it right?
    if (modeConfig.error) // go interactive
    {
        speed = (ui_prompt_int(SWI2CSPEEDMENU, 1, 2, 1));
    }

    ackPending = 0;
}

void SWI2C_setup_exc(void) {
    // update modeConfig pins
    modeConfig.mosiport = BP_SW2W_SDA_PORT;
    modeConfig.clkport = BP_SW2W_CLK_PORT;
    modeConfig.hiz = 1;
    modeConfig.numbits = 8;

    bbSetup(2, 0); // configure the bitbang library for 2-wire, set the speed

    // a guess... 72 period in the PWM is .99999uS. Multiply the period in uS * 72, divide by 4 four 4* over sample
    // TODO: used fixed calculated period for I2C
    modeConfig.logicanalyzerperiod = ((period * 72) / 4);
}

void SWI2C_cleanup(void) {
    printf("SWI2C cleanup()");

    // make all GPIO input
    // SW2W_SETUP_HIZ();

    // update modeConfig pins
    modeConfig.misoport = 0;
    modeConfig.mosiport = 0;
    modeConfig.csport = 0;
    modeConfig.clkport = 0;
    modeConfig.misopin = 0;
    modeConfig.mosipin = 0;
    modeConfig.cspin = 0;
    modeConfig.clkpin = 0;

    // SW2W_cleanup();
}
void SWI2C_pins(void) {
    printf("-\t-\tCLK\tDAT");
}
void SWI2C_settings(void) {
    printf("SWI2C (period hiz)=(%d %d)", period, modeConfig.hiz);
}
void SWI2C_help(void) {
    printf("Muli-Master-multi-slave 2 wire protocol using a CLOCK and an bidirectional DATA\r\n");
    printf("line in opendrain configuration. Standard clock frequencies are 100kHz, 400kHz\r\n");
    printf("and 1MHz.\r\n");
    printf("\r\n");
    printf("More info: https://en.wikipedia.org/wiki/I2C\r\n");
    printf("\r\n");
    printf("Electrical:\r\n");
    printf("\r\n");
    printf("BPCMD\t   { |            ADDRES(7bits+R/!W bit)             |\r\n");
    printf("CMD\tSTART| A6  | A5  | A4  | A3  | A2  | A1  | A0  | R/!W| ACK* \r\n");
    printf("\t-----|-----|-----|-----|-----|-----|-----|-----|-----|-----\r\n");
    printf("SDA\t\"\"___|_###_|_###_|_###_|_###_|_###_|_###_|_###_|_###_|_###_ ..\r\n");
    printf("SCL\t\"\"\"\"\"|__\"__|__\"__|__\"__|__\"__|__\"__|__\"__|__\"__|__\"__|__\"__ ..\r\n");
    printf("\r\n");
    printf("BPCMD\t   |                      DATA (8bit)              |     |  ]  |\r\n");
    printf("CMD\t.. | D7  | D6  | D5  | D4  | D3  | D2  | D1  | D0  | ACK*| STOP|  \r\n");
    printf("\t  -|-----|-----|-----|-----|-----|-----|-----|-----|-----|-----|\r\n");
    printf("SDA\t.. |_###_|_###_|_###_|_###_|_###_|_###_|_###_|_###_|_###_|___\"\"|\r\n");
    printf("SCL\t.. |__\"__|__\"__|__\"__|__\"__|__\"__|__\"__|__\"__|__\"__|__\"__|\"\"\"\"\"|\r\n");
    printf("\r\n");
    printf("* Receiver needs to pull SDA down when address/byte is received correctly\r\n");
    printf("\r\n");
    printf("Connection:\r\n");
    printf("\t\t  +--[4k7]---+--- +3V3 or +5V0\r\n");
    printf("\t\t  | +-[4k7]--|\r\n");
    printf("\t\t  | |\r\n");
    printf("\tSDA \t--+-|------------- SDA\r\n");
    printf("{BP}\tSCL\t----+------------- SCL  {DUT}\r\n");
    printf("\tGND\t------------------ GND\r\n");
}

void I2C_search(void) {

    uint16_t i;

    // bbH(MOSI + CLK, 0); //clock and data high

    printf("I2C address search:");

    /*if (BP_CLK == 0 || BP_MOSI == 0) {
        BPMSG1019; //warning
        BPMSG1020; //short or no pullups
        bpBR;
        return;
    }*/
    for (i = 0; i < 0x100; i++) {
        bbI2Cstart(); // send start
        bbWrite(i);   // send address
        // look for ack
        if (bbReadBit() == 0) { // 0 is ACK
            if ((i & 0b1) == 0) {
                printf("%d (%d W)", i, (i >> 1));
            } else { // if the first bit is set it's a read address, send a byte plus nack to clean up
                printf("%d (%d R)", i, (i >> 1));
                bbRead();
                bbI2Cnack(); // NACK
            }
        }
        bbI2Cstop();
    }
}