/*
 * This file is part of the Bus Pirate project (http://code.google.com/p/the-bus-pirate/).
 *
 * Written and maintained by the Bus Pirate project.
 *
 * To the extent possible under law, the project has
 * waived all copyright and related or neighboring rights to Bus Pirate. This
 * work is published from United States.
 *
 * For details see: http://creativecommons.org/publicdomain/zero/1.0/.
 *
 *    This program is distributed in the hope that it will be useful,
 *    but WITHOUT ANY WARRANTY; without even the implied warranty of
 *    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 */

/**
 * @file pic24_icsp.c
 * @brief PIC24FJ 2-wire ICSP (Microchip DS39768) on three buffered IO pins.
 * @details Bit-banged with the bio_* functions. PGC and MCLR are always driven. PGD is
 *          driven except from a REGOUT to the first rising edge of the next command, as
 *          the PIC may drive it until then (DS39768 3.2.2). Bits go least significant
 *          first: the PIC latches PGD on the rising edge of PGC, and when it answers it
 *          changes PGD after the falling edge, so PGD is read just before PGC falls.
 *          The instruction sequences are those of DS39768 Tables 3-4, 3-5 and 3-8.
 */

#include "pico/stdlib.h"
#include "hardware/clocks.h"
#include "pirate.h"
#include "pirate/bio.h"
#include "pirate/pic24_icsp.h"

#define ICSP_KEY 0x4D434851u // shifted in most significant bit first
#define ICSP_SIX 0x0u        // 4-bit control codes
#define ICSP_REGOUT 0x1u

// Entry delays, with margin over DS39768 Table 7-1: P18 >= 40 ns, P19 >= 1 ms, P7 >= 25 ms
#define ICSP_RESET_US 100u
#define ICSP_MCLR_PULSE_US 20u
#define ICSP_P18_US 100u
#define ICSP_P19_US 1500u
#define ICSP_P7_MS 30u

// Instructions (Wn in the low nibble)
#define NOP 0x000000u
#define GOTO_0X200 0x040200u                                                     // its second word is a NOP
#define MOV_LIT_W(lit, w) (0x200000u | ((uint32_t)((lit) & 0xFFFFu) << 4) | (w)) // MOV #lit16, Wn
#define MOV_W0_TBLPAG 0x880190u
#define MOV_W10_NVMCON 0x883B0Au
#define MOV_NVMCON_W2 0x803B02u
#define MOV_W0_VISI 0x883C20u
#define MOV_W2_VISI 0x883C22u
#define TBLWTL_W0_IND_W0 0xBB0800u // TBLWTL W0, [W0]
#define BSET_NVMCON_WR 0xA8E761u
#define CLR_W6 0xEB0300u
#define TBLWTL_W6PP_W7 0xBB0BB6u    // TBLWTL [W6++], [W7]
#define TBLWTHB_W6PP_W7PP 0xBBDBB6u // TBLWTH.B [W6++], [W7++]
#define TBLWTHB_W6PP_PPW7 0xBBEBB6u // TBLWTH.B [W6++], [++W7]
#define TBLWTL_W6PP_W7PP 0xBB1BB6u  // TBLWTL [W6++], [W7++]
#define TBLRDL_W6_W7 0xBA0B96u      // TBLRDL [W6], [W7]
#define TBLRDHB_W6PP_W7PP 0xBADBB6u // TBLRDH.B [W6++], [W7++]
#define TBLRDHB_PPW6_W7MM 0xBAD3D6u // TBLRDH.B [++W6], [W7--]
#define TBLRDL_W6PP_W7 0xBA0BB6u    // TBLRDL [W6++], [W7]

#define VISI_ADDR 0x0784u
#define NVMCON_ERASE 0x404Fu     // chip erase, of user memory only while TBLPAG < 0x80
#define NVMCON_WRITE_ROW 0x4001u // program one row of 64 instructions
#define NVMCON_WR (1u << 15)
#define NVMCON_WRERR (1u << 13)

#define ICSP_PGD_SETUP_NS 100u // PGD driven again to the next rising edge after a REGOUT (P2 >= 15 ns)

static struct {
    uint8_t pgc, pgd, mclr;
    bool pgd_released;     // PGD is an input from a REGOUT to the next rising edge
    uint32_t phase_cycles; // one PGC phase, in system clock cycles
    uint32_t setup_cycles; // ICSP_PGD_SETUP_NS, in system clock cycles
} icsp;

void pic24_icsp_init(uint8_t pgc, uint8_t pgd, uint8_t mclr, uint32_t phase_ns) {
    icsp.pgc = pgc;
    icsp.pgd = pgd;
    icsp.mclr = mclr;
    pic24_icsp_set_phase_ns(phase_ns);
}

void pic24_icsp_set_phase_ns(uint32_t phase_ns) {
    uint64_t hz = clock_get_hz(clk_sys);
    icsp.phase_cycles = (uint32_t)((hz * phase_ns) / 1000000000u);
    icsp.setup_cycles = (uint32_t)((hz * ICSP_PGD_SETUP_NS) / 1000000000u);
}

static inline void phase_wait(void) {
    busy_wait_at_least_cycles(icsp.phase_cycles);
}

// One PGC clock with PGD already set: low phase, rising edge (the PIC latches PGD), high phase
static void clock_pulse(void) {
    phase_wait();
    if (icsp.pgd_released) { // after a REGOUT, PGD is driven again just before this edge
        bio_output(icsp.pgd);
        icsp.pgd_released = false;
        busy_wait_at_least_cycles(icsp.setup_cycles);
    }
    bio_put(icsp.pgc, true);
    phase_wait();
    bio_put(icsp.pgc, false);
}

// Least significant bit first
static void bits_out(uint32_t value, uint8_t count) {
    for (uint8_t i = 0; i < count; i++) {
        bio_put(icsp.pgd, value & 1u);
        value >>= 1;
        clock_pulse();
    }
}

void pic24_icsp_enter(void) {
    // MCLR low first: the PIC is in reset and its pins are inputs before PGC and PGD are driven.
    // PGC then stays low: a rising edge on PGC with MCLR low is taken as a key bit.
    bio_put(icsp.mclr, false);
    bio_output(icsp.mclr);
    bio_put(icsp.pgc, false);
    bio_output(icsp.pgc);
    bio_put(icsp.pgd, false);
    bio_output(icsp.pgd);
    icsp.pgd_released = false;
    busy_wait_us(ICSP_RESET_US);
    bio_put(icsp.mclr, true); // MCLR briefly high, then low
    busy_wait_us(ICSP_MCLR_PULSE_US);
    bio_put(icsp.mclr, false);
    busy_wait_us(ICSP_P18_US);
    for (int8_t i = 31; i >= 0; i--) {
        bio_put(icsp.pgd, (ICSP_KEY >> i) & 1u);
        clock_pulse();
    }
    bio_put(icsp.pgd, false);
    busy_wait_us(ICSP_P19_US);
    bio_put(icsp.mclr, true);
    busy_wait_ms(ICSP_P7_MS);
    // The first SIX is forced and takes 9 bits. Its instruction only advances the PC, and the PIC
    // resets when the PC reaches the vector table at 0x0004, so GOTO 0x200 must come next.
    bits_out(0, 5);
    pic24_icsp_six(NOP);
    pic24_icsp_six(GOTO_0X200);
    pic24_icsp_six(NOP);
}

void pic24_icsp_exit(void) {
    bio_put(icsp.pgc, false);
    bio_put(icsp.pgd, false); // still an input if the last command was a REGOUT
    busy_wait_us(1);          // P16
    bio_put(icsp.mclr, false);
}

void pic24_icsp_six(uint32_t instruction) {
    bits_out(ICSP_SIX, 4);
    bits_out(instruction, 24);
}

uint16_t pic24_icsp_regout(void) {
    bits_out(ICSP_REGOUT, 4);
    bio_input(icsp.pgd); // the PIC drives PGD after 8 idle clocks
    for (uint8_t i = 0; i < 8; i++) {
        clock_pulse();
    }
    uint16_t value = 0;
    for (uint8_t i = 0; i < 16; i++) {
        phase_wait();
        bio_put(icsp.pgc, true);
        phase_wait();
        if (bio_get(icsp.pgd)) {
            value |= (uint16_t)(1u << i);
        }
        bio_put(icsp.pgc, false);
    }
    // The PIC may drive PGD up to the first rising edge of the next command (DS39768 3.2.2):
    // clock_pulse() drives it again just before that edge
    bio_put(icsp.pgd, false);
    icsp.pgd_released = true;
    return value;
}

uint16_t pic24_icsp_visi_echo(uint16_t pattern) {
    pic24_icsp_six(MOV_LIT_W(pattern, 0));
    pic24_icsp_six(MOV_W0_VISI);
    pic24_icsp_six(NOP);
    uint16_t visi = pic24_icsp_regout();
    pic24_icsp_six(NOP);
    return visi;
}

// The PC keeps counting in ICSP mode, and the PIC resets if it leaves the flash: park it at 0x200
static void park_pc(void) {
    pic24_icsp_six(GOTO_0X200);
    pic24_icsp_six(NOP);
}

// TBLPAG = address bits 23:16, Ww = address bits 15:0 (W0 is used too)
static void table_pointer(uint32_t address, uint8_t w) {
    pic24_icsp_six(MOV_LIT_W(address >> 16, 0));
    pic24_icsp_six(MOV_W0_TBLPAG);
    pic24_icsp_six(MOV_LIT_W(address, w));
}

void pic24_icsp_read(uint32_t address, uint32_t* words, uint32_t count) {
    park_pc();
    table_pointer(address, 6);
    pic24_icsp_six(MOV_LIT_W(VISI_ADDR, 7));
    pic24_icsp_six(NOP);
    for (uint32_t i = 0; i + 1 < count; i += 2) {
        if (i && (i % PIC24_ICSP_ROW_WORDS) == 0) {
            park_pc(); // W6 and W7 keep their values
        }
        pic24_icsp_six(TBLRDL_W6_W7);
        pic24_icsp_six(NOP);
        pic24_icsp_six(NOP);
        uint16_t lsw0 = pic24_icsp_regout();
        pic24_icsp_six(NOP);
        pic24_icsp_six(TBLRDHB_W6PP_W7PP);
        pic24_icsp_six(NOP);
        pic24_icsp_six(NOP);
        pic24_icsp_six(TBLRDHB_PPW6_W7MM);
        pic24_icsp_six(NOP);
        pic24_icsp_six(NOP);
        uint16_t msb = pic24_icsp_regout();
        pic24_icsp_six(NOP);
        pic24_icsp_six(TBLRDL_W6PP_W7);
        pic24_icsp_six(NOP);
        pic24_icsp_six(NOP);
        uint16_t lsw1 = pic24_icsp_regout();
        pic24_icsp_six(NOP);
        words[i] = ((uint32_t)(msb & 0xFFu) << 16) | lsw0;
        words[i + 1] = ((uint32_t)(msb >> 8) << 16) | lsw1;
    }
}

// Poll WR until the PIC clears it (Table 3-4, step 5)
static pic24_icsp_status_t wait_write_done(uint32_t timeout_ms) {
    absolute_time_t deadline = make_timeout_time_ms(timeout_ms);
    while (true) {
        park_pc();
        pic24_icsp_six(MOV_NVMCON_W2);
        pic24_icsp_six(MOV_W2_VISI);
        pic24_icsp_six(NOP);
        uint16_t nvmcon = pic24_icsp_regout();
        pic24_icsp_six(NOP);
        if (!(nvmcon & NVMCON_WR)) {
            return (nvmcon & NVMCON_WRERR) ? PIC24_ICSP_ERR_WRERR : PIC24_ICSP_OK;
        }
        if (time_reached(deadline)) {
            return PIC24_ICSP_ERR_TIMEOUT;
        }
    }
}

static pic24_icsp_status_t start_write(uint32_t timeout_ms) {
    pic24_icsp_six(BSET_NVMCON_WR);
    pic24_icsp_six(NOP);
    pic24_icsp_six(NOP);
    pic24_icsp_status_t status = wait_write_done(timeout_ms);
    park_pc();
    return status;
}

pic24_icsp_status_t pic24_icsp_erase_user(uint32_t timeout_ms) {
    park_pc();
    pic24_icsp_six(MOV_LIT_W(NVMCON_ERASE, 10));
    pic24_icsp_six(MOV_W10_NVMCON);
    // The dummy table write selects what is erased: TBLPAG below 0x80 keeps executive memory
    table_pointer(0, 0);
    pic24_icsp_six(TBLWTL_W0_IND_W0);
    pic24_icsp_six(NOP);
    pic24_icsp_six(NOP);
    return start_write(timeout_ms);
}

pic24_icsp_status_t pic24_icsp_write_row(uint32_t address, const uint32_t* words, uint32_t timeout_ms) {
    static const uint32_t load_latches[] = {
        TBLWTL_W6PP_W7, TBLWTHB_W6PP_W7PP, TBLWTHB_W6PP_PPW7, TBLWTL_W6PP_W7PP,
        TBLWTL_W6PP_W7, TBLWTHB_W6PP_W7PP, TBLWTHB_W6PP_PPW7, TBLWTL_W6PP_W7PP,
    };
    park_pc();
    pic24_icsp_six(MOV_LIT_W(NVMCON_WRITE_ROW, 10));
    pic24_icsp_six(MOV_W10_NVMCON);
    table_pointer(address, 7);
    for (uint32_t i = 0; i < PIC24_ICSP_ROW_WORDS; i += 4) {
        const uint32_t* w = &words[i];
        // Four instructions packed in W0-W5 (Figure 3-6): the upper bytes share W1 and W4
        pic24_icsp_six(MOV_LIT_W(w[0], 0));
        pic24_icsp_six(MOV_LIT_W(((w[1] >> 8) & 0xFF00u) | ((w[0] >> 16) & 0xFFu), 1));
        pic24_icsp_six(MOV_LIT_W(w[1], 2));
        pic24_icsp_six(MOV_LIT_W(w[2], 3));
        pic24_icsp_six(MOV_LIT_W(((w[3] >> 8) & 0xFF00u) | ((w[2] >> 16) & 0xFFu), 4));
        pic24_icsp_six(MOV_LIT_W(w[3], 5));
        pic24_icsp_six(CLR_W6);
        pic24_icsp_six(NOP);
        for (uint8_t k = 0; k < count_of(load_latches); k++) {
            pic24_icsp_six(load_latches[k]);
            pic24_icsp_six(NOP);
            pic24_icsp_six(NOP);
        }
    }
    return start_write(timeout_ms);
}
