/*
 * arduboy_periph.h — ATmega32U4 peripheral emulation for Arduboy
 *
 * Emulates the specific peripherals Arduboy games interact with:
 *   - SSD1306 OLED controller (SPI interface, 128×64 1-bit framebuffer)
 *   - Buttons (6 GPIO pins: UP/DOWN/LEFT/RIGHT/A/B)
 *   - Timer0 (millis()/delay() timebase)
 *   - EEPROM (1KB, for save data)
 *   - Audio pins (stub — two GPIOs for piezo)
 *
 * This layer sits between the AVR CPU's I/O hooks and the display/input
 * backends, making it backend-agnostic.
 *
 * Copyright (c) 2026 Bus Pirate project. MIT License.
 */

#ifndef ARDUBOY_PERIPH_H
#define ARDUBOY_PERIPH_H

#include <stdint.h>
#include <stdbool.h>
#include "avr_cpu.h"

/* ── ATmega32U4 interrupt vector addresses (word addresses) ────
 *    From datasheet Table 11-1: addr = vect_num * 2
 *    vect_num is 0-based (0 = RESET) ──────────────────────────── */
#define AVR_VECT_USBGEN        0x0014  /* USB General Interrupt (vect 10) */
#define AVR_VECT_USBEND        0x0016  /* USB Endpoint Interrupt (vect 11) */
#define AVR_VECT_TIMER1_OVF    0x0028  /* Timer1 Overflow (vect 20) */
#define AVR_VECT_TIMER0_COMPA  0x002A  /* Timer0 Compare A Match (vect 21) */
#define AVR_VECT_TIMER0_COMPB  0x002C  /* Timer0 Compare B Match (vect 22) */
#define AVR_VECT_TIMER0_OVF    0x002E  /* Timer0 Overflow (vect 23) */
#define AVR_VECT_SPI_STC       0x0030  /* SPI Transfer Complete (vect 24) */

/* ── Arduboy display dimensions ──────────────────────────────── */
#define ARDUBOY_WIDTH       128
#define ARDUBOY_HEIGHT      64
#define ARDUBOY_FB_SIZE     (ARDUBOY_WIDTH * ARDUBOY_HEIGHT / 8)  /* 1024 bytes */

/* ── Arduboy button bits (as read from emulated GPIO) ────────── */
#define ARDUBOY_BTN_UP      0x01
#define ARDUBOY_BTN_DOWN    0x02
#define ARDUBOY_BTN_LEFT    0x04
#define ARDUBOY_BTN_RIGHT   0x08
#define ARDUBOY_BTN_A       0x10
#define ARDUBOY_BTN_B       0x20

/* ── SSD1306 state machine ───────────────────────────────────── */

typedef enum {
    SSD1306_IDLE,
    SSD1306_CMD,        /* Receiving command byte */
    SSD1306_CMD_ARG,    /* Receiving command argument */
    SSD1306_DATA,       /* Receiving framebuffer data */
} ssd1306_state_t;

typedef struct {
    ssd1306_state_t state;
    uint8_t framebuffer[ARDUBOY_FB_SIZE];  /* 128×64, column-major pages */
    uint16_t write_ptr;                     /* Current write position */
    uint8_t col_start;
    uint8_t col_end;
    uint8_t page_start;
    uint8_t page_end;
    uint8_t col;
    uint8_t page;
    bool display_on;
    bool framebuffer_dirty;                 /* Set when new data written */

    /* D/C pin state (data/command select) — tracked from PORTD bit 4 */
    bool dc_pin;        /* false = command, true = data */

    /* CS pin state — tracked from PORTD bit 6 */
    bool cs_pin;        /* false = selected (active low) */

    /* Transfer tracking */
    bool transfer_complete;  /* Set when page wraps → full screen sent */
    uint32_t data_count;     /* Bytes in current transfer */

    /* Pending command for multi-byte sequences */
    uint8_t pending_cmd;
    uint8_t pending_args;
    uint8_t arg_count;

    /* Debug (temporary) */
    uint32_t dbg_data_count;
    uint32_t dbg_data_nonzero;
    uint32_t dbg_cmd_count;
    uint32_t dbg_transfer_count;
} ssd1306_t;

/* ── Timer state ─────────────────────────────────────────────── */

typedef struct {
    uint32_t millis;            /* Emulated millis() counter */
    uint32_t micros;            /* Fractional microseconds accumulator */
    uint32_t prescaler_accum;   /* Cycle accumulator for prescaler division */
    uint32_t cycle_accum;       /* Sub-microsecond cycle accumulator */
    uint8_t tccr0a;
    uint8_t tccr0b;
    uint8_t tcnt0;
    uint8_t ocr0a;
    uint8_t timsk0;
    uint8_t tifr0;
} avr_timer_t;

/* ── USB CDC enumeration state machine ────────────────────────── */
/*
 * Simulates the host-side USB enumeration sequence that Arduino's
 * CDC serial needs in order to set lineState (breaking `while(!Serial)`).
 *
 * Modelled after ProjectABE's 3-phase approach:
 *   Phase 0: Idle — waiting for firmware to write USBCON with USBE=1
 *   Phase 1: VBUS detected — fire USB_GEN interrupt (VBUSTI)
 *   Phase 2: End-of-reset — inject SET_CONFIGURATION setup packet
 *   Phase 3: CDC line state — inject SET_CONTROL_LINE_STATE packet
 *   Phase 4: Done — enumeration complete, lineState set
 */

typedef enum {
    USB_ENUM_IDLE,           /* Waiting for USBE write */
    USB_ENUM_VBUS,           /* VBUS detected, delaying before reset */
    USB_ENUM_RESET,          /* End-of-reset, configuring EP0 */
    USB_ENUM_SET_CONFIG,     /* SET_CONFIGURATION packet in EP0 FIFO */
    USB_ENUM_SET_LINE_STATE, /* SET_CONTROL_LINE_STATE in EP0 FIFO */
    USB_ENUM_DONE,           /* Enumeration complete */
} usb_enum_state_t;

#define USB_EP0_FIFO_SIZE  8

typedef struct {
    usb_enum_state_t state;
    uint32_t delay_counter;     /* Cycles remaining before next phase */
    uint8_t ep0_fifo[USB_EP0_FIFO_SIZE]; /* Setup packet data */
    uint8_t ep0_fifo_pos;       /* Read position in FIFO */
    uint8_t ep0_fifo_len;       /* Bytes available in FIFO */
    bool gen_irq_pending;       /* USB_GEN interrupt pending */
    bool end_irq_pending;       /* USB_END interrupt pending */
    bool ep0_rxstpi_pending;    /* EP0 has unprocessed setup packet */
    uint8_t dbg_datx_log[24];   /* Last UEDATX bytes read */
    uint8_t dbg_datx_count;     /* Total UEDATX reads */
    uint8_t dbg_com_count;      /* USB_COM_vect entries (RXSTPI seen) */
} usb_enum_t;

/* ── Full peripheral state ───────────────────────────────────── */

typedef struct {
    avr_cpu_t* cpu;             /* Back-reference to CPU */

    /* SSD1306 OLED emulation */
    ssd1306_t ssd1306;

    /* Buttons */
    uint8_t button_state;       /* Bitmask of ARDUBOY_BTN_* */

    /* Timers */
    avr_timer_t timer0;

    /* EEPROM */
    uint8_t eeprom[AVR_EEPROM_SIZE];

    /* Audio stub state */
    bool audio_pin1;            /* Speaker pin 1 */
    bool audio_pin2;            /* Speaker pin 2 */

    /* USB CDC enumeration state machine */
    usb_enum_t usb_enum;

    /* SPI state for SSD1306 */
    uint8_t spcr;
    uint8_t spsr;
    uint8_t spdr;
    bool spi_transfer_complete;

    /* Port direction registers (for input detection) */
    uint8_t ddrb, ddrc, ddrd, ddre, ddrf;
    uint8_t portb, portc, portd, porte, portf;

    /* Debug counters (temporary) */
    uint32_t dbg_spdr_writes;
    uint32_t dbg_spdr_nonzero;
    uint32_t dbg_ssd_data_bytes;
    uint32_t dbg_ssd_cmd_bytes;
    uint32_t dbg_portd_writes;
    uint32_t dbg_io_writes;
    uint32_t dbg_io_reads;
    uint32_t dbg_steps;
    uint32_t dbg_irq_count;
    uint16_t dbg_last_unhandled_opcode;
    uint16_t dbg_hot_addr;
    uint32_t dbg_hot_count;
    uint16_t dbg_last_read_addr;

} arduboy_periph_t;

/* ── Public API ───────────────────────────────────────────────── */

/**
 * Initialize peripheral state and connect to CPU via I/O hooks.
 */
void arduboy_periph_init(arduboy_periph_t* periph, avr_cpu_t* cpu);

/**
 * Reset peripheral state (called on CPU reset).
 */
void arduboy_periph_reset(arduboy_periph_t* periph);

/**
 * Advance timer state by `cycles` CPU cycles.
 * Call after each avr_cpu_step() or batch.
 */
void arduboy_periph_tick(arduboy_periph_t* periph, uint32_t cycles);

/**
 * Set button state. Buttons are packed as ARDUBOY_BTN_* bitmask.
 */
void arduboy_periph_set_buttons(arduboy_periph_t* periph, uint8_t buttons);

/**
 * Check if the display framebuffer has been updated since last check.
 * Clears the dirty flag on read.
 */
bool arduboy_periph_fb_dirty(arduboy_periph_t* periph);

/**
 * Get pointer to the 1024-byte framebuffer (read-only).
 * Format: column-major, 8 pages of 128 columns. Bit 0 = top pixel.
 */
const uint8_t* arduboy_periph_get_fb(arduboy_periph_t* periph);

/**
 * Check for pending interrupts and dispatch if enabled.
 * Call after each avr_cpu_step() + arduboy_periph_tick().
 * Handles Timer0 OVF/COMPA vectors used by Arduino millis().
 */
void arduboy_periph_check_interrupts(arduboy_periph_t* periph);

/**
 * Get current millis count (for debug/status).
 */
uint32_t arduboy_periph_millis(arduboy_periph_t* periph);

#endif /* ARDUBOY_PERIPH_H */
