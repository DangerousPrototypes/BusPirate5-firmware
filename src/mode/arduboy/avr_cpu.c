/*
 * avr_cpu.c — ATmega32U4 AVR interpreter (optimized)
 *
 * Switch-based instruction decoder for fast dispatch on Cortex-M0+.
 * Top-level switch on opcode bits [15:12] eliminates long if-chains.
 * SREG helpers compute the full byte in one pass.
 *
 * Copyright (c) 2026 Bus Pirate project. MIT License.
 */

#include "avr_cpu.h"
#include <string.h>
#include <stdio.h>

/* ── Helpers ──────────────────────────────────────────────────── */

static inline bool is_two_word_opcode(uint16_t op) {
    if ((op & 0xFE0E) == 0x940C) return true;  /* JMP */
    if ((op & 0xFE0E) == 0x940E) return true;  /* CALL */
    if ((op & 0xFE0F) == 0x9000) return true;  /* LDS */
    if ((op & 0xFE0F) == 0x9200) return true;  /* STS */
    return false;
}

static inline uint16_t reg_pair(avr_cpu_t* cpu, uint8_t lo) {
    return (uint16_t)cpu->r[lo] | ((uint16_t)cpu->r[lo + 1] << 8);
}
static inline void reg_pair_set(avr_cpu_t* cpu, uint8_t lo, uint16_t val) {
    cpu->r[lo] = val & 0xFF;
    cpu->r[lo + 1] = (val >> 8) & 0xFF;
}

/* Stack — direct SRAM access (SP always points into SRAM, no I/O hooks) */
static inline void push_byte(avr_cpu_t* cpu, uint8_t val) {
    cpu->sram[cpu->sp] = val;
    cpu->sp--;
}
static inline uint8_t pop_byte(avr_cpu_t* cpu) {
    cpu->sp++;
    return cpu->sram[cpu->sp];
}

/* ── Optimized SREG helpers ──────────────────────────────────── */

/* Preserve only I (bit 7) and T (bit 6) across arithmetic updates.
 * Computes all 6 flags and writes SREG once, instead of 6× read-modify-write. */
#define SREG_KEEP_IT  ((1 << SREG_I) | (1 << SREG_T))

static void update_sreg_arith(avr_cpu_t* cpu, uint8_t rd_val, uint8_t rr_val, uint8_t result) {
    bool R7 = (result >> 7) & 1, Rd7 = (rd_val >> 7) & 1, Rr7 = (rr_val >> 7) & 1;
    bool R3 = (result >> 3) & 1, Rd3 = (rd_val >> 3) & 1, Rr3 = (rr_val >> 3) & 1;
    bool V = (Rd7 && Rr7 && !R7) || (!Rd7 && !Rr7 && R7);

    uint8_t s = cpu->sreg & SREG_KEEP_IT;
    if (result == 0)                                        s |= (1 << SREG_Z);
    if (R7)                                                 s |= (1 << SREG_N);
    if (V)                                                  s |= (1 << SREG_V);
    if (R7 ^ V)                                             s |= (1 << SREG_S);
    if ((Rd7 && Rr7) || (Rr7 && !R7) || (!R7 && Rd7))     s |= (1 << SREG_C);
    if ((Rd3 && Rr3) || (Rr3 && !R3) || (!R3 && Rd3))     s |= (1 << SREG_H);
    cpu->sreg = s;
}

static void update_sreg_sub(avr_cpu_t* cpu, uint8_t rd_val, uint8_t rr_val, uint8_t result) {
    bool R7 = (result >> 7) & 1, Rd7 = (rd_val >> 7) & 1, Rr7 = (rr_val >> 7) & 1;
    bool R3 = (result >> 3) & 1, Rd3 = (rd_val >> 3) & 1, Rr3 = (rr_val >> 3) & 1;
    bool V = (Rd7 && !Rr7 && !R7) || (!Rd7 && Rr7 && R7);

    uint8_t s = cpu->sreg & SREG_KEEP_IT;
    if (result == 0)                                        s |= (1 << SREG_Z);
    if (R7)                                                 s |= (1 << SREG_N);
    if (V)                                                  s |= (1 << SREG_V);
    if (R7 ^ V)                                             s |= (1 << SREG_S);
    if ((!Rd7 && Rr7) || (Rr7 && R7) || (R7 && !Rd7))     s |= (1 << SREG_C);
    if ((!Rd3 && Rr3) || (Rr3 && R3) || (R3 && !Rd3))     s |= (1 << SREG_H);
    cpu->sreg = s;
}

static void update_sreg_logical(avr_cpu_t* cpu, uint8_t result) {
    /* Preserve I, T, H, C — only update Z, N, V(=0), S(=N) */
    uint8_t s = cpu->sreg & (SREG_KEEP_IT | (1 << SREG_H) | (1 << SREG_C));
    if (result == 0)    s |= (1 << SREG_Z);
    if (result & 0x80)  s |= (1 << SREG_N) | (1 << SREG_S);
    cpu->sreg = s;
}

/* ── Init / Reset ─────────────────────────────────────────────── */

void avr_cpu_init(avr_cpu_t* cpu, uint8_t* flash, uint8_t* sram, uint8_t* eeprom) {
    memset(cpu, 0, sizeof(*cpu));
    cpu->flash = flash;
    cpu->sram = sram;
    cpu->eeprom = eeprom;
    avr_cpu_reset(cpu);
}

void avr_cpu_reset(avr_cpu_t* cpu) {
    memset(cpu->r, 0, 32);
    memset(cpu->sram, 0, AVR_RAMEND + 1);
    cpu->pc = 0;
    cpu->sp = AVR_RAMEND;
    cpu->sreg = 0;
    cpu->cycle_count = 0;
    cpu->halted = false;
    cpu->sleeping = false;
}

void avr_cpu_set_io_hooks(avr_cpu_t* cpu, avr_io_read_fn read, avr_io_write_fn write, void* ctx) {
    cpu->io_read = read;
    cpu->io_write = write;
    cpu->io_ctx = ctx;
}

/* ── Instruction Decoder (switch-based) ───────────────────────── */

uint8_t avr_cpu_step(avr_cpu_t* cpu) {
    if (cpu->halted) return 0;
    if (cpu->sleeping) return 1;

    uint16_t opcode = avr_flash_read_word(cpu, cpu->pc);

    /* Pre-extract common fields */
    uint8_t Rd5 = (opcode >> 4) & 0x1F;
    uint8_t Rr5 = ((opcode >> 5) & 0x10) | (opcode & 0x0F);
    uint8_t d16 = 16 + ((opcode >> 4) & 0x0F);
    uint8_t K8  = ((opcode >> 4) & 0xF0) | (opcode & 0x0F);

    switch (opcode >> 12) {

    /* ═══════════════════════════════════════════════════════════
     * 0x0xxx — NOP, MOVW, MULS, CPC, SBC, ADD
     * ═══════════════════════════════════════════════════════════ */
    case 0x0: {
        if (opcode == 0x0000) { cpu->pc++; return 1; } /* NOP */
        switch ((opcode >> 10) & 0x3) {
        case 0: /* 0000 00xx — MOVW, MULS */
            if ((opcode & 0xFF00) == 0x0100) { /* MOVW */
                uint8_t d = ((opcode >> 4) & 0x0F) * 2;
                uint8_t r = (opcode & 0x0F) * 2;
                cpu->r[d] = cpu->r[r];
                cpu->r[d + 1] = cpu->r[r + 1];
                cpu->pc++; return 1;
            }
            if ((opcode & 0xFF00) == 0x0200) { /* MULS */
                int8_t d_val = (int8_t)cpu->r[16 + ((opcode >> 4) & 0xF)];
                int8_t r_val = (int8_t)cpu->r[16 + (opcode & 0xF)];
                int16_t res = (int16_t)d_val * (int16_t)r_val;
                cpu->r[0] = res & 0xFF;
                cpu->r[1] = (res >> 8) & 0xFF;
                avr_sreg_set(cpu, SREG_C, (res >> 15) & 1);
                avr_sreg_set(cpu, SREG_Z, res == 0);
                cpu->pc++; return 2;
            }
            break;
        case 1: { /* CPC: 0000 01rd dddd rrrr */
            uint8_t rd = cpu->r[Rd5], rr = cpu->r[Rr5];
            uint8_t c = (cpu->sreg >> SREG_C) & 1;
            uint8_t result = rd - rr - c;
            bool prev_z = (cpu->sreg >> SREG_Z) & 1;
            update_sreg_sub(cpu, rd, rr, result);
            if (result == 0) { if (prev_z) cpu->sreg |= (1<<SREG_Z); }
            cpu->pc++; return 1;
        }
        case 2: { /* SBC: 0000 10rd dddd rrrr */
            uint8_t rd = cpu->r[Rd5], rr = cpu->r[Rr5];
            uint8_t c = (cpu->sreg >> SREG_C) & 1;
            uint8_t result = rd - rr - c;
            bool prev_z = (cpu->sreg >> SREG_Z) & 1;
            update_sreg_sub(cpu, rd, rr, result);
            if (result == 0) { if (prev_z) cpu->sreg |= (1<<SREG_Z); }
            cpu->r[Rd5] = result;
            cpu->pc++; return 1;
        }
        case 3: { /* ADD: 0000 11rd dddd rrrr */
            uint8_t rd = cpu->r[Rd5], rr = cpu->r[Rr5];
            uint8_t result = rd + rr;
            update_sreg_arith(cpu, rd, rr, result);
            cpu->r[Rd5] = result;
            cpu->pc++; return 1;
        }
        }
        break;
    }

    /* ═══════════════════════════════════════════════════════════
     * 0x1xxx — CPSE, CP, SUB, ADC
     * ═══════════════════════════════════════════════════════════ */
    case 0x1: {
        switch ((opcode >> 10) & 0x3) {
        case 0: { /* CPSE: 0001 00rd dddd rrrr */
            cpu->pc++;
            if (cpu->r[Rd5] == cpu->r[Rr5]) {
                uint16_t next = avr_flash_read_word(cpu, cpu->pc);
                cpu->pc++;
                if (is_two_word_opcode(next)) { cpu->pc++; return 3; }
                return 2;
            }
            return 1;
        }
        case 1: { /* CP: 0001 01rd dddd rrrr */
            uint8_t rd = cpu->r[Rd5], rr = cpu->r[Rr5];
            update_sreg_sub(cpu, rd, rr, rd - rr);
            cpu->pc++; return 1;
        }
        case 2: { /* SUB: 0001 10rd dddd rrrr */
            uint8_t rd = cpu->r[Rd5], rr = cpu->r[Rr5];
            uint8_t result = rd - rr;
            update_sreg_sub(cpu, rd, rr, result);
            cpu->r[Rd5] = result;
            cpu->pc++; return 1;
        }
        case 3: { /* ADC: 0001 11rd dddd rrrr */
            uint8_t rd = cpu->r[Rd5], rr = cpu->r[Rr5];
            uint8_t c = (cpu->sreg >> SREG_C) & 1;
            uint8_t result = rd + rr + c;
            update_sreg_arith(cpu, rd, rr, result);
            cpu->r[Rd5] = result;
            cpu->pc++; return 1;
        }
        }
        break;
    }

    /* ═══════════════════════════════════════════════════════════
     * 0x2xxx — AND, EOR, OR, MOV
     * ═══════════════════════════════════════════════════════════ */
    case 0x2: {
        switch ((opcode >> 10) & 0x3) {
        case 0: { /* AND */
            uint8_t result = cpu->r[Rd5] & cpu->r[Rr5];
            update_sreg_logical(cpu, result);
            cpu->r[Rd5] = result;
            cpu->pc++; return 1;
        }
        case 1: { /* EOR */
            uint8_t result = cpu->r[Rd5] ^ cpu->r[Rr5];
            update_sreg_logical(cpu, result);
            cpu->r[Rd5] = result;
            cpu->pc++; return 1;
        }
        case 2: { /* OR */
            uint8_t result = cpu->r[Rd5] | cpu->r[Rr5];
            update_sreg_logical(cpu, result);
            cpu->r[Rd5] = result;
            cpu->pc++; return 1;
        }
        case 3: /* MOV */
            cpu->r[Rd5] = cpu->r[Rr5];
            cpu->pc++; return 1;
        }
        break;
    }

    /* ═══════════════════════════════════════════════════════════
     * 0x3xxx — CPI
     * ═══════════════════════════════════════════════════════════ */
    case 0x3: {
        uint8_t rd = cpu->r[d16];
        update_sreg_sub(cpu, rd, K8, rd - K8);
        cpu->pc++; return 1;
    }

    /* ═══════════════════════════════════════════════════════════
     * 0x4xxx — SBCI
     * ═══════════════════════════════════════════════════════════ */
    case 0x4: {
        uint8_t rd = cpu->r[d16];
        uint8_t c = (cpu->sreg >> SREG_C) & 1;
        uint8_t result = rd - K8 - c;
        bool prev_z = (cpu->sreg >> SREG_Z) & 1;
        update_sreg_sub(cpu, rd, K8, result);
        if (result == 0) { if (prev_z) cpu->sreg |= (1<<SREG_Z); }
        cpu->r[d16] = result;
        cpu->pc++; return 1;
    }

    /* ═══════════════════════════════════════════════════════════
     * 0x5xxx — SUBI
     * ═══════════════════════════════════════════════════════════ */
    case 0x5: {
        uint8_t rd = cpu->r[d16];
        uint8_t result = rd - K8;
        update_sreg_sub(cpu, rd, K8, result);
        cpu->r[d16] = result;
        cpu->pc++; return 1;
    }

    /* ═══════════════════════════════════════════════════════════
     * 0x6xxx — ORI / SBR
     * ═══════════════════════════════════════════════════════════ */
    case 0x6: {
        uint8_t result = cpu->r[d16] | K8;
        update_sreg_logical(cpu, result);
        cpu->r[d16] = result;
        cpu->pc++; return 1;
    }

    /* ═══════════════════════════════════════════════════════════
     * 0x7xxx — ANDI / CBR
     * ═══════════════════════════════════════════════════════════ */
    case 0x7: {
        uint8_t result = cpu->r[d16] & K8;
        update_sreg_logical(cpu, result);
        cpu->r[d16] = result;
        cpu->pc++; return 1;
    }

    /* ═══════════════════════════════════════════════════════════
     * 0x8xxx / 0xAxxx — LDD / STD with Z+q or Y+q
     * ═══════════════════════════════════════════════════════════ */
    case 0x8:
    case 0xA: {
        uint8_t q = ((opcode >> 8) & 0x20) | ((opcode >> 7) & 0x18) | (opcode & 0x07);
        bool is_y = (opcode >> 3) & 1;
        uint16_t base = is_y ? reg_pair(cpu, 28) : reg_pair(cpu, 30);
        uint16_t addr = base + q;
        if (opcode & 0x0200) {
            avr_data_write(cpu, addr, cpu->r[Rd5]); /* STD */
        } else {
            cpu->r[Rd5] = avr_data_read(cpu, addr); /* LDD */
        }
        cpu->pc++; return 2;
    }

    /* ═══════════════════════════════════════════════════════════
     * 0x9xxx — Big bucket: LD/ST/PUSH/POP/LDS/STS, single-reg,
     *          JMP/CALL, ADIW/SBIW, CBI/SBI/SBIC/SBIS, MUL
     * ═══════════════════════════════════════════════════════════ */
    case 0x9: {
        switch ((opcode >> 9) & 0x7) {

        /* ─── 0x9000–0x91FF: LD variants, LPM, POP, LDS ─── */
        case 0:
            switch (opcode & 0x000F) {
            case 0x0: { /* LDS Rd, k (2-word) */
                uint16_t addr = avr_flash_read_word(cpu, cpu->pc + 1);
                cpu->r[Rd5] = avr_data_read(cpu, addr);
                cpu->pc += 2; return 2;
            }
            case 0x1: { /* LD Rd, Z+ */
                uint16_t z = reg_pair(cpu, 30);
                cpu->r[Rd5] = avr_data_read(cpu, z);
                reg_pair_set(cpu, 30, z + 1);
                cpu->pc++; return 2;
            }
            case 0x2: { /* LD Rd, -Z */
                uint16_t z = reg_pair(cpu, 30) - 1;
                reg_pair_set(cpu, 30, z);
                cpu->r[Rd5] = avr_data_read(cpu, z);
                cpu->pc++; return 2;
            }
            case 0x4: { /* LPM Rd, Z */
                uint16_t z = reg_pair(cpu, 30);
                uint8_t val = (z < AVR_FLASH_SIZE) ? cpu->flash[z] : 0;
                cpu->r[Rd5] = val;
                cpu->dbg_lpm_count++;
                if (val) cpu->dbg_lpm_nonzero++;
                cpu->pc++; return 3;
            }
            case 0x5: { /* LPM Rd, Z+ */
                uint16_t z = reg_pair(cpu, 30);
                uint8_t val = (z < AVR_FLASH_SIZE) ? cpu->flash[z] : 0;
                cpu->r[Rd5] = val;
                cpu->dbg_lpm_count++;
                if (val) cpu->dbg_lpm_nonzero++;
                reg_pair_set(cpu, 30, z + 1);
                cpu->pc++; return 3;
            }
            case 0x9: { /* LD Rd, Y+ */
                uint16_t y = reg_pair(cpu, 28);
                cpu->r[Rd5] = avr_data_read(cpu, y);
                reg_pair_set(cpu, 28, y + 1);
                cpu->pc++; return 2;
            }
            case 0xA: { /* LD Rd, -Y */
                uint16_t y = reg_pair(cpu, 28) - 1;
                reg_pair_set(cpu, 28, y);
                cpu->r[Rd5] = avr_data_read(cpu, y);
                cpu->pc++; return 2;
            }
            case 0xC: { /* LD Rd, X */
                uint16_t x = reg_pair(cpu, 26);
                cpu->r[Rd5] = avr_data_read(cpu, x);
                cpu->pc++; return 2;
            }
            case 0xD: { /* LD Rd, X+ */
                uint16_t x = reg_pair(cpu, 26);
                cpu->r[Rd5] = avr_data_read(cpu, x);
                reg_pair_set(cpu, 26, x + 1);
                cpu->pc++; return 2;
            }
            case 0xE: { /* LD Rd, -X */
                uint16_t x = reg_pair(cpu, 26) - 1;
                reg_pair_set(cpu, 26, x);
                cpu->r[Rd5] = avr_data_read(cpu, x);
                cpu->pc++; return 2;
            }
            case 0xF: /* POP Rd */
                cpu->r[Rd5] = pop_byte(cpu);
                cpu->pc++; return 2;
            default: break;
            }
            break;

        /* ─── 0x9200–0x93FF: ST variants, PUSH, STS ─── */
        case 1:
            switch (opcode & 0x000F) {
            case 0x0: { /* STS k, Rd (2-word) */
                uint16_t addr = avr_flash_read_word(cpu, cpu->pc + 1);
                avr_data_write(cpu, addr, cpu->r[Rd5]);
                cpu->pc += 2; return 2;
            }
            case 0x1: { /* ST Z+, Rd */
                uint16_t z = reg_pair(cpu, 30);
                avr_data_write(cpu, z, cpu->r[Rd5]);
                reg_pair_set(cpu, 30, z + 1);
                cpu->pc++; return 2;
            }
            case 0x2: { /* ST -Z, Rd */
                uint16_t z = reg_pair(cpu, 30) - 1;
                reg_pair_set(cpu, 30, z);
                avr_data_write(cpu, z, cpu->r[Rd5]);
                cpu->pc++; return 2;
            }
            case 0x9: { /* ST Y+, Rd */
                uint16_t y = reg_pair(cpu, 28);
                avr_data_write(cpu, y, cpu->r[Rd5]);
                reg_pair_set(cpu, 28, y + 1);
                cpu->pc++; return 2;
            }
            case 0xA: { /* ST -Y, Rd */
                uint16_t y = reg_pair(cpu, 28) - 1;
                reg_pair_set(cpu, 28, y);
                avr_data_write(cpu, y, cpu->r[Rd5]);
                cpu->pc++; return 2;
            }
            case 0xC: { /* ST X, Rd */
                uint16_t x = reg_pair(cpu, 26);
                avr_data_write(cpu, x, cpu->r[Rd5]);
                cpu->pc++; return 2;
            }
            case 0xD: { /* ST X+, Rd */
                uint16_t x = reg_pair(cpu, 26);
                avr_data_write(cpu, x, cpu->r[Rd5]);
                reg_pair_set(cpu, 26, x + 1);
                cpu->pc++; return 2;
            }
            case 0xE: { /* ST -X, Rd */
                uint16_t x = reg_pair(cpu, 26) - 1;
                reg_pair_set(cpu, 26, x);
                avr_data_write(cpu, x, cpu->r[Rd5]);
                cpu->pc++; return 2;
            }
            case 0xF: /* PUSH Rd */
                push_byte(cpu, cpu->r[Rd5]);
                cpu->pc++; return 2;
            default: break;
            }
            break;

        /* ─── 0x9400–0x95FF: Single-reg ops, JMP, CALL, specials ─── */
        case 2:
            switch (opcode & 0x000F) {
            case 0x0: { /* COM Rd */
                uint8_t result = ~cpu->r[Rd5];
                uint8_t s = cpu->sreg & SREG_KEEP_IT;
                s |= (1 << SREG_C); /* C always set */
                if (result == 0)   s |= (1 << SREG_Z);
                if (result & 0x80) s |= (1 << SREG_N) | (1 << SREG_S);
                cpu->sreg = s;
                cpu->r[Rd5] = result;
                cpu->pc++; return 1;
            }
            case 0x1: { /* NEG Rd */
                uint8_t rd = cpu->r[Rd5];
                uint8_t result = 0 - rd;
                uint8_t s = cpu->sreg & SREG_KEEP_IT;
                if (result == 0)    s |= (1 << SREG_Z);
                else                s |= (1 << SREG_C); /* C = result != 0 */
                if (result & 0x80)  s |= (1 << SREG_N);
                if (result == 0x80) s |= (1 << SREG_V);
                if (((result >> 7) & 1) ^ (result == 0x80)) s |= (1 << SREG_S);
                if (((result >> 3) & 1) | ((rd >> 3) & 1))  s |= (1 << SREG_H);
                cpu->sreg = s;
                cpu->r[Rd5] = result;
                cpu->pc++; return 1;
            }
            case 0x2: { /* SWAP Rd */
                uint8_t v = cpu->r[Rd5];
                cpu->r[Rd5] = (v >> 4) | (v << 4);
                cpu->pc++; return 1;
            }
            case 0x3: { /* INC Rd */
                uint8_t old = cpu->r[Rd5];
                uint8_t result = old + 1;
                uint8_t s = cpu->sreg & (SREG_KEEP_IT | (1 << SREG_H) | (1 << SREG_C));
                if (result == 0)    s |= (1 << SREG_Z);
                if (result & 0x80)  s |= (1 << SREG_N);
                if (old == 0x7F)    s |= (1 << SREG_V);
                if (((result >> 7) & 1) ^ (old == 0x7F)) s |= (1 << SREG_S);
                cpu->sreg = s;
                cpu->r[Rd5] = result;
                cpu->pc++; return 1;
            }
            case 0x5: { /* ASR Rd */
                uint8_t rd = cpu->r[Rd5];
                uint8_t result = (rd >> 1) | (rd & 0x80);
                uint8_t s = cpu->sreg & SREG_KEEP_IT;
                if (rd & 1)         s |= (1 << SREG_C);
                if (result == 0)    s |= (1 << SREG_Z);
                if (result & 0x80)  s |= (1 << SREG_N);
                bool V = ((result >> 7) ^ rd) & 1;
                if (V)              s |= (1 << SREG_V);
                if (((result >> 7) & 1) ^ V) s |= (1 << SREG_S);
                cpu->sreg = s;
                cpu->r[Rd5] = result;
                cpu->pc++; return 1;
            }
            case 0x6: { /* LSR Rd */
                uint8_t rd = cpu->r[Rd5];
                uint8_t result = rd >> 1;
                uint8_t s = cpu->sreg & SREG_KEEP_IT;
                if (rd & 1)        s |= (1 << SREG_C) | (1 << SREG_V) | (1 << SREG_S);
                if (result == 0)   s |= (1 << SREG_Z);
                /* N is always 0 for LSR */
                cpu->sreg = s;
                cpu->r[Rd5] = result;
                cpu->pc++; return 1;
            }
            case 0x7: { /* ROR Rd */
                uint8_t rd = cpu->r[Rd5];
                uint8_t old_c = (cpu->sreg >> SREG_C) & 1;
                uint8_t result = (rd >> 1) | (old_c ? 0x80 : 0);
                uint8_t s = cpu->sreg & SREG_KEEP_IT;
                if (rd & 1)         s |= (1 << SREG_C);
                if (result == 0)    s |= (1 << SREG_Z);
                if (result & 0x80)  s |= (1 << SREG_N);
                bool V = ((result >> 7) ^ (rd & 1)) & 1;
                if (V)              s |= (1 << SREG_V);
                if (((result >> 7) & 1) ^ V) s |= (1 << SREG_S);
                cpu->sreg = s;
                cpu->r[Rd5] = result;
                cpu->pc++; return 1;
            }
            case 0x8: {
                /* Many special opcodes share bottom nibble 0x8 */
                if ((opcode & 0xFF8F) == 0x9408) { /* BSET s */
                    cpu->sreg |= (1 << ((opcode >> 4) & 0x07));
                    cpu->pc++; return 1;
                }
                if ((opcode & 0xFF8F) == 0x9488) { /* BCLR s */
                    cpu->sreg &= ~(1 << ((opcode >> 4) & 0x07));
                    cpu->pc++; return 1;
                }
                if (opcode == 0x9508) { /* RET */
                    uint8_t hi = pop_byte(cpu);
                    uint8_t lo = pop_byte(cpu);
                    cpu->pc = ((uint16_t)hi << 8) | lo;
                    return 4;
                }
                if (opcode == 0x9518) { /* RETI */
                    uint8_t hi = pop_byte(cpu);
                    uint8_t lo = pop_byte(cpu);
                    cpu->pc = ((uint16_t)hi << 8) | lo;
                    cpu->sreg |= (1 << SREG_I);
                    return 4;
                }
                if (opcode == 0x9588) { /* SLEEP */
                    cpu->sleeping = true;
                    cpu->pc++; return 1;
                }
                if (opcode == 0x95A8) { cpu->pc++; return 1; } /* WDR */
                if (opcode == 0x9598) { cpu->halted = true; return 1; } /* BREAK */
                if (opcode == 0x95E8) { cpu->pc++; return 1; } /* SPM */
                if (opcode == 0x95C8) { /* LPM R0, Z (bare) */
                    uint16_t z = reg_pair(cpu, 30);
                    uint8_t val = (z < AVR_FLASH_SIZE) ? cpu->flash[z] : 0;
                    cpu->r[0] = val;
                    cpu->dbg_lpm_count++;
                    if (val) cpu->dbg_lpm_nonzero++;
                    cpu->pc++; return 3;
                }
                break;
            }
            case 0x9: {
                if (opcode == 0x9509) { /* ICALL */
                    uint16_t ret = cpu->pc + 1;
                    push_byte(cpu, ret & 0xFF);
                    push_byte(cpu, (ret >> 8) & 0xFF);
                    cpu->pc = reg_pair(cpu, 30);
                    return 3;
                }
                if (opcode == 0x9409) { /* IJMP */
                    cpu->pc = reg_pair(cpu, 30);
                    return 2;
                }
                break;
            }
            case 0xA: { /* DEC Rd */
                uint8_t old = cpu->r[Rd5];
                uint8_t result = old - 1;
                uint8_t s = cpu->sreg & (SREG_KEEP_IT | (1 << SREG_H) | (1 << SREG_C));
                if (result == 0)    s |= (1 << SREG_Z);
                if (result & 0x80)  s |= (1 << SREG_N);
                if (old == 0x80)    s |= (1 << SREG_V);
                if (((result >> 7) & 1) ^ (old == 0x80)) s |= (1 << SREG_S);
                cpu->sreg = s;
                cpu->r[Rd5] = result;
                cpu->pc++; return 1;
            }
            case 0xC:
            case 0xD: { /* JMP k (2-word) */
                uint32_t k = ((uint32_t)(opcode & 0x01F0) << 13) |
                             ((uint32_t)(opcode & 0x0001) << 16) |
                             avr_flash_read_word(cpu, cpu->pc + 1);
                cpu->pc = k;
                return 3;
            }
            case 0xE:
            case 0xF: { /* CALL k (2-word) */
                uint32_t k = ((uint32_t)(opcode & 0x01F0) << 13) |
                             ((uint32_t)(opcode & 0x0001) << 16) |
                             avr_flash_read_word(cpu, cpu->pc + 1);
                uint16_t ret = cpu->pc + 2;
                push_byte(cpu, ret & 0xFF);
                push_byte(cpu, (ret >> 8) & 0xFF);
                cpu->pc = k;
                return 4;
            }
            default: break;
            }
            break;

        /* ─── 0x9600–0x97FF: ADIW / SBIW ─── */
        case 3: {
            uint8_t pair = 24 + (((opcode >> 4) & 0x03) * 2);
            uint8_t K = ((opcode >> 2) & 0x30) | (opcode & 0x0F);
            uint16_t val = reg_pair(cpu, pair);

            if (!(opcode & 0x0100)) { /* ADIW: 1001 0110 */
                uint16_t result = val + K;
                uint8_t s = cpu->sreg & SREG_KEEP_IT;
                if (result == 0)                              s |= (1 << SREG_Z);
                if (result & 0x8000)                          s |= (1 << SREG_N);
                if (!(val & 0x8000) && (result & 0x8000))     s |= (1 << SREG_V);
                if (!(result & 0x8000) && (val & 0x8000))     s |= (1 << SREG_C);
                bool N = (result >> 15) & 1;
                bool V2 = !(val & 0x8000) && (result & 0x8000);
                if (N ^ V2)                                   s |= (1 << SREG_S);
                cpu->sreg = s;
                reg_pair_set(cpu, pair, result);
            } else { /* SBIW: 1001 0111 */
                uint16_t result = val - K;
                uint8_t s = cpu->sreg & SREG_KEEP_IT;
                if (result == 0)                              s |= (1 << SREG_Z);
                if (result & 0x8000)                          s |= (1 << SREG_N);
                if ((val & 0x8000) && !(result & 0x8000))     s |= (1 << SREG_V);
                if ((result & 0x8000) && !(val & 0x8000))     s |= (1 << SREG_C);
                bool N = (result >> 15) & 1;
                bool V2 = (val & 0x8000) && !(result & 0x8000);
                if (N ^ V2)                                   s |= (1 << SREG_S);
                cpu->sreg = s;
                reg_pair_set(cpu, pair, result);
            }
            cpu->pc++; return 2;
        }

        /* ─── 0x9800–0x9BFF: CBI, SBIC, SBI, SBIS ─── */
        case 4:
        case 5: {
            uint8_t A = (opcode >> 3) & 0x1F;
            uint8_t b = opcode & 0x07;
            uint8_t val = avr_data_read(cpu, A + AVR_IO_BASE);
            uint8_t sub_op = (opcode >> 8) & 0x03;

            switch (sub_op) {
            case 0: /* CBI */
                avr_data_write(cpu, A + AVR_IO_BASE, val & ~(1 << b));
                cpu->pc++; return 2;
            case 1: { /* SBIC — skip if bit cleared */
                cpu->pc++;
                if (!(val & (1 << b))) {
                    uint16_t next = avr_flash_read_word(cpu, cpu->pc);
                    cpu->pc++;
                    if (is_two_word_opcode(next)) { cpu->pc++; return 3; }
                    return 2;
                }
                return 1;
            }
            case 2: /* SBI */
                avr_data_write(cpu, A + AVR_IO_BASE, val | (1 << b));
                cpu->pc++; return 2;
            case 3: { /* SBIS — skip if bit set */
                cpu->pc++;
                if (val & (1 << b)) {
                    uint16_t next = avr_flash_read_word(cpu, cpu->pc);
                    cpu->pc++;
                    if (is_two_word_opcode(next)) { cpu->pc++; return 3; }
                    return 2;
                }
                return 1;
            }
            }
            break;
        }

        /* ─── 0x9C00–0x9FFF: MUL (unsigned) ─── */
        case 6:
        case 7: {
            uint16_t result = (uint16_t)cpu->r[Rd5] * (uint16_t)cpu->r[Rr5];
            cpu->r[0] = result & 0xFF;
            cpu->r[1] = (result >> 8) & 0xFF;
            avr_sreg_set(cpu, SREG_C, (result >> 15) & 1);
            avr_sreg_set(cpu, SREG_Z, result == 0);
            cpu->pc++; return 2;
        }

        } /* end 0x9xxx sub-switch */
        break;
    }

    /* ═══════════════════════════════════════════════════════════
     * 0xBxxx — IN / OUT
     * ═══════════════════════════════════════════════════════════ */
    case 0xB: {
        uint8_t A = ((opcode >> 5) & 0x30) | (opcode & 0x0F);
        if (opcode & 0x0800) {
            avr_data_write(cpu, A + AVR_IO_BASE, cpu->r[Rd5]); /* OUT */
        } else {
            cpu->r[Rd5] = avr_data_read(cpu, A + AVR_IO_BASE); /* IN */
        }
        cpu->pc++; return 1;
    }

    /* ═══════════════════════════════════════════════════════════
     * 0xCxxx — RJMP
     * ═══════════════════════════════════════════════════════════ */
    case 0xC: {
        int16_t k = opcode & 0x0FFF;
        if (k & 0x0800) k |= 0xF000;
        cpu->pc += k + 1;
        return 2;
    }

    /* ═══════════════════════════════════════════════════════════
     * 0xDxxx — RCALL
     * ═══════════════════════════════════════════════════════════ */
    case 0xD: {
        int16_t k = opcode & 0x0FFF;
        if (k & 0x0800) k |= 0xF000;
        uint16_t ret = cpu->pc + 1;
        push_byte(cpu, ret & 0xFF);
        push_byte(cpu, (ret >> 8) & 0xFF);
        cpu->pc += k + 1;
        return 3;
    }

    /* ═══════════════════════════════════════════════════════════
     * 0xExxx — LDI
     * ═══════════════════════════════════════════════════════════ */
    case 0xE:
        cpu->r[d16] = K8;
        cpu->pc++; return 1;

    /* ═══════════════════════════════════════════════════════════
     * 0xFxxx — Branches (BRBS/BRBC), SBRC/SBRS, BLD, BST
     * ═══════════════════════════════════════════════════════════ */
    case 0xF: {
        uint8_t bit = opcode & 0x07;

        if ((opcode & 0x0C00) == 0x0C00) {
            /* SBRC/SBRS: 1111 11Xd dddd 0bbb */
            bool skip_if_set = (opcode & 0x0200) != 0;
            bool bit_val = (cpu->r[Rd5] >> bit) & 1;
            cpu->pc++;
            if (bit_val == skip_if_set) {
                uint16_t next = avr_flash_read_word(cpu, cpu->pc);
                cpu->pc++;
                if (is_two_word_opcode(next)) { cpu->pc++; return 3; }
                return 2;
            }
            return 1;
        }
        if ((opcode & 0xFE08) == 0xF800) { /* BLD Rd, b */
            if ((cpu->sreg >> SREG_T) & 1)
                cpu->r[Rd5] |= (1 << bit);
            else
                cpu->r[Rd5] &= ~(1 << bit);
            cpu->pc++; return 1;
        }
        if ((opcode & 0xFE08) == 0xFA00) { /* BST Rd, b */
            avr_sreg_set(cpu, SREG_T, (cpu->r[Rd5] >> bit) & 1);
            cpu->pc++; return 1;
        }
        /* BRBS/BRBC: 1111 0Xkk kkkk ksss */
        if (!(opcode & 0x0800)) {
            int8_t k = (int8_t)((opcode >> 3) & 0x7F);
            if (k & 0x40) k |= 0x80;
            bool flag = (cpu->sreg >> bit) & 1;
            bool branch = (opcode & 0x0400) ? !flag : flag;
            if (branch) {
                cpu->pc += k + 1;
                return 2;
            }
            cpu->pc++; return 1;
        }
        break;
    }

    } /* end top-level switch */

    /* Unrecognized opcode */
    cpu->dbg_unrecognized++;
    cpu->dbg_last_bad_op = opcode;
    cpu->pc++;
    return 1;
}

/* ── Batch execution ──────────────────────────────────────────── */

uint32_t avr_cpu_run(avr_cpu_t* cpu, uint32_t max_cycles) {
    uint32_t total = 0;
    while (total < max_cycles && !cpu->halted) {
        uint8_t c = avr_cpu_step(cpu);
        cpu->cycle_count += c;
        total += c;
    }
    return total;
}

/* ── Intel HEX loader ─────────────────────────────────────────── */

static uint8_t hex_nibble(char c) {
    if (c >= '0' && c <= '9') return c - '0';
    if (c >= 'A' && c <= 'F') return c - 'A' + 10;
    if (c >= 'a' && c <= 'f') return c - 'a' + 10;
    return 0;
}

static uint8_t hex_byte(const char* p) {
    return (hex_nibble(p[0]) << 4) | hex_nibble(p[1]);
}

bool avr_load_hex(avr_cpu_t* cpu, const char* hex_data, uint32_t hex_len) {
    uint32_t base_addr = 0;
    const char* p = hex_data;
    const char* end = hex_data + hex_len;

    while (p < end) {
        while (p < end && *p != ':') p++;
        if (p >= end) break;
        p++;

        if (p + 8 > end) return false;

        uint8_t byte_count = hex_byte(p); p += 2;
        uint16_t addr = ((uint16_t)hex_byte(p) << 8); p += 2;
        addr |= hex_byte(p); p += 2;
        uint8_t type = hex_byte(p); p += 2;

        if (p + byte_count * 2 > end) return false;

        switch (type) {
        case 0x00:
            for (uint8_t i = 0; i < byte_count; i++) {
                uint32_t flash_addr = base_addr + addr + i;
                if (flash_addr < AVR_FLASH_SIZE) {
                    cpu->flash[flash_addr] = hex_byte(p);
                }
                p += 2;
            }
            break;
        case 0x01:
            return true;
        case 0x02:
            if (byte_count >= 2) {
                base_addr = ((uint32_t)hex_byte(p) << 12) | ((uint32_t)hex_byte(p + 2) << 4);
            }
            p += byte_count * 2;
            break;
        case 0x04:
            if (byte_count >= 2) {
                base_addr = ((uint32_t)hex_byte(p) << 24) | ((uint32_t)hex_byte(p + 2) << 16);
            }
            p += byte_count * 2;
            break;
        default:
            p += byte_count * 2;
            break;
        }

        if (p + 2 <= end) p += 2;
        while (p < end && (*p == '\r' || *p == '\n')) p++;
    }

    return true;
}
