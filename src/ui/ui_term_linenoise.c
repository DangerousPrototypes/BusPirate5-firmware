/**
 * @file ui_term_linenoise.c
 * @brief Linenoise integration for Bus Pirate terminal.
 * @details Connects linenoise to USB CDC I/O and the command processing system.
 */

#include <stdio.h>
#include <string.h>
#include "pico/stdlib.h"
#include "pirate.h"
#include "system_config.h"
#include "usb_rx.h"
#include "usb_tx.h"
#include "lib/bp_linenoise/linenoise.h"
#include "lib/bp_linenoise/ln_cmdreader.h"
#include "ui/ui_term.h"
#include "ui/ui_statusbar.h"
#include "command_struct.h"
#include "commands.h"
#include "bytecode.h"  // For struct _bytecode used in modes.h
#include "modes.h"

// Main command line linenoise state (has history)
static struct linenoiseState ln_state;
static bool ln_initialized = false;

// Sub-prompt linenoise state (no history, simpler prompts)
static struct linenoiseState ln_prompt_state;
static bool ln_prompt_initialized = false;

// I/O callbacks for linenoise
static bool ln_try_read(char *c) {
    return rx_fifo_try_get(c);
}

static void ln_read_blocking(char *c) {
    rx_fifo_get_blocking(c);
}

static void ln_write(const char *s, size_t len) {
    for (size_t i = 0; i < len; i++) {
        tx_fifo_put((char*)&s[i]);
    }
}

// TODO: completion and hints callbacks will be wired up in a future phase
// once the linenoise.h callback signatures are adapted for embedded use.

/**
 * @brief Initialize linenoise for terminal use.
 * @param cols  Terminal width in columns
 */
void ui_term_linenoise_init(size_t cols) {
    linenoiseSetCallbacks(&ln_state, ln_try_read, ln_read_blocking, ln_write, cols);
    // TODO: wire up completion/hints callbacks in a future phase
    ln_initialized = true;
}

/**
 * @brief Update terminal width (after resize detection).
 * @param cols  New column count
 */
void ui_term_linenoise_set_cols(size_t cols) {
    if (ln_initialized) {
        linenoiseSetCols(&ln_state, cols);
    }
}

/**
 * @brief Start a new line editing session with prompt.
 * @param prompt  Prompt string to display
 */
void ui_term_linenoise_start(const char *prompt) {
    if (!ln_initialized) {
        ui_term_linenoise_init(80);  // Default width
    }
    linenoiseStartEdit(&ln_state, prompt);
}

/**
 * @brief Feed input to linenoise (non-blocking).
 * @return Result code:
 *         0 = still editing (no complete line)
 *         1 = key pressed (for screensaver reset)
 *         0xff = Enter pressed, line complete
 *         0xfe = Ctrl+C pressed
 *         0xfd = screen refresh requested (Ctrl+B)
 */
uint32_t ui_term_linenoise_feed(void) {
    linenoiseResult result = linenoiseEditFeedResult(&ln_state);
    
    switch (result) {
        case LN_CONTINUE:
            return 0;  // No input or still editing
            
        case LN_ENTER:
            // Line complete - set up linear buffer reader
            linenoiseEditStop(&ln_state);
            ln_cmdln_init(ln_state.buf, ln_state.len);
            
            // Add to history (if not empty)
            if (ln_state.len > 0) {
                linenoiseHistoryAdd(ln_state.buf);
            }
            return 0xff;  // Signal: line complete
            
        case LN_CTRL_C:
            // Ctrl+C - cancel current line
            linenoiseEditStop(&ln_state);
            return 0xfe;
            
        case LN_CTRL_D:
            // Ctrl+D on empty line - treat as cancel
            linenoiseEditStop(&ln_state);
            return 0xfe;
            
        case LN_REFRESH:
            // Ctrl+B - screen refresh requested
            return 0xfd;
            
        default:
            return 1;  // Key was pressed (for screensaver)
    }
}

/**
 * @brief Get the current line buffer.
 * @return Pointer to null-terminated line
 */
const char* ui_term_linenoise_get_line(void) {
    return ln_state.buf;
}

/**
 * @brief Get the current line length.
 * @return Length in bytes
 */
size_t ui_term_linenoise_get_len(void) {
    return ln_state.len;
}

/**
 * @brief Clear history.
 */
void ui_term_linenoise_clear_history(void) {
    linenoiseHistoryClear();
}

/*
 * =============================================================================
 * Sub-prompt support (for ui_prompt.c - voltage, current, y/n prompts, etc.)
 * =============================================================================
 * These use a separate linenoise state with NO history.
 * Provides full line editing for sub-prompts.
 */

/**
 * @brief Initialize prompt linenoise session (no history).
 */
static void ui_prompt_linenoise_init(void) {
    if (!ln_prompt_initialized) {
        linenoiseSetCallbacks(&ln_prompt_state, ln_try_read, ln_read_blocking, ln_write,
                              system_config.terminal_ansi_columns);
        linenoiseSetSimpleMode(&ln_prompt_state, true);
        ln_prompt_initialized = true;
    }
}

/**
 * @brief Get user input for a sub-prompt (blocking, with line editing).
 * @param prompt  The prompt string to display (already printed by caller typically)
 * @return true if line complete (Enter), false if cancelled (Ctrl+C) or error
 * 
 * After return, the linear buffer reader is set up for parsing.
 */
bool ui_prompt_linenoise_input(const char *prompt) {
    ui_prompt_linenoise_init();
    
    // Start editing with prompt (if provided, otherwise empty)
    linenoiseStartEdit(&ln_prompt_state, prompt ? prompt : "");
    
    // Block until line complete or cancelled
    while (true) {
        if (system_config.error) {
            linenoiseEditStop(&ln_prompt_state);
            return false;
        }
        
        linenoiseResult result = linenoiseEditFeedResult(&ln_prompt_state);
        
        switch (result) {
            case LN_ENTER:
                linenoiseEditStop(&ln_prompt_state);
                ln_cmdln_init(ln_prompt_state.buf, ln_prompt_state.len);
                return true;
                
            case LN_CTRL_C:
            case LN_CTRL_D:
                linenoiseEditStop(&ln_prompt_state);
                return false;
                
            default:
                // Continue editing
                break;
        }
    }
}

/**
 * @brief Get user input for sub-prompt (non-blocking feed).
 * @return Result code matching ui_term_linenoise_feed() 
 */
uint32_t ui_prompt_linenoise_feed(void) {
    linenoiseResult result = linenoiseEditFeedResult(&ln_prompt_state);
    
    switch (result) {
        case LN_CONTINUE:
            return 0;
            
        case LN_ENTER:
            linenoiseEditStop(&ln_prompt_state);
            ln_cmdln_init(ln_prompt_state.buf, ln_prompt_state.len);
            return 0xff;
            
        case LN_CTRL_C:
        case LN_CTRL_D:
            linenoiseEditStop(&ln_prompt_state);
            return 0xfe;
            
        default:
            return 1;
    }
}

/**
 * @brief Start a sub-prompt editing session.
 * @param prompt  Prompt to display (can be empty string "")
 */
void ui_prompt_linenoise_start(const char *prompt) {
    ui_prompt_linenoise_init();
    linenoiseStartEdit(&ln_prompt_state, prompt ? prompt : "");
}

/*
 * =============================================================================
 * Command injection (for macros and scripts)
 * =============================================================================
 * Writes a command string directly into the main linenoise buffer
 * and sets up the linear reader for immediate parsing.
 * No echo, no editing — just buffer injection.
 */

/**
 * @brief Inject a command string for processing (no echo/editing).
 * @details Used by macros and scripts to feed commands directly into
 *          the command processing pipeline. Writes the string into the
 *          main linenoise state buffer and sets up the linear reader.
 * @param str  Null-terminated command string to inject
 * @return true if string fit in buffer, false if truncated
 */
bool ui_term_linenoise_inject_string(const char *str) {
    if (!ln_initialized) {
        ui_term_linenoise_init(80);
    }

    size_t slen = strlen(str);
    if (slen > BP_LINENOISE_MAX_LINE) {
        slen = BP_LINENOISE_MAX_LINE;
    }

    memcpy(ln_state.buf, str, slen);
    ln_state.buf[slen] = '\0';
    ln_state.len = slen;
    ln_state.pos = slen;

    /* Set up the linear reader so cmdln_try_peek/remove route here. */
    ln_cmdln_init(ln_state.buf, ln_state.len);

    return (slen == strlen(str));
}