/*
 * arduboy.c — Arduboy emulator mode for Bus Pirate
 *
 * Integrates the AVR interpreter, peripheral emulation, and terminal
 * display/input backend into a Bus Pirate protocol mode.
 *
 * When the user selects the Arduboy mode:
 *   1. setup()     — prompts for .hex filename from flash storage
 *   2. setup_exec()— loads .hex, initializes emulator
 *   3. The "play" mode command launches the fullscreen game loop
 *   4. cleanup()   — tears down emulator, returns to HiZ
 *
 * The game loop runs as a blocking command on Core 0, with the
 * toolbar paused. Input comes from the USB serial terminal via
 * vt100_keys, display renders as ANSI art.
 *
 * Memory layout:
 *   BIG_BUFFER (128KB) is carved up as:
 *     [0..32KB)        — AVR flash (program memory)
 *     [32KB..35.75KB)  — AVR data space (regs + I/O + SRAM)
 *     [35.75KB..36.75KB) — AVR EEPROM
 *     [36.75KB..128KB) — Scratch / hex file loading buffer
 *
 * Copyright (c) 2026 Bus Pirate project. MIT License.
 */

#include <stdio.h>
#include <string.h>
#include "pico/stdlib.h"
#include "pico/time.h"
#include "pirate.h"
#include "system_config.h"
#include "command_struct.h"
#include "ui/ui_term.h"
#include "ui/ui_toolbar.h"
#include "ui/ui_help.h"
#include "usb_rx.h"
#include "usb_tx.h"
#include "pirate/mem.h"
#include "pirate/storage.h"
#include "fatfs/ff.h"
#include "bytecode.h"
#include "modes.h"
#include "lib/bp_args/bp_cmd.h"

#include "arduboy.h"
#include "avr_cpu.h"
#include "arduboy_periph.h"
#include "arduboy_term.h"

/* ── Memory carve-up within BIG_BUFFER ────────────────────────── */
#define EMU_FLASH_OFFSET    0
#define EMU_FLASH_SIZE      AVR_FLASH_SIZE          /* 32768 */
#define EMU_SRAM_OFFSET     (EMU_FLASH_OFFSET + EMU_FLASH_SIZE)
#define EMU_SRAM_SIZE       (AVR_RAMEND + 1)        /* 2816 */
#define EMU_EEPROM_OFFSET   (EMU_SRAM_OFFSET + EMU_SRAM_SIZE)
#define EMU_EEPROM_SIZE     AVR_EEPROM_SIZE         /* 1024 */
#define EMU_SCRATCH_OFFSET  (EMU_EEPROM_OFFSET + EMU_EEPROM_SIZE)
/* Scratch area for hex file loading — remaining buffer space */

/* ── Module state ─────────────────────────────────────────────── */

static uint8_t* arena = NULL;   /* BIG_BUFFER pointer */
static avr_cpu_t cpu;
static arduboy_periph_t periph;
static arduboy_term_t term;
static char loaded_filename[64];

/* ── AVR cycles per frame ─────────────────────────────────────── */
/*
 * ATmega32U4 @ 16MHz, Arduboy targets 60fps:
 *   16,000,000 / 60 = 266,666 cycles per frame
 */
#define CYCLES_PER_FRAME    266666
#define TARGET_FRAME_US     16667   /* 1/60th second in microseconds */

/* ── Load a .hex file from flash ──────────────────────────────── */

static bool load_hex_file(const char* filename) {
    FIL fil;
    FRESULT fr;
    UINT bytes_read;

    fr = f_open(&fil, filename, FA_READ);
    if (fr != FR_OK) {
        printf("Error: cannot open '%s' (FatFS error %d)\r\n", filename, fr);
        return false;
    }

    /* Read into scratch area of BIG_BUFFER */
    uint32_t scratch_size = BIG_BUFFER_SIZE - EMU_SCRATCH_OFFSET - 1;
    uint8_t* scratch = arena + EMU_SCRATCH_OFFSET;

    fr = f_read(&fil, scratch, scratch_size, &bytes_read);
    f_close(&fil);

    if (fr != FR_OK) {
        printf("Error: read failed (FatFS error %d)\r\n", fr);
        return false;
    }

    scratch[bytes_read] = '\0'; /* Null-terminate for hex parser */

    printf("Loaded %lu bytes from '%s'\r\n", (unsigned long)bytes_read, filename);

    /* Clear flash before loading */
    memset(arena + EMU_FLASH_OFFSET, 0xFF, EMU_FLASH_SIZE);

    /* Parse Intel HEX into emulated flash */
    if (!avr_load_hex(&cpu, (const char*)scratch, bytes_read)) {
        printf("Error: invalid Intel HEX format\r\n");
        return false;
    }

    printf("Program loaded into emulated flash\r\n");
    return true;
}

/* ── Game loop ────────────────────────────────────────────────── */

static void game_loop(void) {
    /* Pause toolbar to prevent VT100 interleaving */
    toolbar_draw_prepare();

    /* Drain stale input */
    { char drain; while (rx_fifo_try_get(&drain)) {} }

    /* Initialize terminal display */
    arduboy_term_init(&term, &PALETTE_GREEN, loaded_filename);

    /* Reset CPU and peripherals for clean start */
    avr_cpu_reset(&cpu);
    arduboy_periph_reset(&periph);

    /* Main emulation loop */
    bool running = true;
    uint64_t frame_start;

    while (running) {
        frame_start = time_us_64();

        /* ── Input phase ── */
        /* Tick hold timers down for this new emulated frame */
        arduboy_term_frame_tick(&term);
        /* Read any new keypresses and refresh hold timers */
        arduboy_term_update_buttons(&term);
        {
            uint8_t buttons = arduboy_term_get_buttons(&term);
            if (buttons == 0xFF) { running = false; break; }
            arduboy_periph_set_buttons(&periph, buttons);
        }

        /* ── CPU execution phase ── */
        /*
         * Batch peripheral ticks to every TICK_BATCH cycles.
         * Timer0 at /64 prescaler ticks once per 64 cycles, so
         * batching at 64 keeps timer accuracy identical.
         *
         * INPUT_POLL_INTERVAL: also poll input mid-frame so that
         * keypresses during the (potentially long) CPU phase are
         * picked up promptly instead of waiting for next frame.
         */
        #define TICK_BATCH 64
        #define INPUT_POLL_INTERVAL 16000  /* ~1ms worth of AVR cycles */
        uint32_t cycles_remaining = CYCLES_PER_FRAME;
        uint32_t tick_accum = 0;
        uint32_t input_accum = 0;
        while (cycles_remaining > 0 && !cpu.halted) {
            if (cpu.sleeping) {
                uint32_t chunk = (cycles_remaining > TICK_BATCH) ? TICK_BATCH : cycles_remaining;
                cpu.cycle_count += chunk;
                arduboy_periph_tick(&periph, chunk);
                arduboy_periph_check_interrupts(&periph);
                cycles_remaining -= chunk;
                input_accum += chunk;
                if (input_accum >= INPUT_POLL_INTERVAL) {
                    arduboy_term_update_buttons(&term);
                    if (term.exit_requested) { running = false; break; }
                    uint8_t btn = arduboy_term_get_buttons(&term);
                    arduboy_periph_set_buttons(&periph, btn);
                    input_accum = 0;
                }
                continue;
            }
            uint8_t c = avr_cpu_step(&cpu);
            cpu.cycle_count += c;
            tick_accum += c;
            input_accum += c;
            if (tick_accum >= TICK_BATCH) {
                arduboy_periph_tick(&periph, tick_accum);
                arduboy_periph_check_interrupts(&periph);
                tick_accum = 0;
            }
            /* Poll input mid-frame (~every 1ms of emulated time) */
            if (input_accum >= INPUT_POLL_INTERVAL) {
                arduboy_term_update_buttons(&term);
                if (term.exit_requested) { running = false; break; }
                uint8_t btn = arduboy_term_get_buttons(&term);
                arduboy_periph_set_buttons(&periph, btn);
                input_accum = 0;
            }
            /* Render when a full 1024-byte SSD1306 transfer completes */
            if (periph.ssd1306.transfer_complete) {
                periph.ssd1306.transfer_complete = false;
                arduboy_term_render(&term, arduboy_periph_get_fb(&periph));
                periph.ssd1306.framebuffer_dirty = false;
            }
            if (c >= cycles_remaining) break;
            cycles_remaining -= c;
        }
        /* Flush remaining accumulated cycles */
        if (tick_accum > 0) {
            arduboy_periph_tick(&periph, tick_accum);
            arduboy_periph_check_interrupts(&periph);
        }

        /* ── Render phase ── */
        /* Most rendering happens inline during CPU execution (above).
         * This catches any final dirty state after the loop exits. */
        if (arduboy_periph_fb_dirty(&periph)) {
            arduboy_term_render(&term, arduboy_periph_get_fb(&periph));
        }

        /* Status bar update */
        arduboy_term_status(&term, arduboy_periph_millis(&periph));

        /* ── Frame timing ── */
        uint64_t elapsed = time_us_64() - frame_start;
        if (elapsed < TARGET_FRAME_US) {
            sleep_us(TARGET_FRAME_US - elapsed);
        }

        /* Check for halted CPU */
        if (cpu.halted) {
            /* Brief pause then exit */
            sleep_ms(500);
            running = false;
        }
    }

    /* Cleanup terminal */
    arduboy_term_cleanup(&term);

    /* Drain any remaining input */
    { char drain; while (rx_fifo_try_get(&drain)) {} }

    /* Restore terminal state */
    printf("\x1b[?1049l"); /* leave alt screen (redundant safety) */
    toolbar_apply_scroll_region();
    ui_term_cursor_position(toolbar_scroll_bottom(), 0);
    toolbar_draw_release();

    printf("Game exited.\r\n");
}

/* ── Mode command: "play" ─────────────────────────────────────── */

static void cmd_play_handler(struct command_result* res) {
    if (!arena) {
        printf("Error: no ROM loaded. Re-enter Arduboy mode.\r\n");
        res->error = true;
        return;
    }
    game_loop();
}

static void cmd_load_handler(struct command_result* res) {
    /* List .hex files on storage */
    printf("Loading new ROM...\r\n");
    printf("Enter .hex filename: ");

    /* Read filename from terminal (simple blocking read until Enter) */
    char filename[64];
    uint8_t pos = 0;
    while (pos < sizeof(filename) - 1) {
        char c;
        rx_fifo_get_blocking(&c);
        if (c == '\r' || c == '\n') break;
        if (c == 0x7f || c == 0x08) { /* Backspace */
            if (pos > 0) {
                pos--;
                printf("\b \b");
            }
            continue;
        }
        if (c >= ' ' && c <= '~') {
            filename[pos++] = c;
            printf("%c", c);
        }
    }
    filename[pos] = '\0';
    printf("\r\n");

    if (pos == 0) {
        printf("Cancelled.\r\n");
        return;
    }

    if (load_hex_file(filename)) {
        strncpy(loaded_filename, filename, sizeof(loaded_filename) - 1);
        loaded_filename[sizeof(loaded_filename) - 1] = '\0';
        printf("ROM loaded. Type 'play' to start.\r\n");
    }
}

static void cmd_info_handler(struct command_result* res) {
    printf("Arduboy Emulator\r\n");
    printf("  ROM: %s\r\n", loaded_filename[0] ? loaded_filename : "(none)");
    printf("  CPU: ATmega32U4 (emulated)\r\n");
    printf("  Flash: %d bytes\r\n", AVR_FLASH_SIZE);
    printf("  SRAM:  %d bytes\r\n", AVR_SRAM_SIZE);
    printf("  EEPROM: %d bytes\r\n", AVR_EEPROM_SIZE);
    printf("  Display: 128x64 → terminal (ANSI)\r\n");
    printf("  Cycles/frame: %d (~60fps @ 16MHz)\r\n", CYCLES_PER_FRAME);
}

/* ── Mode command definitions ─────────────────────────────────── */

static const char* const play_usage[] = { "play", "Start the loaded Arduboy game" };
static const bp_command_def_t play_def = {
    .name = "play",
    .description = 0,
    .usage = play_usage,
    .usage_count = 2,
};

static const char* const load_usage[] = { "load", "Load a .hex ROM from flash storage" };
static const bp_command_def_t load_def = {
    .name = "load",
    .description = 0,
    .usage = load_usage,
    .usage_count = 2,
};

static const char* const info_usage[] = { "info", "Show emulator status" };
static const bp_command_def_t info_def = {
    .name = "info",
    .description = 0,
    .usage = info_usage,
    .usage_count = 2,
};

const struct _mode_command_struct arduboy_commands[] = {
    {
        .func = cmd_play_handler,
        .def = &play_def,
        .supress_fala_capture = true,
    },
    {
        .func = cmd_load_handler,
        .def = &load_def,
        .supress_fala_capture = true,
    },
    {
        .func = cmd_info_handler,
        .def = &info_def,
        .supress_fala_capture = true,
    },
};
const uint32_t arduboy_commands_count = count_of(arduboy_commands);

/* ── Mode interface ───────────────────────────────────────────── */

uint32_t arduboy_setup(void) {
    printf("Arduboy Emulator Mode\r\n");
    printf("Place .hex files on the Bus Pirate flash drive.\r\n");

    /* During development: default to game.hex */
    strncpy(loaded_filename, "game.hex", sizeof(loaded_filename) - 1);
    loaded_filename[sizeof(loaded_filename) - 1] = '\0';
    printf("Auto-loading: %s\r\n", loaded_filename);

    return 1;
}

uint32_t arduboy_setup_exec(void) {
    /* Allocate BIG_BUFFER */
    arena = mem_alloc(BIG_BUFFER_SIZE, BP_BIG_BUFFER_ARDUBOY);
    if (!arena) {
        printf("Error: memory in use by another feature\r\n");
        return 0;
    }

    /* Initialize CPU with carved-up buffer regions */
    uint8_t* flash  = arena + EMU_FLASH_OFFSET;
    uint8_t* sram   = arena + EMU_SRAM_OFFSET;
    uint8_t* eeprom = arena + EMU_EEPROM_OFFSET;

    memset(flash, 0xFF, EMU_FLASH_SIZE);
    memset(sram, 0, EMU_SRAM_SIZE);
    memset(eeprom, 0xFF, EMU_EEPROM_SIZE);

    avr_cpu_init(&cpu, flash, sram, eeprom);
    arduboy_periph_init(&periph, &cpu);

    /* Load ROM if filename was provided */
    if (loaded_filename[0]) {
        if (!load_hex_file(loaded_filename)) {
            printf("ROM load failed. Use 'load' command to try another file.\r\n");
        } else {
            /* Verify flash contents */
            printf("Flash[0..7]: %02X %02X %02X %02X %02X %02X %02X %02X\r\n",
                flash[0], flash[1], flash[2], flash[3],
                flash[4], flash[5], flash[6], flash[7]);
            uint16_t word0 = (uint16_t)flash[0] | ((uint16_t)flash[1] << 8);
            printf("Reset vector word: 0x%04X\r\n", word0);
            printf("Type 'play' to start the game, 'info' for details.\r\n");
        }
    } else {
        printf("No ROM loaded. Use 'load' to load a .hex file.\r\n");
    }

    return 1;
}

void arduboy_cleanup(void) {
    if (arena) {
        mem_free(arena);
        arena = NULL;
    }
    loaded_filename[0] = '\0';
}

void arduboy_help(void) {
    printf("%sArduboy Emulator Mode%s\r\n", ui_term_color_info(), ui_term_color_reset());
    printf("Emulates an Arduboy (ATmega32U4 + SSD1306) in the terminal.\r\n");
    printf("\r\n");
    printf("Commands:\r\n");
    printf("  play  — Start the loaded game (fullscreen terminal)\r\n");
    printf("  load  — Load a .hex ROM file from flash storage\r\n");
    printf("  info  — Show emulator status\r\n");
    printf("\r\n");
    printf("In-game controls:\r\n");
    printf("  Arrow keys  = D-pad (UP/DOWN/LEFT/RIGHT)\r\n");
    printf("  Z or N      = A button\r\n");
    printf("  X or M      = B button\r\n");
    printf("  ESC         = Exit to Bus Pirate prompt\r\n");
    printf("\r\n");
    printf("To load games: connect Bus Pirate as USB drive, copy .hex files.\r\n");
    ui_help_mode_commands(arduboy_commands, arduboy_commands_count);
}
