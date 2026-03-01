/*
 * arduboy_periph.c — ATmega32U4 peripheral emulation for Arduboy
 *
 * Implements I/O hooks that intercept AVR register reads/writes and
 * route them to emulated SSD1306, buttons, timers, and EEPROM.
 *
 * Arduboy hardware mapping on ATmega32U4:
 *   - SSD1306 CS:   PD6 (PORTD bit 6)
 *   - SSD1306 D/C:  PD4 (PORTD bit 4)
 *   - SSD1306 RST:  PD7 (PORTD bit 7)
 *   - SPI MOSI:     PB2
 *   - SPI SCK:      PB1
 *   - Button A:     PE6
 *   - Button B:     PB4
 *   - Button UP:    PF7
 *   - Button DOWN:  PF4
 *   - Button LEFT:  PF5
 *   - Button RIGHT: PF6
 *   - Speaker 1:    PC6 (Timer3A)
 *   - Speaker 2:    PC7 (Timer4A)
 *   - RGB LED:      PB5 (red), PB6 (green), PB7 (blue) — active low
 *
 * Copyright (c) 2026 Bus Pirate project. MIT License.
 */

#include "arduboy_periph.h"
#include <string.h>

/* ── SSD1306 command processing ──────────────────────────────── */

static void ssd1306_reset(ssd1306_t* oled) {
    memset(oled, 0, sizeof(*oled));
    oled->col_end = 127;
    oled->page_end = 7;
    oled->display_on = false;
    oled->state = SSD1306_IDLE;
}

static void ssd1306_process_command(ssd1306_t* oled, uint8_t cmd) {
    /* Handle multi-byte command continuations */
    if (oled->pending_args > 0) {
        oled->pending_args--;
        switch (oled->pending_cmd) {
        case 0x21: /* Set column address */
            if (oled->arg_count == 0) {
                oled->col_start = cmd & 0x7F;
                oled->col = oled->col_start;
            } else {
                oled->col_end = cmd & 0x7F;
            }
            oled->arg_count++;
            break;
        case 0x22: /* Set page address */
            if (oled->arg_count == 0) {
                oled->page_start = cmd & 0x07;
                oled->page = oled->page_start;
            } else {
                oled->page_end = cmd & 0x07;
            }
            oled->arg_count++;
            break;
        default:
            /* Ignore args for unhandled commands */
            break;
        }
        return;
    }

    /* Single-byte commands and first byte of multi-byte commands */
    if (cmd >= 0x00 && cmd <= 0x0F) {
        /* Set lower nibble of column start address (page addressing) */
        oled->col = (oled->col & 0xF0) | (cmd & 0x0F);
    } else if (cmd >= 0x10 && cmd <= 0x1F) {
        /* Set upper nibble of column start address */
        oled->col = (oled->col & 0x0F) | ((cmd & 0x0F) << 4);
    } else if (cmd >= 0xB0 && cmd <= 0xB7) {
        /* Set page start address (page addressing mode) */
        oled->page = cmd & 0x07;
    } else {
        switch (cmd) {
        case 0x20: /* Set memory addressing mode — takes 1 arg */
            oled->pending_cmd = cmd;
            oled->pending_args = 1;
            oled->arg_count = 0;
            break;
        case 0x21: /* Set column address — takes 2 args */
            oled->pending_cmd = cmd;
            oled->pending_args = 2;
            oled->arg_count = 0;
            break;
        case 0x22: /* Set page address — takes 2 args */
            oled->pending_cmd = cmd;
            oled->pending_args = 2;
            oled->arg_count = 0;
            break;
        case 0xAE: /* Display OFF */
            oled->display_on = false;
            break;
        case 0xAF: /* Display ON */
            oled->display_on = true;
            break;
        /* Commands that take 1 arg — consume and ignore */
        case 0x81: /* Set contrast */
        case 0x8D: /* Charge pump setting */
        case 0xA8: /* Set multiplex ratio */
        case 0xD3: /* Set display offset */
        case 0xD5: /* Set display clock divide */
        case 0xD9: /* Set pre-charge period */
        case 0xDA: /* Set COM pins hardware config */
        case 0xDB: /* Set VCOMH deselect level */
            oled->pending_cmd = cmd;
            oled->pending_args = 1;
            oled->arg_count = 0;
            break;
        /* Single-byte commands — no args */
        case 0xA0: case 0xA1: /* Segment remap */
        case 0xA4: case 0xA5: /* Entire display ON (resume/force) */
        case 0xA6: case 0xA7: /* Normal/inverse display */
        case 0xC0: case 0xC8: /* COM output scan direction */
        case 0x2E: case 0x2F: /* Deactivate/activate scroll */
        case 0xE3: /* NOP */
            break;
        default:
            /* Unknown command — ignore */
            break;
        }
    }
}

static void ssd1306_write_data(ssd1306_t* oled, uint8_t data) {
    /* Write one byte to the framebuffer at current position */
    if (oled->page <= 7 && oled->col < 128) {
        uint16_t idx = (uint16_t)oled->page * 128 + oled->col;
        if (idx < ARDUBOY_FB_SIZE) {
            oled->framebuffer[idx] = data;
            oled->framebuffer_dirty = true;
        }
    }

    /* Advance position (horizontal addressing mode — Arduboy default) */
    oled->col++;
    oled->data_count++;
    if (oled->col > oled->col_end) {
        oled->col = oled->col_start;
        oled->page++;
        if (oled->page > oled->page_end) {
            oled->page = oled->page_start;
            /* Full screen transfer completed — signal render */
            oled->transfer_complete = true;
            oled->dbg_transfer_count++;
            oled->data_count = 0;
        }
    }
}

/* Process a byte received via SPI */
static void ssd1306_spi_byte(ssd1306_t* oled, uint8_t byte) {
    if (oled->cs_pin) return; /* CS high — not selected */

    if (oled->dc_pin) {
        /* D/C high — data */
        oled->dbg_data_count++;
        if (byte) oled->dbg_data_nonzero++;
        ssd1306_write_data(oled, byte);
    } else {
        /* D/C low — command */
        oled->dbg_cmd_count++;
        ssd1306_process_command(oled, byte);
    }
}

/* ── USB CDC enumeration state machine ───────────────────────── */

/* USB register bit definitions */
#define USBE_BIT        (1 << 7) /* USB macro enable in USBCON */
#define OTGPADE_BIT     (1 << 4) /* OTG pad enable in USBCON */
#define VBUSTI_BIT      (1 << 0) /* VBUS transition in USBINT */
#define EORSTI_BIT      (1 << 3) /* End-of-reset in UDINT */
#define EORSTE_BIT      (1 << 3) /* End-of-reset enable in UDIEN */
#define RXSTPI_BIT      (1 << 3) /* Setup packet received in UEINTX */
#define TXINI_BIT       (1 << 0) /* TX IN ready in UEINTX */
#define RXOUTI_BIT      (1 << 2) /* RX OUT ready in UEINTX */

/* Delay between enumeration phases (in CPU cycles at 16MHz).
 * 100,000 cycles ≈ 6.25ms — long enough for firmware init loops. */
#define USB_ENUM_DELAY  100000

/*
 * Inject a setup packet into EP0 FIFO and flag the endpoint interrupt.
 */
static void usb_enum_inject_packet(arduboy_periph_t* periph,
                                    const uint8_t* pkt, uint8_t len) {
    usb_enum_t* usb = &periph->usb_enum;
    memcpy(usb->ep0_fifo, pkt, len);
    usb->ep0_fifo_pos = 0;
    usb->ep0_fifo_len = len;

    /* Signal that EP0 has a setup packet ready.
     * We use a shadow flag instead of writing SRAM directly because
     * the endpoint registers (UEINTX, UEBCLX, etc.) are banked per
     * endpoint on real hardware — the firmware overwrites the SRAM
     * locations when configuring other endpoints (1-6). */
    usb->ep0_rxstpi_pending = true;
    usb->end_irq_pending = true;
}

/*
 * Advance the USB enumeration state machine by `cycles` CPU cycles.
 * Called from arduboy_periph_tick().
 */
static void usb_enum_tick(arduboy_periph_t* periph, uint32_t cycles) {
    usb_enum_t* usb = &periph->usb_enum;

    if (usb->state == USB_ENUM_IDLE || usb->state == USB_ENUM_DONE) return;

    /* Countdown delay */
    if (usb->delay_counter > cycles) {
        usb->delay_counter -= cycles;
        return;
    }
    usb->delay_counter = 0;

    switch (usb->state) {
    case USB_ENUM_VBUS:
        /*
         * Phase 1: VBUS detected. Set VBUSTI in USBINT and fire
         * USB_GEN interrupt so the firmware's USB_GEN ISR runs.
         */
        periph->cpu->sram[AVR_USBINT_ADDR] |= VBUSTI_BIT;
        usb->gen_irq_pending = true;
        usb->state = USB_ENUM_RESET;
        usb->delay_counter = USB_ENUM_DELAY;
        break;

    case USB_ENUM_RESET:
        /*
         * Phase 2: End-of-reset. The firmware has attached (UDCON DETACH
         * cleared) and is waiting for bus reset. Set EORSTI in UDINT
         * and fire USB_GEN again. After this, configure EP0 and inject
         * SET_CONFIGURATION.
         */
        periph->cpu->sram[AVR_UDINT_ADDR] |= EORSTI_BIT;
        usb->gen_irq_pending = true;

        /* Pre-configure EP0 as the firmware would expect after reset */
        periph->cpu->sram[AVR_UECONX_ADDR] = 0x01;   /* EPEN = 1 */
        periph->cpu->sram[AVR_UECFG1X_ADDR] = 0x32;  /* 32-byte EP, allocated */
        periph->cpu->sram[AVR_UESTA0X_ADDR] = 0x80;   /* CFGOK */
        periph->cpu->sram[AVR_UEIENX_ADDR] = 0x08;    /* RXSTPE enabled */

        usb->state = USB_ENUM_SET_CONFIG;
        usb->delay_counter = USB_ENUM_DELAY;
        break;

    case USB_ENUM_SET_CONFIG: {
        /*
         * Phase 3: Inject SET_CONFIGURATION (bRequest=0x09, wValue=1).
         * This is the standard USB request that selects configuration #1.
         * The firmware's EP0 handler processes it and sets up CDC endpoints.
         *
         * USB setup packet format (8 bytes):
         *   bmRequestType=0x00 (host→device, standard, device)
         *   bRequest=0x09 (SET_CONFIGURATION)
         *   wValue=0x0001 (config 1)
         *   wIndex=0x0000
         *   wLength=0x0000
         */
        static const uint8_t set_config[8] = {
            0x00, 0x09, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00
        };
        usb_enum_inject_packet(periph, set_config, 8);

        usb->state = USB_ENUM_SET_LINE_STATE;
        usb->delay_counter = USB_ENUM_DELAY;
        break;
    }

    case USB_ENUM_SET_LINE_STATE: {
        /*
         * Phase 4: Inject SET_CONTROL_LINE_STATE (CDC class request).
         * This tells the firmware that DTR+RTS are asserted (wValue=0x0003),
         * which sets lineState — breaking `while(!Serial)`.
         *
         * USB setup packet format (8 bytes):
         *   bmRequestType=0x21 (host→device, class, interface)
         *   bRequest=0x22 (SET_CONTROL_LINE_STATE)
         *   wValue=0x0003 (DTR=1, RTS=1)
         *   wIndex=0x0000 (interface 0)
         *   wLength=0x0000
         */
        static const uint8_t set_line_state[8] = {
            0x21, 0x22, 0x03, 0x00, 0x00, 0x00, 0x00, 0x00
        };
        usb_enum_inject_packet(periph, set_line_state, 8);

        usb->state = USB_ENUM_DONE;
        break;
    }

    default:
        break;
    }
}

/* ── Button mapping ──────────────────────────────────────────── */

/*
 * Arduboy button-to-pin mapping:
 *   UP    → PF7 (PINF bit 7)
 *   DOWN  → PF4 (PINF bit 4)
 *   LEFT  → PF5 (PINF bit 5)
 *   RIGHT → PF6 (PINF bit 6)
 *   A     → PE6 (PINE bit 6)
 *   B     → PB4 (PINB bit 4)
 *
 * Buttons are active-low with internal pull-ups — pressed = 0.
 */
static uint8_t get_pinf(arduboy_periph_t* periph) {
    uint8_t val = 0xFF; /* All pull-ups, all high by default */
    if (periph->button_state & ARDUBOY_BTN_UP)    val &= ~(1 << 7);
    if (periph->button_state & ARDUBOY_BTN_DOWN)  val &= ~(1 << 4);
    if (periph->button_state & ARDUBOY_BTN_LEFT)  val &= ~(1 << 5);
    if (periph->button_state & ARDUBOY_BTN_RIGHT) val &= ~(1 << 6);
    return val;
}

static uint8_t get_pine(arduboy_periph_t* periph) {
    uint8_t val = 0xFF;
    if (periph->button_state & ARDUBOY_BTN_A) val &= ~(1 << 6);
    return val;
}

static uint8_t get_pinb(arduboy_periph_t* periph) {
    uint8_t val = 0xFF;
    if (periph->button_state & ARDUBOY_BTN_B) val &= ~(1 << 4);
    return val;
}

/* ── I/O hooks ────────────────────────────────────────────────── */

static uint8_t io_read_hook(uint16_t addr, void* ctx) {
    arduboy_periph_t* periph = (arduboy_periph_t*)ctx;
    periph->dbg_io_reads++;

    switch (addr) {
    /* ── GPIO PIN registers (read actual pin state) ── */
    case AVR_PINB_ADDR:  return get_pinb(periph);
    case AVR_PINC_ADDR:  return 0xFF; /* No buttons on PORTC */
    case AVR_PIND_ADDR:  return 0xFF;
    case AVR_PINE_ADDR:  return get_pine(periph);
    case AVR_PINF_ADDR:  return get_pinf(periph);

    /* ── GPIO DDR/PORT registers ── */
    case AVR_DDRB_ADDR:  return periph->ddrb;
    case AVR_PORTB_ADDR: return periph->portb;
    case AVR_DDRC_ADDR:  return periph->ddrc;
    case AVR_PORTC_ADDR: return periph->portc;
    case AVR_DDRD_ADDR:  return periph->ddrd;
    case AVR_PORTD_ADDR: return periph->portd;
    case AVR_DDRE_ADDR:  return periph->ddre;
    case AVR_PORTE_ADDR: return periph->porte;
    case AVR_DDRF_ADDR:  return periph->ddrf;
    case AVR_PORTF_ADDR: return periph->portf;

    /* ── SPI ── */
    case AVR_SPCR_ADDR:  return periph->spcr;
    case AVR_SPSR_ADDR:  return periph->spsr | (periph->spi_transfer_complete ? 0x80 : 0);
    case AVR_SPDR_ADDR:
        periph->spi_transfer_complete = false;
        periph->spsr &= ~0x80; /* Clear SPIF on read */
        return periph->spdr;

    /* ── Timer0 ── */
    case AVR_TCCR0A_ADDR: return periph->timer0.tccr0a;
    case AVR_TCCR0B_ADDR: return periph->timer0.tccr0b;
    case AVR_TCNT0_ADDR:  return periph->timer0.tcnt0;
    case AVR_OCR0A_ADDR:  return periph->timer0.ocr0a;
    case AVR_TIMSK0_ADDR: return periph->timer0.timsk0;
    case AVR_TIFR0_ADDR:  return periph->timer0.tifr0;

    /* ── PLL ── */
    case AVR_PLLCSR_ADDR:
        /* PLOCK (bit 0) always set — PLL is always "locked" */
        return periph->cpu->sram[addr] | 0x01;

    /* ── USB controller ── state-machine-driven reads */
    case AVR_UHWCON_ADDR:
    case AVR_USBCON_ADDR:
    case AVR_USBINT_ADDR:
    case AVR_UDCON_ADDR:
    case AVR_UDIEN_ADDR:
    case AVR_UDADDR_ADDR:
    case AVR_UENUM_ADDR:
    case AVR_UERST_ADDR:
    case AVR_UECONX_ADDR:
    case AVR_UECFG0X_ADDR:
    case AVR_UECFG1X_ADDR:
    case AVR_UESTA1X_ADDR:
    case AVR_UEIENX_ADDR:
        return periph->cpu->sram[addr];

    case AVR_USBSTA_ADDR:
        return periph->cpu->sram[addr] | 0x01; /* VBUS always present */

    case AVR_UDINT_ADDR:
        return periph->cpu->sram[addr];

    case AVR_UEINTX_ADDR: {
        uint8_t val = periph->cpu->sram[addr] | TXINI_BIT;
        /* Endpoint registers are banked per-endpoint on real HW.
         * If EP0 is selected and our FSM injected a setup packet,
         * force RXSTPI so the firmware's USB_COM ISR sees it. */
        if (periph->cpu->sram[AVR_UENUM_ADDR] == 0 &&
            periph->usb_enum.ep0_rxstpi_pending) {
            val |= RXSTPI_BIT;
            /* Count each time firmware reads RXSTPI as set (first read per packet) */
            static bool last_rxstpi_read = false;
            if (!last_rxstpi_read) {
                periph->usb_enum.dbg_com_count++;
                last_rxstpi_read = true;
            }
        } else {
            static bool last_rxstpi_read;
            last_rxstpi_read = false;
        }
        return val;
    }

    case AVR_UESTA0X_ADDR:
        return periph->cpu->sram[addr] | 0x80; /* CFGOK — EP config valid */

    case AVR_UEBCLX_ADDR:
        /* Return remaining EP0 FIFO bytes only when EP0 is selected */
        if (periph->cpu->sram[AVR_UENUM_ADDR] == 0 &&
            periph->usb_enum.ep0_fifo_len > periph->usb_enum.ep0_fifo_pos) {
            return periph->usb_enum.ep0_fifo_len - periph->usb_enum.ep0_fifo_pos;
        }
        return periph->cpu->sram[addr];

    case AVR_UEDATX_ADDR:
        /* Read next byte from EP0 FIFO only when EP0 is selected */
        if (periph->cpu->sram[AVR_UENUM_ADDR] == 0 &&
            periph->usb_enum.ep0_fifo_pos < periph->usb_enum.ep0_fifo_len) {
            uint8_t byte = periph->usb_enum.ep0_fifo[periph->usb_enum.ep0_fifo_pos++];
            /* Log for debug */
            if (periph->usb_enum.dbg_datx_count < 24) {
                periph->usb_enum.dbg_datx_log[periph->usb_enum.dbg_datx_count] = byte;
            }
            periph->usb_enum.dbg_datx_count++;
            return byte;
        }
        return 0;

    /* ── ADC — return conversion-complete immediately ── */
    case AVR_ADCSRA_ADDR:
        return periph->cpu->sram[addr] & ~0x40; /* ADSC cleared */
    case AVR_ADCSRB_ADDR:
    case AVR_ADCL_ADDR:
    case AVR_ADCH_ADDR:
        return periph->cpu->sram[addr];

    /* ── Misc system registers ── */
    case AVR_MCUSR_ADDR:
    case AVR_MCUCR_ADDR:
    case AVR_SMCR_ADDR:
    case AVR_CLKPR_ADDR:
        return periph->cpu->sram[addr];

    /* ── EEPROM ── */
    case AVR_EECR_ADDR:   return 0; /* EEPROM ready */
    case AVR_EEDR_ADDR: {
        uint16_t ea = periph->cpu->sram[AVR_EEARL_ADDR] |
                      ((uint16_t)(periph->cpu->sram[AVR_EEARH_ADDR] & 0x03) << 8);
        if (ea < AVR_EEPROM_SIZE) return periph->eeprom[ea];
        return 0xFF;
    }

    /* ── SREG/SP ── */
    case AVR_SREG_ADDR: return periph->cpu->sreg;
    case AVR_SPH_ADDR:  return (periph->cpu->sp >> 8) & 0xFF;
    case AVR_SPL_ADDR:  return periph->cpu->sp & 0xFF;

    default:
        /* Return value from data space for unmapped registers */
        if (addr <= AVR_RAMEND) return periph->cpu->sram[addr];
        return 0;
    }
}

static void io_write_hook(uint16_t addr, uint8_t val, void* ctx) {
    arduboy_periph_t* periph = (arduboy_periph_t*)ctx;
    periph->dbg_io_writes++;

    switch (addr) {
    /* ── GPIO DDR/PORT registers ── */
    case AVR_DDRB_ADDR:  periph->ddrb = val; periph->cpu->sram[addr] = val; break;
    case AVR_PORTB_ADDR: periph->portb = val; periph->cpu->sram[addr] = val; break;
    case AVR_DDRC_ADDR:  periph->ddrc = val; periph->cpu->sram[addr] = val; break;
    case AVR_PORTC_ADDR:
        periph->portc = val;
        periph->cpu->sram[addr] = val;
        /* PC6/PC7 = speaker pins */
        periph->audio_pin1 = (val >> 6) & 1;
        periph->audio_pin2 = (val >> 7) & 1;
        break;
    case AVR_DDRD_ADDR:  periph->ddrd = val; periph->cpu->sram[addr] = val; break;
    case AVR_PORTD_ADDR:
        periph->portd = val;
        periph->cpu->sram[addr] = val;
        periph->dbg_portd_writes++;
        /* Track D/C and CS pins for SSD1306 */
        periph->ssd1306.dc_pin = (val >> 4) & 1;
        periph->ssd1306.cs_pin = (val >> 6) & 1;
        break;
    case AVR_DDRE_ADDR:  periph->ddre = val; periph->cpu->sram[addr] = val; break;
    case AVR_PORTE_ADDR: periph->porte = val; periph->cpu->sram[addr] = val; break;
    case AVR_DDRF_ADDR:  periph->ddrf = val; periph->cpu->sram[addr] = val; break;
    case AVR_PORTF_ADDR: periph->portf = val; periph->cpu->sram[addr] = val; break;

    /* ── SPI ── */
    case AVR_SPCR_ADDR:
        periph->spcr = val;
        periph->cpu->sram[addr] = val;
        break;
    case AVR_SPSR_ADDR:
        periph->spsr = val & 0x01; /* Only SPI2X is writable */
        periph->cpu->sram[addr] = periph->spsr;
        break;
    case AVR_SPDR_ADDR:
        /* SPI data write — immediately "transfers" the byte to SSD1306 */
        periph->spdr = val;
        periph->cpu->sram[addr] = val;
        periph->dbg_spdr_writes++;
        if (val) periph->dbg_spdr_nonzero++;
        ssd1306_spi_byte(&periph->ssd1306, val);
        periph->spi_transfer_complete = true;
        periph->spsr |= 0x80; /* Set SPIF */
        break;

    /* ── Timer0 ── */
    case AVR_TCCR0A_ADDR: periph->timer0.tccr0a = val; periph->cpu->sram[addr] = val; break;
    case AVR_TCCR0B_ADDR: periph->timer0.tccr0b = val; periph->cpu->sram[addr] = val; break;
    case AVR_TCNT0_ADDR:  periph->timer0.tcnt0 = val; periph->cpu->sram[addr] = val; break;
    case AVR_OCR0A_ADDR:  periph->timer0.ocr0a = val; periph->cpu->sram[addr] = val; break;
    case AVR_TIMSK0_ADDR: periph->timer0.timsk0 = val; periph->cpu->sram[addr] = val; break;
    case AVR_TIFR0_ADDR:
        /* Writing 1 to a flag bit clears it */
        periph->timer0.tifr0 &= ~val;
        periph->cpu->sram[addr] = periph->timer0.tifr0;
        break;

    /* ── EEPROM ── */
    case AVR_EECR_ADDR:
        if (val & 0x02) { /* EEPE — EEPROM program enable */
            uint16_t ea = periph->cpu->sram[AVR_EEARL_ADDR] |
                          ((uint16_t)(periph->cpu->sram[AVR_EEARH_ADDR] & 0x03) << 8);
            if (ea < AVR_EEPROM_SIZE) {
                periph->eeprom[ea] = periph->cpu->sram[AVR_EEDR_ADDR];
            }
        }
        if (val & 0x01) { /* EERE — EEPROM read enable */
            uint16_t ea = periph->cpu->sram[AVR_EEARL_ADDR] |
                          ((uint16_t)(periph->cpu->sram[AVR_EEARH_ADDR] & 0x03) << 8);
            if (ea < AVR_EEPROM_SIZE) {
                periph->cpu->sram[AVR_EEDR_ADDR] = periph->eeprom[ea];
            }
        }
        periph->cpu->sram[addr] = val;
        break;
    case AVR_EEDR_ADDR:
    case AVR_EEARL_ADDR:
    case AVR_EEARH_ADDR:
        /* Store in data space for later use */
        periph->cpu->sram[addr] = val;
        break;

    /* ── PLL ── */
    case AVR_PLLCSR_ADDR:
        periph->cpu->sram[addr] = val;
        break;

    /* ── USB controller — FSM-aware writes ── */
    case AVR_UHWCON_ADDR:
    case AVR_USBSTA_ADDR:
    case AVR_UDCON_ADDR:
    case AVR_UDADDR_ADDR:
    case AVR_UENUM_ADDR:
    case AVR_UERST_ADDR:
    case AVR_UECONX_ADDR:
    case AVR_UECFG0X_ADDR:
    case AVR_UECFG1X_ADDR:
    case AVR_UESTA0X_ADDR:
    case AVR_UESTA1X_ADDR:
    case AVR_UEBCLX_ADDR:
    case AVR_UEIENX_ADDR:
        periph->cpu->sram[addr] = val;
        break;

    case AVR_USBCON_ADDR:
        periph->cpu->sram[addr] = val;
        /* Detect USBE=1 to kick off enumeration */
        if ((val & USBE_BIT) && periph->usb_enum.state == USB_ENUM_IDLE) {
            periph->usb_enum.state = USB_ENUM_VBUS;
            periph->usb_enum.delay_counter = USB_ENUM_DELAY;
        }
        break;

    case AVR_USBINT_ADDR:
        /* ATmega32U4 USB registers use write-0-to-clear:
         * firmware writes 0 to a bit to clear it, 1 to keep it.
         * Same convention as UEINTX. */
        periph->cpu->sram[addr] &= val;
        break;

    case AVR_UDIEN_ADDR:
        periph->cpu->sram[addr] = val;
        break;

    case AVR_UEINTX_ADDR:
        /* Writing 0 to a bit clears it (ATmega32U4 convention for UEINTX:
         * write 0 to clear, 1 to keep) */
        periph->cpu->sram[addr] &= val;
        /* If firmware clears RXSTPI on EP0, clear our shadow flag */
        if (periph->cpu->sram[AVR_UENUM_ADDR] == 0 && !(val & RXSTPI_BIT)) {
            periph->usb_enum.ep0_rxstpi_pending = false;
        }
        break;

    case AVR_UEDATX_ADDR:
        /* Firmware writes data to EP — just store */
        periph->cpu->sram[addr] = val;
        break;

    case AVR_UDINT_ADDR:
        /* ATmega32U4 USB registers use write-0-to-clear:
         * firmware writes 0 to a bit to clear it, 1 to keep it.
         * e.g. `UDINT &= ~(1<<EORSTI)` clears EORSTI by writing 0 to bit 3. */
        periph->cpu->sram[addr] &= val;
        break;

    /* ── ADC ── */
    case AVR_ADCSRA_ADDR:
    case AVR_ADCSRB_ADDR:
    case AVR_ADCL_ADDR:
    case AVR_ADCH_ADDR:
        periph->cpu->sram[addr] = val;
        break;

    /* ── Misc system ── */
    case AVR_MCUSR_ADDR:
    case AVR_MCUCR_ADDR:
    case AVR_SMCR_ADDR:
    case AVR_CLKPR_ADDR:
        periph->cpu->sram[addr] = val;
        break;

    /* ── SREG/SP ── */
    case AVR_SREG_ADDR: periph->cpu->sreg = val; periph->cpu->sram[addr] = val; break;
    case AVR_SPH_ADDR:
        periph->cpu->sp = (periph->cpu->sp & 0x00FF) | ((uint16_t)val << 8);
        periph->cpu->sram[addr] = val;
        break;
    case AVR_SPL_ADDR:
        periph->cpu->sp = (periph->cpu->sp & 0xFF00) | val;
        periph->cpu->sram[addr] = val;
        break;

    default:
        /* Store in data space for unmapped I/O registers.
         * avr_data_write no longer stores to sram for I/O addresses,
         * so every hook path must do it explicitly. */
        if (addr <= AVR_RAMEND) periph->cpu->sram[addr] = val;
        break;
    }
}

/* ── Public API ───────────────────────────────────────────────── */

void arduboy_periph_init(arduboy_periph_t* periph, avr_cpu_t* cpu) {
    memset(periph, 0, sizeof(*periph));
    periph->cpu = cpu;
    ssd1306_reset(&periph->ssd1306);
    memset(periph->eeprom, 0xFF, AVR_EEPROM_SIZE); /* EEPROM default = 0xFF */
    avr_cpu_set_io_hooks(cpu, io_read_hook, io_write_hook, periph);
}

void arduboy_periph_reset(arduboy_periph_t* periph) {
    ssd1306_reset(&periph->ssd1306);
    memset(&periph->timer0, 0, sizeof(periph->timer0));
    memset(&periph->usb_enum, 0, sizeof(periph->usb_enum));
    periph->button_state = 0;
    periph->spcr = 0;
    periph->spsr = 0;
    periph->spdr = 0;
    periph->spi_transfer_complete = false;
    periph->ddrb = periph->ddrc = periph->ddrd = 0;
    periph->ddre = periph->ddrf = 0;
    periph->portb = periph->portc = periph->portd = 0;
    periph->porte = periph->portf = 0;
}

void arduboy_periph_tick(arduboy_periph_t* periph, uint32_t cycles) {
    /*
     * ATmega32U4 runs at 16MHz.
     *
     * Microsecond/millis tracking: accumulate raw cycles, convert
     * to microseconds at 16 cycles/µs. This provides the status bar
     * time display.
     *
     * Timer0 prescaler: accumulate cycles, tick timer counter when
     * accumulator reaches the prescaler divisor. This drives the
     * Timer0 overflow interrupt that Arduino's millis() ISR needs.
     */

    /* ── Microsecond / millis tracking (16 cycles = 1µs at 16MHz) ── */
    periph->timer0.cycle_accum += cycles;
    while (periph->timer0.cycle_accum >= 16) {
        periph->timer0.cycle_accum -= 16;
        periph->timer0.micros++;
    }
    while (periph->timer0.micros >= 1000) {
        periph->timer0.micros -= 1000;
        periph->timer0.millis++;
    }

    /* ── Timer0 counter with prescaler accumulator ── */
    uint8_t cs0 = periph->timer0.tccr0b & 0x07;
    uint16_t prescaler_div;
    switch (cs0) {
    case 1: prescaler_div = 1;    break;
    case 2: prescaler_div = 8;    break;
    case 3: prescaler_div = 64;   break;
    case 4: prescaler_div = 256;  break;
    case 5: prescaler_div = 1024; break;
    default: return; /* Timer stopped (cs0=0) or external clock (6,7) */
    }

    periph->timer0.prescaler_accum += cycles;
    while (periph->timer0.prescaler_accum >= prescaler_div) {
        periph->timer0.prescaler_accum -= prescaler_div;
        periph->timer0.tcnt0++;
        if (periph->timer0.tcnt0 == 0) {
            periph->timer0.tifr0 |= 0x01; /* TOV0 — overflow */
        }
        if (periph->timer0.tcnt0 == periph->timer0.ocr0a) {
            periph->timer0.tifr0 |= 0x02; /* OCF0A — compare match A */
        }
    }

    /* ── USB enumeration state machine ── */
    usb_enum_tick(periph, cycles);
}

void arduboy_periph_set_buttons(arduboy_periph_t* periph, uint8_t buttons) {
    periph->button_state = buttons;
}

bool arduboy_periph_fb_dirty(arduboy_periph_t* periph) {
    bool dirty = periph->ssd1306.framebuffer_dirty;
    periph->ssd1306.framebuffer_dirty = false;
    return dirty;
}

const uint8_t* arduboy_periph_get_fb(arduboy_periph_t* periph) {
    return periph->ssd1306.framebuffer;
}

uint32_t arduboy_periph_millis(arduboy_periph_t* periph) {
    return periph->timer0.millis;
}

void arduboy_periph_check_interrupts(arduboy_periph_t* periph) {
    avr_cpu_t* cpu = periph->cpu;

    /* Interrupts only fire when global interrupt flag (I) is set */
    if (!(cpu->sreg & (1 << SREG_I))) return;

    /* ── USB interrupts (higher priority than Timer0) ── */
    usb_enum_t* usb = &periph->usb_enum;

    if (usb->gen_irq_pending) {
        /* USB General interrupt — check if UDIEN has matching enable bits */
        uint8_t udien = cpu->sram[AVR_UDIEN_ADDR];
        uint8_t udint = cpu->sram[AVR_UDINT_ADDR];
        uint8_t usbint = cpu->sram[AVR_USBINT_ADDR];

        /* Fire if any enabled interrupt flag is set */
        bool should_fire = (udint & udien) || usbint;
        if (should_fire) {
            /* Dispatch interrupt — direct SRAM push (SP is always in SRAM) */
            cpu->sreg &= ~(1 << SREG_I);
            cpu->sram[cpu->sp--] = cpu->pc & 0xFF;
            cpu->sram[cpu->sp--] = (cpu->pc >> 8) & 0xFF;
            cpu->sleeping = false;
            periph->dbg_irq_count++;
            cpu->pc = AVR_VECT_USBGEN;
            usb->gen_irq_pending = false;
            return; /* Only one interrupt per check */
        }
    }

    if (usb->end_irq_pending) {
        /* USB Endpoint interrupt — dispatch unconditionally.
         * The banked UEIENX/UEINTX in SRAM may reflect a non-EP0
         * endpoint after SET_CONFIGURATION configured CDC endpoints.
         * Our ep0_rxstpi shadow ensures the ISR finds the packet. */
        cpu->sreg &= ~(1 << SREG_I);
        cpu->sram[cpu->sp--] = cpu->pc & 0xFF;
        cpu->sram[cpu->sp--] = (cpu->pc >> 8) & 0xFF;
        cpu->sleeping = false;
        periph->dbg_irq_count++;
        cpu->pc = AVR_VECT_USBEND;
        usb->end_irq_pending = false;
        return;
    }

    /* ── Timer0 interrupts ── */
    uint8_t pending = periph->timer0.tifr0 & periph->timer0.timsk0;
    if (pending == 0) return;

    uint8_t vect_flag = 0;
    uint16_t vect_addr = 0;

    /* Prioritize by vector number (lower address = higher priority) */
    if (pending & 0x02) {          /* OCF0A — Timer0 Compare A Match */
        vect_flag = 0x02;
        vect_addr = AVR_VECT_TIMER0_COMPA;
    } else if (pending & 0x01) {   /* TOV0 — Timer0 Overflow */
        vect_flag = 0x01;
        vect_addr = AVR_VECT_TIMER0_OVF;
    }

    if (vect_flag == 0) return;

    /*
     * Dispatch interrupt:
     *  1. Clear global interrupt enable (I flag)
     *  2. Push current PC onto stack (low byte first, same as CALL)
     *  3. Clear the specific interrupt flag
     *  4. Jump to interrupt vector
     *  5. Wake CPU if sleeping
     */
    cpu->sreg &= ~(1 << SREG_I);

    /* Push PC — direct SRAM (SP always points into SRAM, not I/O space) */
    cpu->sram[cpu->sp--] = cpu->pc & 0xFF;
    cpu->sram[cpu->sp--] = (cpu->pc >> 8) & 0xFF;

    /* Clear the specific interrupt flag */
    periph->timer0.tifr0 &= ~vect_flag;

    /* Wake CPU if sleeping */
    cpu->sleeping = false;

    /* Track interrupt dispatches */
    periph->dbg_irq_count++;

    /* Jump to interrupt vector */
    cpu->pc = vect_addr;
}
