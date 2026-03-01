/*
 * avr_cpu.h — ATmega32U4 AVR interpreter core
 *
 * Lightweight instruction-level AVR interpreter targeting the ATmega32U4
 * (Arduboy's MCU). Executes .hex binaries loaded into emulated flash.
 *
 * Memory model:
 *   - 32KB flash (program memory)
 *   - 2.5KB SRAM (0x0100–0x0AFF)
 *   - 32 general-purpose registers (R0–R31) mapped at 0x0000–0x001F
 *   - 64 I/O registers at 0x0020–0x005F
 *   - 160 extended I/O registers at 0x0060–0x00FF
 *
 * Copyright (c) 2026 Bus Pirate project. MIT License.
 */

#ifndef AVR_CPU_H
#define AVR_CPU_H

#include <stdint.h>
#include <stdbool.h>

/* ── ATmega32U4 memory sizes ──────────────────────────────────── */
#define AVR_FLASH_SIZE      32768   /* 32KB program flash */
#define AVR_SRAM_SIZE       2560    /* 2.5KB SRAM */
#define AVR_SRAM_START      0x0100
#define AVR_SRAM_END        (AVR_SRAM_START + AVR_SRAM_SIZE - 1)
#define AVR_IO_BASE         0x0020
#define AVR_EXT_IO_BASE     0x0060
#define AVR_RAMEND          0x0AFF
#define AVR_EEPROM_SIZE     1024    /* 1KB EEPROM */

/* ── SREG bit positions ───────────────────────────────────────── */
#define SREG_C  0   /* Carry */
#define SREG_Z  1   /* Zero */
#define SREG_N  2   /* Negative */
#define SREG_V  3   /* Overflow */
#define SREG_S  4   /* Sign (N xor V) */
#define SREG_H  5   /* Half-carry */
#define SREG_T  6   /* Transfer bit */
#define SREG_I  7   /* Global interrupt enable */

/* ── I/O register addresses (data-space, not I/O-space) ──────── */
#define AVR_SREG_ADDR       0x5F
#define AVR_SPH_ADDR        0x5E
#define AVR_SPL_ADDR        0x5D

/* ATmega32U4 port registers — used by Arduboy for buttons + SSD1306 */
#define AVR_PINB_ADDR       0x23
#define AVR_DDRB_ADDR       0x24
#define AVR_PORTB_ADDR      0x25
#define AVR_PINC_ADDR       0x26
#define AVR_DDRC_ADDR       0x27
#define AVR_PORTC_ADDR      0x28
#define AVR_PIND_ADDR       0x29
#define AVR_DDRD_ADDR       0x2A
#define AVR_PORTD_ADDR      0x2B
#define AVR_PINE_ADDR       0x2C
#define AVR_DDRE_ADDR       0x2D
#define AVR_PORTE_ADDR      0x2E
#define AVR_PINF_ADDR       0x2F
#define AVR_DDRF_ADDR       0x30
#define AVR_PORTF_ADDR      0x31

/* PLL */
#define AVR_PLLCSR_ADDR     0x49  /* PLL Control and Status Register */

/* SPI registers */
#define AVR_SPCR_ADDR       0x4C
#define AVR_SPSR_ADDR       0x4D
#define AVR_SPDR_ADDR       0x4E

/* Misc system registers */
#define AVR_SMCR_ADDR       0x53  /* Sleep Mode Control */
#define AVR_MCUSR_ADDR      0x54  /* MCU Status Register */
#define AVR_MCUCR_ADDR      0x55  /* MCU Control Register */
#define AVR_CLKPR_ADDR      0x61  /* Clock Prescale Register */

/* Timer registers */
#define AVR_TCCR0A_ADDR     0x44
#define AVR_TCCR0B_ADDR     0x45
#define AVR_TCNT0_ADDR      0x46
#define AVR_OCR0A_ADDR      0x47
#define AVR_OCR0B_ADDR      0x48
#define AVR_TIMSK0_ADDR     0x6E  /* extended I/O */
#define AVR_TIFR0_ADDR      0x35

/* ADC registers */
#define AVR_ADCL_ADDR       0x78
#define AVR_ADCH_ADDR       0x79
#define AVR_ADCSRA_ADDR     0x7A
#define AVR_ADCSRB_ADDR     0x7B

/* EEPROM registers */
#define AVR_EECR_ADDR       0x3F
#define AVR_EEDR_ADDR       0x40
#define AVR_EEARL_ADDR      0x41
#define AVR_EEARH_ADDR      0x42

/* USB controller registers (ATmega32U4) */
#define AVR_UHWCON_ADDR     0xD7  /* USB HW Configuration */
#define AVR_USBCON_ADDR     0xD8  /* USB General Control */
#define AVR_USBSTA_ADDR     0xD9  /* USB General Status */
#define AVR_USBINT_ADDR     0xDA  /* USB General Interrupt */
#define AVR_UDCON_ADDR      0xE0  /* USB Device Control */
#define AVR_UDINT_ADDR      0xE1  /* USB Device Interrupt */
#define AVR_UDIEN_ADDR      0xE2  /* USB Device Interrupt Enable */
#define AVR_UDADDR_ADDR     0xE3  /* USB Device Address */
#define AVR_UEINTX_ADDR     0xE8  /* USB Endpoint Interrupt */
#define AVR_UENUM_ADDR      0xE9  /* USB Endpoint Number */
#define AVR_UERST_ADDR      0xEA  /* USB Endpoint Reset */
#define AVR_UECONX_ADDR     0xEB  /* USB Endpoint Control */
#define AVR_UECFG0X_ADDR    0xEC  /* USB Endpoint Config 0 */
#define AVR_UECFG1X_ADDR    0xED  /* USB Endpoint Config 1 */
#define AVR_UESTA0X_ADDR    0xEE  /* USB Endpoint Status 0 */
#define AVR_UESTA1X_ADDR    0xEF  /* USB Endpoint Status 1 */
#define AVR_UEBCLX_ADDR     0xF2  /* USB Endpoint Byte Count Low */
#define AVR_UEDATX_ADDR     0xF1  /* USB Endpoint Data */
#define AVR_UEIENX_ADDR     0xF0  /* USB Endpoint Interrupt Enable */

/* ── I/O read/write hook signatures ──────────────────────────── */

/**
 * I/O write hook — called when AVR code writes to an I/O register.
 * @param addr  Data-space address (0x0020–0x00FF)
 * @param val   Byte value being written
 * @param ctx   User context pointer
 */
typedef void (*avr_io_write_fn)(uint16_t addr, uint8_t val, void* ctx);

/**
 * I/O read hook — called when AVR code reads an I/O register.
 * @param addr  Data-space address (0x0020–0x00FF)
 * @param ctx   User context pointer
 * @return      Byte value read
 */
typedef uint8_t (*avr_io_read_fn)(uint16_t addr, void* ctx);

/* ── CPU state ────────────────────────────────────────────────── */

typedef struct {
    /* Registers */
    uint8_t r[32];          /* R0–R31 */
    uint16_t pc;            /* Program counter (word address) */
    uint16_t sp;            /* Stack pointer */
    uint8_t sreg;           /* Status register */

    /* Memory */
    uint8_t* flash;         /* Program memory (AVR_FLASH_SIZE bytes) */
    uint8_t* sram;          /* Full data space: regs + I/O + ext I/O + SRAM */
    uint8_t* eeprom;        /* EEPROM (AVR_EEPROM_SIZE bytes) */

    /* Execution state */
    uint64_t cycle_count;   /* Total cycles executed */
    bool halted;            /* Set on BREAK or fatal error */
    bool sleeping;          /* Set on SLEEP instruction */

    /* Debug counters (temporary) */
    uint32_t dbg_lpm_count;
    uint32_t dbg_lpm_nonzero;
    uint32_t dbg_sram_writes;    /* writes to addr >= 0x100 */
    uint32_t dbg_sram_writes_nz; /* non-zero writes to addr >= 0x100 */
    uint32_t dbg_unrecognized;   /* unrecognized opcodes */
    uint16_t dbg_last_bad_op;    /* last unrecognized opcode */

    /* I/O hooks */
    avr_io_write_fn io_write;
    avr_io_read_fn io_read;
    void* io_ctx;           /* Context pointer passed to hooks */

} avr_cpu_t;

/* ── Public API ───────────────────────────────────────────────── */

/**
 * Initialize CPU state. Caller must provide pre-allocated memory buffers.
 * @param cpu       CPU state struct
 * @param flash     Buffer of AVR_FLASH_SIZE bytes (program memory)
 * @param sram      Buffer of (AVR_RAMEND + 1) bytes (full data space)
 * @param eeprom    Buffer of AVR_EEPROM_SIZE bytes (EEPROM)
 */
void avr_cpu_init(avr_cpu_t* cpu, uint8_t* flash, uint8_t* sram, uint8_t* eeprom);

/**
 * Reset CPU to power-on state. Flash/SRAM contents are preserved.
 */
void avr_cpu_reset(avr_cpu_t* cpu);

/**
 * Set I/O read/write hooks for peripheral emulation.
 */
void avr_cpu_set_io_hooks(avr_cpu_t* cpu, avr_io_read_fn read, avr_io_write_fn write, void* ctx);

/**
 * Execute one instruction. Returns the number of cycles consumed.
 */
uint8_t avr_cpu_step(avr_cpu_t* cpu);

/**
 * Execute up to `max_cycles` worth of instructions.
 * Returns total cycles actually executed.
 */
uint32_t avr_cpu_run(avr_cpu_t* cpu, uint32_t max_cycles);

/**
 * Load an Intel HEX file from a memory buffer into flash.
 * @param cpu       CPU state
 * @param hex_data  Null-terminated Intel HEX string
 * @param hex_len   Length of hex_data
 * @return          true on success, false on parse error
 */
bool avr_load_hex(avr_cpu_t* cpu, const char* hex_data, uint32_t hex_len);

/* ── Inline helpers ───────────────────────────────────────────── */

static inline void avr_sreg_set(avr_cpu_t* cpu, uint8_t bit, bool val) {
    if (val) cpu->sreg |= (1 << bit);
    else     cpu->sreg &= ~(1 << bit);
}

static inline bool avr_sreg_get(avr_cpu_t* cpu, uint8_t bit) {
    return (cpu->sreg >> bit) & 1;
}

/* Read a 16-bit word from flash (little-endian, word-addressed) */
static inline uint16_t avr_flash_read_word(avr_cpu_t* cpu, uint16_t word_addr) {
    uint32_t byte_addr = (uint32_t)word_addr * 2;
    if (byte_addr + 1 >= AVR_FLASH_SIZE) return 0;
    return (uint16_t)cpu->flash[byte_addr] | ((uint16_t)cpu->flash[byte_addr + 1] << 8);
}

/* Data space read — routes through I/O hook if applicable */
static inline uint8_t avr_data_read(avr_cpu_t* cpu, uint16_t addr) {
    if (addr >= AVR_IO_BASE && addr < AVR_SRAM_START && cpu->io_read) {
        return cpu->io_read(addr, cpu->io_ctx);
    }
    if (addr <= AVR_RAMEND) return cpu->sram[addr];
    return 0;
}

/* Data space write — routes through I/O hook if applicable */
static inline void avr_data_write(avr_cpu_t* cpu, uint16_t addr, uint8_t val) {
    if (addr >= AVR_IO_BASE && addr < AVR_SRAM_START) {
        if (cpu->io_write) {
            /* The I/O hook is responsible for updating sram[addr]
             * with the correct semantics (write-0-to-clear, etc.).
             * Do NOT overwrite sram[addr] after the hook returns. */
            cpu->io_write(addr, val, cpu->io_ctx);
            return;
        }
    }
    if (addr <= AVR_RAMEND) {
        cpu->sram[addr] = val;
    }
}

#endif /* AVR_CPU_H */
