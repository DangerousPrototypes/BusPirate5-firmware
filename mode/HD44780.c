

#include <stdint.h>
#include <libopencm3/stm32/spi.h>
#include "buspirateNG.h"
#include "spi.h"
#include "cdcacm.h"
#include "UI.h"
#include "LCDSPI.h"
#include "HD44780.h"

static void HD44780_send(uint8_t rs, uint8_t d);

// hd44780 driver

#define HCT595_LCD_LED 0b00000001
#define HCT595_LCD_RS 0b00000010
#define HCT595_LCD_RW 0b00000100
#define HCT595_LCD_EN 0b00001000
#define HCT595_LCD_D4 0b00010000
#define HCT595_LCD_D5 0b00100000
#define HCT595_LCD_D6 0b01000000
#define HCT595_LCD_D7 0b10000000

#define CMD_CLEARDISPLAY 0b00000001 // 82us-1.64ms

#define CMD_RETURNHOME 0b00000010 // 40us-1.64ms

#define CMD_ENTRYMODESET 0b00000100 // 40us
#define INCREMENT 0b10
#define DECREMENT 0b00
#define DISPLAYSHIFTON 0b1
#define DISPLAYSHIFTOFF 0

#define CMD_DISPLAYCONTROL 0b00001000 // 40us
#define DISPLAYON 0b100
#define DISPLAYOFF 0
#define CURSERON 0b10
#define CURSEROFF 0
// #define BLINKON 		0b1
// #define BLINKOFF 		0

#define CMD_CURSERDISPLAYSHIFT 0b00010000 // 40us
#define DISPLAYSHIFT 0b1000
#define CURSERMOVE 0
#define SHIFTRIGHT 0b100
#define SHIFTLEFT 0

#define CMD_FUNCTIONSET 0b00100000 // 40us
#define DATAWIDTH8 0b10000
#define DATAWIDTH4 0
#define DISPLAYLINES2 0b1000
#define DISPLAYLINES1 0
#define FONT5X10 0b100
#define FONT5X7 0

#define MODULE24X4 0b1
#define CMD_SETCGRAMADDR 0b01000000 // 40us
// 6bit character generator RAM address
#define CMD_SETDDRAMADDR 0b10000000 // 40us
// 7bit display data RAM address

uint32_t HD44780_write(uint32_t d) {
    HD44780_send(1, d & 0xff);
    return 0;
}

uint32_t HD44780_read(void) {
    return 0;
}

void HD44780_macro(uint32_t macro) {
    uint32_t arg1;
    uint32_t i;

    cmdtail = (cmdtail + 1) & (CMDBUFFSIZE - 1);
    consumewhitechars();
    arg1 = getint();

    printf("arg=%d", arg1);

    switch (macro) {
        case 0:
            printf(" 1. reset and init 1 line\r\n");
            printf(" 2. reset and init 2 lines\r\n");
            printf(" 3. clear display\r\n");
            printf(" 4. set cursor\r\n");
            printf(" 5. write command n\r\n");
            printf(" 6. write n digits to display\r\n");
            printf(" 7. write n chars to display\r\n");
            break;
        case 1:
        case 2:
            HD44780_reset();
            HD44780_init(macro);
            break;
        case 3:
            HD44780_send(0, CMD_CLEARDISPLAY);
            break;
        case 4:
            arg1 &= 0x7f;
            HD44780_send(0, CMD_SETDDRAMADDR | arg1);
            break;
        case 5:
            HD44780_send(1, arg1);
            break;
        case 6:
            for (i = 0; i < arg1; i++) {
                HD44780_send(1, 0x30 + (i % 10));
            }
            break;
        case 7:
            for (i = 0; i < arg1; i++) {
                HD44780_send(1, 0x20 + (i % 0x5e));
            }
            break;
        default:
            printf("Macro not defined");
            modeConfig.error = 1;
    }
}

void HD44780_setup(void) {
    // setup SPI
    spi_setcpol(1 << 1);
    spi_setcpha(1);
    spi_setbr(5 << 3);
    spi_setdff(SPI_CR1_DFF_8BIT);
    spi_setlsbfirst(SPI_CR1_MSBFIRST);
    spi_setcsidle(1);
    spi_setup_exc();

    printf("This mode uses an external adapter");
}

void HD44780_cleanup(void) {
    spi_cleanup();
}

void HD44780_reset(void) {
    spi_xfer(0x00);
    spi_setcs(1);
    spi_setcs(0);
    busy_wait_ms(15);
    HD44780_writenibble(0, 0x03);
    busy_wait_ms(15);
    HD44780_writenibble(0, 0x03);
    busy_wait_ms(15);
    HD44780_writenibble(0, 0x03);
    busy_wait_ms(15);
    HD44780_writenibble(0, 0x02);
    busy_wait_ms(15);
}

void HD44780_init(uint8_t lines) {
    if (lines == 1) {
        HD44780_send(0, 0x20);
    } else {
        HD44780_send(0, 0x28);
    }
    busy_wait_ms(15);
    HD44780_send(0, 0x08);
    busy_wait_ms(15);
    HD44780_send(0, 0x01);
    busy_wait_ms(15);
    HD44780_send(0, 0x0f);
    busy_wait_ms(15);
}

static void HD44780_send(uint8_t rs, uint8_t d) {
    HD44780_writenibble(rs, (d >> 4));
    HD44780_writenibble(rs, (d & 0x0F));
}

void HD44780_writenibble(uint8_t rs, uint8_t d) {
    d &= 0x0F;
    d <<= 4;

    d |= HCT595_LCD_LED;

    if (rs) {
        d |= HCT595_LCD_RS;
    }

    spi_xfer(d);
    spi_setcs(1);
    spi_setcs(0);
    spi_xfer(d | HCT595_LCD_EN);
    spi_setcs(1);
    spi_setcs(0);
    spi_xfer(d);
    spi_setcs(1);
    spi_setcs(0);
}
