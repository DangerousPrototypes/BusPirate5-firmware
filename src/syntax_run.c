/**
 * @file syntax_run.c
 * @brief Syntax executor - runs compiled bytecode.
 * @details Phase 2 of syntax processing: executes bytecode instructions
 *          and captures results for post-processing.
 */

#include <stdio.h>
#include "pico/stdlib.h"
#include <stdint.h>
#include "pirate.h"
#include "system_config.h"
#include "command_struct.h"
#include "bytecode.h"
#include "modes.h"
#include "ui/ui_const.h"
#include "syntax.h"
#include "syntax_internal.h"
#include "pirate/bio.h"
#include "pirate/amux.h"

// #define SYNTAX_DEBUG

/*
 * =============================================================================
 * Pin labels for aux operations
 * =============================================================================
 */

static const char labels[][5] = { "AUXL", "AUXH" };

/*
 * =============================================================================
 * Run handlers for each bytecode instruction
 * =============================================================================
 */

static void syntax_run_write(struct _syntax_io *io, uint32_t pos) {
    if (io->in_cnt + io->out[pos].repeat >= SYN_MAX_LENGTH) {
        io->in[io->in_cnt].error_message = GET_T(T_SYNTAX_EXCEEDS_MAX_SLOTS);
        io->in[io->in_cnt].error = SERR_ERROR;
        return;
    }
    for (uint32_t j = 0; j < io->out[pos].repeat; j++) {
        if (j > 0) {
            io->in_cnt++;
            io->in[io->in_cnt] = io->out[pos];
        }
        modes[system_config.mode].protocol_write(&io->in[io->in_cnt], NULL);
    }
}

static void syntax_run_read(struct _syntax_io *io, uint32_t pos) {
    if (io->in_cnt + io->out->repeat >= SYN_MAX_LENGTH) {
        io->in[io->in_cnt].error_message = GET_T(T_SYNTAX_EXCEEDS_MAX_SLOTS);
        io->in[io->in_cnt].error = SERR_ERROR;
        return;
    }
#ifdef SYNTAX_DEBUG
    printf("[DEBUG] repeat %d, pos %d, cmd: %d\r\n",
           io->out[pos].repeat, pos, io->out[pos].command);
#endif
    for (uint32_t j = 0; j < io->out[pos].repeat; j++) {
        if (j > 0) {
            io->in_cnt++;
            io->in[io->in_cnt] = io->out[pos];
        }
        modes[system_config.mode].protocol_read(
            &io->in[io->in_cnt],
            ((pos + 1 < io->out_cnt) && (j + 1 == io->out[pos].repeat))
                ? &io->out[pos + 1]
                : NULL);
    }
}

static void syntax_run_start(struct _syntax_io *io, uint32_t pos) {
    modes[system_config.mode].protocol_start(&io->in[io->in_cnt], NULL);
}

static void syntax_run_start_alt(struct _syntax_io *io, uint32_t pos) {
    modes[system_config.mode].protocol_start_alt(&io->in[io->in_cnt], NULL);
}

static void syntax_run_stop(struct _syntax_io *io, uint32_t pos) {
    modes[system_config.mode].protocol_stop(&io->in[io->in_cnt], NULL);
}

static void syntax_run_stop_alt(struct _syntax_io *io, uint32_t pos) {
    modes[system_config.mode].protocol_stop_alt(&io->in[io->in_cnt], NULL);
}

static void syntax_run_delay_us(struct _syntax_io *io, uint32_t pos) {
    busy_wait_us_32(io->out[pos].repeat);
}

static void syntax_run_delay_ms(struct _syntax_io *io, uint32_t pos) {
    busy_wait_ms(io->out[pos].repeat);
}

static inline void _syntax_run_aux_output(uint8_t bio, bool direction) {
    bio_output(bio);
    bio_put(bio, direction);
    system_bio_update_purpose_and_label(true, bio, BP_PIN_IO, labels[direction]);
    system_set_active(true, bio, &system_config.aux_active);
}

static void syntax_run_aux_output_high(struct _syntax_io *io, uint32_t pos) {
    _syntax_run_aux_output(io->out[pos].bits, true);
}

static void syntax_run_aux_output_low(struct _syntax_io *io, uint32_t pos) {
    _syntax_run_aux_output(io->out[pos].bits, false);
}

static void syntax_run_aux_input(struct _syntax_io *io, uint32_t pos) {
    bio_input(io->out[pos].bits);
    io->in[io->in_cnt].in_data = bio_get(io->out[pos].bits);
    system_bio_update_purpose_and_label(false, io->out[pos].bits, BP_PIN_IO, 0);
    system_set_active(false, io->out[pos].bits, &system_config.aux_active);
}

static void syntax_run_adc(struct _syntax_io *io, uint32_t pos) {
    io->in[io->in_cnt].in_data = amux_read_bio(io->out[pos].bits);
}

static void syntax_run_tick_clock(struct _syntax_io *io, uint32_t pos) {
    for (uint32_t j = 0; j < io->out[pos].repeat; j++) {
        modes[system_config.mode].protocol_tick_clock(&io->in[io->in_cnt], NULL);
    }
}

static void syntax_run_set_clk_high(struct _syntax_io *io, uint32_t pos) {
    modes[system_config.mode].protocol_clkh(&io->in[io->in_cnt], NULL);
}

static void syntax_run_set_clk_low(struct _syntax_io *io, uint32_t pos) {
    modes[system_config.mode].protocol_clkl(&io->in[io->in_cnt], NULL);
}

static void syntax_run_set_dat_high(struct _syntax_io *io, uint32_t pos) {
    modes[system_config.mode].protocol_dath(&io->in[io->in_cnt], NULL);
}

static void syntax_run_set_dat_low(struct _syntax_io *io, uint32_t pos) {
    modes[system_config.mode].protocol_datl(&io->in[io->in_cnt], NULL);
}

static void syntax_run_read_dat(struct _syntax_io *io, uint32_t pos) {
    for (uint32_t j = 0; j < io->out[pos].repeat; j++) {
        modes[system_config.mode].protocol_bitr(&io->in[io->in_cnt], NULL);
    }
}

/*
 * =============================================================================
 * Dispatch table
 * =============================================================================
 */

syntax_run_func_ptr_t syntax_run_func[] = {
    [SYN_WRITE]          = syntax_run_write,
    [SYN_READ]           = syntax_run_read,
    [SYN_START]          = syntax_run_start,
    [SYN_START_ALT]      = syntax_run_start_alt,
    [SYN_STOP]           = syntax_run_stop,
    [SYN_STOP_ALT]       = syntax_run_stop_alt,
    [SYN_DELAY_US]       = syntax_run_delay_us,
    [SYN_DELAY_MS]       = syntax_run_delay_ms,
    [SYN_AUX_OUTPUT_HIGH]= syntax_run_aux_output_high,
    [SYN_AUX_OUTPUT_LOW] = syntax_run_aux_output_low,
    [SYN_AUX_INPUT]      = syntax_run_aux_input,
    [SYN_ADC]            = syntax_run_adc,
    [SYN_TICK_CLOCK]     = syntax_run_tick_clock,
    [SYN_SET_CLK_HIGH]   = syntax_run_set_clk_high,
    [SYN_SET_CLK_LOW]    = syntax_run_set_clk_low,
    [SYN_SET_DAT_HIGH]   = syntax_run_set_dat_high,
    [SYN_SET_DAT_LOW]    = syntax_run_set_dat_low,
    [SYN_READ_DAT]       = syntax_run_read_dat
};

/*
 * =============================================================================
 * Main run function
 * =============================================================================
 */

SYNTAX_STATUS syntax_run(void) {
    if (!syntax_io.out_cnt) {
        return SSTATUS_ERROR;
    }

    syntax_io.in_cnt = 0;

    for (uint32_t pos = 0; pos < syntax_io.out_cnt; pos++) {
        syntax_io.in[syntax_io.in_cnt] = syntax_io.out[pos];

        if (syntax_io.out[pos].command >= count_of(syntax_run_func)) {
            printf("Unknown internal code %d\r\n", syntax_io.out[pos].command);
            return SSTATUS_ERROR;
        }

        syntax_run_func[syntax_io.out[pos].command](&syntax_io, pos);

        if (syntax_io.in_cnt + 1 >= SYN_MAX_LENGTH) {
            syntax_io.in[syntax_io.in_cnt].error_message = GET_T(T_SYNTAX_EXCEEDS_MAX_SLOTS);
            syntax_io.in[syntax_io.in_cnt].error = SERR_ERROR;
            return SSTATUS_OK; // Let post-process show the error
        }

        if (syntax_io.in[syntax_io.in_cnt].error >= SERR_ERROR) {
            syntax_io.in_cnt++;
            return SSTATUS_OK; // Let post-process show the error
        }

        syntax_io.in_cnt++;
    }

#ifdef SYNTAX_DEBUG
    printf("Out:\r\n");
    for (uint32_t i = 0; i < syntax_io.out_cnt; i++) {
        printf("%d:%d\r\n", syntax_io.out[i].command, syntax_io.out[i].repeat);
    }
    printf("In:\r\n");
    for (uint32_t i = 0; i < syntax_io.in_cnt; i++) {
        printf("%d:%d\r\n", syntax_io.in[i].command, syntax_io.in[i].repeat);
    }
#endif

    return SSTATUS_OK;
}
