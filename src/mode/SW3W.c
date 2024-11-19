

#include <stdint.h>
#include <libopencm3/stm32/gpio.h>
#include "buspirateNG.h"
#include "UI.h"
#include "SW3W.h"
#include "cdcacm.h"

static uint32_t period;
static uint8_t csmode;
static uint8_t opendrain;
static uint8_t cpha, cpol;

void SW3W_start(void) {
    printf("set CS=%d", !csmode);

    if (csmode) {
        gpio_clear(BP_SW3W_CS_PORT, BP_SW3W_CS_PIN);
    } else {
        gpio_set(BP_SW3W_CS_PORT, BP_SW3W_CS_PIN);
    }

    modeConfig.wwr = 0;
}

void SW3W_startr(void) {
    printf("set CS=%d", !csmode);

    if (csmode) {
        gpio_clear(BP_SW3W_CS_PORT, BP_SW3W_CS_PIN);
    } else {
        gpio_set(BP_SW3W_CS_PORT, BP_SW3W_CS_PIN);
    }

    modeConfig.wwr = 1;
}

void SW3W_stop(void) {
    printf("set CS=%d", csmode);

    if (csmode) {
        gpio_set(BP_SW3W_CS_PORT, BP_SW3W_CS_PIN);
    } else {
        gpio_clear(BP_SW3W_CS_PORT, BP_SW3W_CS_PIN);
    }

    modeConfig.wwr = 0;
}

void SW3W_stopr(void) {
    printf("set CS=%d", csmode);

    if (csmode) {
        gpio_set(BP_SW3W_CS_PORT, BP_SW3W_CS_PIN);
    } else {
        gpio_clear(BP_SW3W_CS_PORT, BP_SW3W_CS_PIN);
    }

    modeConfig.wwr = 0;
}

uint32_t SW3W_send(uint32_t d) {
    int i;

    uint32_t mask, returnval;

    mask = 0x80000000 >> (32 - modeConfig.numbits);
    returnval = 0;

    // set clock to right idle level
    if (cpol) {
        gpio_set(BP_SW3W_CLK_PORT, BP_SW3W_CLK_PIN);
    } else {
        gpio_clear(BP_SW3W_CLK_PORT, BP_SW3W_CLK_PIN);
    }

    // let it settle?

    for (i = 0; i < modeConfig.numbits; i++) {
        if (cpha) // CPHA=1 change CLK before MOSI
        {
            gpio_toggle(BP_SW3W_CLK_PORT, BP_SW3W_CLK_PIN); // toggle is ok as the right polarity is set

            if (d & mask) { // write MSB first (UI.c takes care of endianess)
                gpio_set(BP_SW3W_MOSI_PORT, BP_SW3W_MOSI_PIN);
            } else {
                gpio_clear(BP_SW3W_MOSI_PORT, BP_SW3W_MOSI_PIN);
            }

            delayus(period / 2); // wait half period

            gpio_toggle(BP_SW3W_CLK_PORT, BP_SW3W_CLK_PIN); // toggle is ok as the right polarity is set

            returnval <<= 1;
            mask >>= 1;

            if (gpio_get(BP_SW3W_MISO_PORT, BP_SW3W_MISO_PIN)) { // directly read the MISO
                returnval |= 0x00000001;
            }

            delayus(period / 2); // wait half period
        } else {
            if (d & mask) { // write MSB first (UI.c takes care of endianess)
                gpio_set(BP_SW3W_MOSI_PORT, BP_SW3W_MOSI_PIN);
            } else {
                gpio_clear(BP_SW3W_MOSI_PORT, BP_SW3W_MOSI_PIN);
            }

            delayus(period / 2); // wait half period

            gpio_toggle(BP_SW3W_CLK_PORT, BP_SW3W_CLK_PIN); // toggle is ok as the right polarity is set

            returnval <<= 1;
            mask >>= 1;

            if (gpio_get(BP_SW3W_MISO_PORT, BP_SW3W_MISO_PIN)) { // directly read the MISO
                returnval |= 0x00000001;
            }

            delayus(period / 2); // wait half period

            gpio_toggle(BP_SW3W_CLK_PORT, BP_SW3W_CLK_PIN); // toggle is ok as the right polarity is set
        }
    }

    return returnval;
}

uint32_t SW3W_read(void) {
    uint32_t returnval;

    returnval = SW3W_send(0xFFFFFFFF);

    return returnval;
}

void SW3W_clkh(void) {
    printf("set CLK=1");

    gpio_set(BP_SW3W_CLK_PORT, BP_SW3W_CLK_PIN);
}

void SW3W_clkl(void) {
    printf("set CLK=0");

    gpio_clear(BP_SW3W_CLK_PORT, BP_SW3W_CLK_PIN);
}

void SW3W_dath(void) {
    printf("set MOSI=1");

    gpio_set(BP_SW3W_MOSI_PORT, BP_SW3W_MOSI_PIN);
}

void SW3W_datl(void) {
    printf("set MOSI=0");

    gpio_clear(BP_SW3W_MOSI_PORT, BP_SW3W_MOSI_PIN);
}

uint32_t SW3W_dats(void) {
    uint32_t returnval;

    returnval = (gpio_get(BP_SW3W_MISO_PORT, BP_SW3W_MISO_PIN) ? 1 : 0);

    printf("MISO=%d", returnval);

    return returnval;
}

void SW3W_clk(void) {
    printf("set CLK=%d", cpol);

    if (cpol) {
        gpio_clear(BP_SW3W_CLK_PORT, BP_SW3W_CLK_PIN);
    } else {
        gpio_set(BP_SW3W_CLK_PORT, BP_SW3W_CLK_PIN);
    }

    delayus(period / 2);

    printf("\r\nset CLK=%d", !cpol);

    if (cpol) {
        gpio_set(BP_SW3W_CLK_PORT, BP_SW3W_CLK_PIN);
    } else {
        gpio_clear(BP_SW3W_CLK_PORT, BP_SW3W_CLK_PIN);
    }

    delayus(period / 2);
}

// assumes CLK is in the right state!
uint32_t SW3W_bitr(void) {
    uint32_t returnval;

    returnval = 0;

    if (cpha) // CPHA=1 change CLK before MOSI
    {
        gpio_toggle(BP_SW3W_CLK_PORT, BP_SW3W_CLK_PIN); // toggle is ok as the right polarity is set

        gpio_set(BP_SW3W_MOSI_PORT, BP_SW3W_MOSI_PIN);

        delayus(period / 2); // wait half period

        gpio_toggle(BP_SW3W_CLK_PORT, BP_SW3W_CLK_PIN); // toggle is ok as the right polarity is set

        if (gpio_get(BP_SW3W_MISO_PORT, BP_SW3W_MISO_PIN)) { // directly read the MISO
            returnval = 1;
        }

        delayus(period / 2); // wait half period
    } else {
        gpio_set(BP_SW3W_MOSI_PORT, BP_SW3W_MOSI_PIN);

        delayus(period / 2); // wait half period

        gpio_toggle(BP_SW3W_CLK_PORT, BP_SW3W_CLK_PIN); // toggle is ok as the right polarity is set

        if (gpio_get(BP_SW3W_MISO_PORT, BP_SW3W_MISO_PIN)) { // directly read the MISO
            returnval = 1;
        }

        delayus(period / 2); // wait half period

        gpio_toggle(BP_SW3W_CLK_PORT, BP_SW3W_CLK_PIN); // toggle is ok as the right polarity is set
    }

    printf("RX: %d.1", returnval);

    return returnval;
}

uint32_t SW3W_period(void) {
    uint32_t returnval;

    returnval = 0;

    return returnval;
}

void SW3W_macro(uint32_t macro) {
    switch (macro) {
        case 0:
            printf("No macros available");
            break;
        default:
            printf("Macro not defined");
            modeConfig.error = 1;
    }
}

void SW3W_setup(void) {
    // did the user leave us arguments?
    // period
    if (cmdtail != cmdhead) {
        cmdtail = (cmdtail + 1) & (CMDBUFFSIZE - 1);
    }
    consumewhitechars();
    period = getint();
    if (period < 20) {
        modeConfig.error = 1;
    }

    // cpol
    if (cmdtail != cmdhead) {
        cmdtail = (cmdtail + 1) & (CMDBUFFSIZE - 1);
    }
    consumewhitechars();
    cpol = getint();
    if ((cpol > 0) && (cpol <= 2)) {
        cpol -= 1;
    } else {
        modeConfig.error = 1;
    }

    // cpha
    if (cmdtail != cmdhead) {
        cmdtail = (cmdtail + 1) & (CMDBUFFSIZE - 1);
    }
    consumewhitechars();
    cpha = getint();
    if ((cpha > 0) && (cpha <= 2)) {
        cpha -= 1;
    } else {
        modeConfig.error = 1;
    }

    // csmode
    if (cmdtail != cmdhead) {
        cmdtail = (cmdtail + 1) & (CMDBUFFSIZE - 1);
    }
    consumewhitechars();
    csmode = getint();
    if ((csmode > 0) && (csmode <= 2)) {
        csmode -= 1;
    } else {
        modeConfig.error = 1;
    }

    // opendrain?
    if (cmdtail != cmdhead) {
        cmdtail = (cmdtail + 1) & (CMDBUFFSIZE - 1);
    }
    consumewhitechars();
    opendrain = getint();
    if ((opendrain > 0) && (opendrain <= 2)) {
        opendrain -= 1;
    } else {
        modeConfig.error = 1;
    }

    // did the user did it right?
    if (modeConfig.error) // go interactive
    {
        period = (ui_prompt_int(SW3WPERIODMENU, 10, 1000, 1000));
        cpha = (ui_prompt_int(SW3WCPHAMENU, 1, 2, 2) - 1);
        cpol = (ui_prompt_int(SW3WCPOLMENU, 1, 2, 2) - 1);
        csmode = (ui_prompt_int(SW3WCSMENU, 1, 2, 2) - 1);
        opendrain = (ui_prompt_int(SW3WODMENU, 1, 2, 1) - 1);
    }
}

void SW3W_setup_exc(void) {
    if (opendrain) {
        gpio_set_mode(BP_SW3W_MOSI_PORT, GPIO_MODE_OUTPUT_10_MHZ, GPIO_CNF_OUTPUT_OPENDRAIN, BP_SW3W_MOSI_PIN);
        gpio_set_mode(BP_SW3W_CS_PORT, GPIO_MODE_OUTPUT_10_MHZ, GPIO_CNF_OUTPUT_OPENDRAIN, BP_SW3W_CS_PIN);
        gpio_set_mode(BP_SW3W_CLK_PORT, GPIO_MODE_OUTPUT_10_MHZ, GPIO_CNF_OUTPUT_OPENDRAIN, BP_SW3W_CLK_PIN);
        gpio_set_mode(BP_SW3W_MISO_PORT, GPIO_MODE_INPUT, GPIO_CNF_INPUT_FLOAT, BP_SW3W_MISO_PIN);
    } else {
        gpio_set_mode(BP_SW3W_MOSI_PORT, GPIO_MODE_OUTPUT_10_MHZ, GPIO_CNF_OUTPUT_PUSHPULL, BP_SW3W_MOSI_PIN);
        gpio_set_mode(BP_SW3W_CS_PORT, GPIO_MODE_OUTPUT_10_MHZ, GPIO_CNF_OUTPUT_PUSHPULL, BP_SW3W_CS_PIN);
        gpio_set_mode(BP_SW3W_CLK_PORT, GPIO_MODE_OUTPUT_10_MHZ, GPIO_CNF_OUTPUT_PUSHPULL, BP_SW3W_CLK_PIN);
        gpio_set_mode(BP_SW3W_MISO_PORT, GPIO_MODE_INPUT, GPIO_CNF_INPUT_FLOAT, BP_SW3W_MISO_PIN);
    }

    // set the right levels
    if (cpol) {
        gpio_set(BP_SW3W_CLK_PORT, BP_SW3W_CLK_PIN);
    } else {
        gpio_clear(BP_SW3W_CLK_PORT, BP_SW3W_CLK_PIN);
    }

    if (csmode) {
        gpio_clear(BP_SW3W_CS_PORT, BP_SW3W_CS_PIN);
    } else {
        gpio_set(BP_SW3W_CS_PORT, BP_SW3W_CS_PIN);
    }

    // update modeConfig pins
    modeConfig.misoport = BP_SW3W_MISO_PORT;
    modeConfig.mosiport = BP_SW3W_MOSI_PORT;
    modeConfig.csport = BP_SW3W_CS_PORT;
    modeConfig.clkport = BP_SW3W_CLK_PORT;
    modeConfig.misopin = BP_SW3W_MISO_PIN;
    modeConfig.mosipin = BP_SW3W_MOSI_PIN;
    modeConfig.cspin = BP_SW3W_CS_PIN;
    modeConfig.clkpin = BP_SW3W_CLK_PIN;

    // a guess... 72 period in the PWM is .99999uS. Multiply the period in uS * 72, divide by 4 four 4* over sample
    modeConfig.logicanalyzerperiod = ((period * 72) / 4);
}
void SW3W_cleanup(void) {
    // make all GPIO input
    gpio_set_mode(BP_SW3W_MOSI_PORT, GPIO_MODE_INPUT, GPIO_CNF_INPUT_FLOAT, BP_SW3W_MOSI_PIN);
    gpio_set_mode(BP_SW3W_MISO_PORT, GPIO_MODE_INPUT, GPIO_CNF_INPUT_FLOAT, BP_SW3W_MISO_PIN);
    gpio_set_mode(BP_SW3W_CLK_PORT, GPIO_MODE_INPUT, GPIO_CNF_INPUT_FLOAT, BP_SW3W_CLK_PIN);
    gpio_set_mode(BP_SW3W_CS_PORT, GPIO_MODE_INPUT, GPIO_CNF_INPUT_FLOAT, BP_SW3W_CS_PIN);

    // update modeConfig pins
    modeConfig.misoport = 0;
    modeConfig.mosiport = 0;
    modeConfig.csport = 0;
    modeConfig.clkport = 0;
    modeConfig.misopin = 0;
    modeConfig.mosipin = 0;
    modeConfig.cspin = 0;
    modeConfig.clkpin = 0;
}

void SW3W_pins(void) {
    printf("CS\tMISO\tCLK\tMOSI");
}

void SW3W_settings(void) {
    printf("SW3W (holdtime cpol cpha csmode od)=(%d, %d, %d, %d, %d)",
           period,
           cpol + 1,
           cpha + 1,
           csmode + 1,
           opendrain + 1);
}

void SW3W_help(void) {
    printf("Peer to peer 3 or 4 wire full duplex protocol. Very\r\n");
    printf("high clockrates upto 20MHz are possible.\r\n");
    printf("\r\n");
    printf("More info: https://en.wikipedia.org/wiki/Serial_Peripheral_Interface_Bus\r\n");
    printf("\r\n");

    printf("BPCMD\t {,] |                 DATA (1..32bit)               | },]\r\n");
    printf("CMD\tSTART| D7  | D6  | D5  | D4  | D3  | D2  | D1  | D0  | STOP\r\n");

    if (cpha) {
        printf("MISO\t-----|{###}|{###}|{###}|{###}|{###}|{###}|{###}|{###}|------\r\n");
        printf("MOSI\t-----|{###}|{###}|{###}|{###}|{###}|{###}|{###}|{###}|------\r\n");
    } else {
        printf("MISO\t---{#|##}{#|##}{#|##}{#|##}{#|##}{#|##}{#|##}{#|##}--|------\r\n");
        printf("MOSI\t---{#|##}{#|##}{#|##}{#|##}{#|##}{#|##}{#|##}{#|##}--|------\r\n");
    }

    if (cpol) {
        printf("CLK     "
               "\"\"\"\"\"|\"\"__\"|\"\"__\"|\"\"__\"|\"\"__\"|\"\"__\"|\"\"__\"|\"\"__\"|\"\"__\"|\"\"\"\"\"\"\r\n");
    } else {
        printf("CLK\t_____|__\"\"_|__\"\"_|__\"\"_|__\"\"_|__\"\"_|__\"\"_|__\"\"_|__\"\"_|______\r\n");
    }

    if (csmode) {
        printf("CS\t\"\"___|_____|_____|_____|_____|_____|_____|_____|_____|___\"\"\"\r\n");
    } else {
        printf("CS\t__\"\"\"|\"\"\"\"\"|\"\"\"\"\"|\"\"\"\"\"|\"\"\"\"\"|\"\"\"\"\"|\"\"\"\"\"|\"\"\"\"\"|\"\"\"\"\"|"
               "\"\"\"___\r\n");
    }

    printf("\r\nCurrent mode is CPHA=%d and CPOL=%d\r\n", cpha, cpol);
    printf("\r\n");
    printf("Connection:\r\n");
    printf("\tMOSI \t------------------ MOSI\r\n");
    printf("\tMISO \t------------------ MISO\r\n");
    printf("{BP}\tCLK\t------------------ CLK\t{DUT}\r\n");
    printf("\tCS\t------------------ CS\r\n");
    printf("\tGND\t------------------ GND\r\n");
}
