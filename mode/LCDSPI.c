

#include <stdint.h>
#include <libopencm3/stm32/spi.h>
#include "buspirateNG.h"
#include "spi.h"
#include "cdcacm.h"
#include "UI.h"
#include "LCDSPI.h"
#include "modes.h"

#include "HD44780.h"
#include "ST7735.h"

static uint32_t currentdisplay;

// subset of the protocol
typedef struct _display {
    uint32_t (*display_send)(uint32_t); // send max 32 bit
    uint32_t (*display_read)(void);     // read max 32 bit (is thsi actually used?)
    void (*display_macro)(uint32_t);    // macro
    void (*display_setup)(void);        // setup UI
    void (*display_cleanup)(void);      // cleanup for HiZ
    char name[10];
} display;

static struct _display displays[2] = {
    // HD44780 is mandatory
    { HD44780_write,
      nullfunc3, // read is not used
      HD44780_macro,
      HD44780_setup,
      HD44780_cleanup,
      "HD44780" },
#ifdef DISPLAY_USE_ST7735
    { ST7735_send,
      nullfunc3, // read is not used
      ST7735_macro,
      ST7735_setup,
      ST7735_cleanup,
      "ST7735D/R" },
#endif
};

uint32_t LCDSPI_send(uint32_t d) {
    return displays[currentdisplay].display_send(d);
}

uint32_t LCDSPI_read(void) {
    return displays[currentdisplay].display_read();
}

void LCDSPI_macro(uint32_t macro) {
    displays[currentdisplay].display_macro(macro);
}

void LCDSPI_setup(void) {
    int i;

    currentdisplay = 0;
    if (cmdtail != cmdhead) {
        cmdtail = (cmdtail + 1) & (CMDBUFFSIZE - 1);
    }
    consumewhitechars();
    currentdisplay = getint();

    if ((currentdisplay == 0) || (currentdisplay > MAXDISPLAYS)) {
        for (i = 0; i < MAXDISPLAYS; i++) {
            printf(" %2d. %s\r\n", i + 1, displays[i].name);
        }

        currentdisplay = ui_prompt_int("\r\ndisplay> ", 1, 2, 1);
        printf("\r\n");
    }
    currentdisplay--;
}

void LCDSPI_setup_exc(void) {
    displays[currentdisplay].display_setup();

    modeConfig.subprotocolname = displays[currentdisplay].name;
}

void LCDSPI_cleanup(void) {
    displays[currentdisplay].display_cleanup();

    modeConfig.subprotocolname = 0;
}

void LCDSPI_pins(void) {
    printf("CS\tMISO\tCLK\tMOSI");
}

void LCDSPI_settings(void) {
    printf("LCDSPI (display)=(%d) \"%s\"", currentdisplay, displays[currentdisplay].name);
}
