/**
 * @file ui_term_linenoise.c
 * @brief Linenoise integration for Bus Pirate terminal.
 * @details Connects bp_linenoise to USB CDC I/O and the command processing system.
 */

#include <stdio.h>
#include <string.h>
#include "pico/stdlib.h"
#include "pirate.h"
#include "system_config.h"
#include "usb_rx.h"
#include "usb_tx.h"
#include "lib/bp_linenoise/bp_linenoise.h"
#include "ui/ui_term.h"
#include "ui/ui_cmdln.h"
#include "ui/ui_statusbar.h"

// Global linenoise state
static bp_linenoise_state_t ln_state;
static bool ln_initialized = false;

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

/**
 * @brief Initialize linenoise for terminal use.
 * @param cols  Terminal width in columns
 */
void ui_term_linenoise_init(size_t cols) {
    bp_linenoise_init(&ln_state, ln_try_read, ln_read_blocking, ln_write, cols);
    ln_initialized = true;
}

/**
 * @brief Update terminal width (after resize detection).
 * @param cols  New column count
 */
void ui_term_linenoise_set_cols(size_t cols) {
    if (ln_initialized) {
        bp_linenoise_set_cols(&ln_state, cols);
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
    bp_linenoise_start(&ln_state, prompt);
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
    bp_linenoise_result_t result = bp_linenoise_feed(&ln_state);
    
    switch (result) {
        case BP_LN_CONTINUE:
            return 0;  // No input or still editing
            
        case BP_LN_ENTER:
            // Line complete - set up linear buffer reader
            bp_linenoise_stop(&ln_state);
            bp_cmdln_init_reader(ln_state.buf, ln_state.len);
            
            // Add to history (if not empty)
            if (ln_state.len > 0) {
                bp_linenoise_history_add(&ln_state, ln_state.buf);
            }
            return 0xff;  // Signal: line complete
            
        case BP_LN_CTRL_C:
            // Ctrl+C - cancel current line
            bp_linenoise_stop(&ln_state);
            return 0xfe;
            
        case BP_LN_CTRL_D:
            // Ctrl+D on empty line - treat as cancel
            bp_linenoise_stop(&ln_state);
            return 0xfe;
            
        case BP_LN_REFRESH:
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
    return bp_linenoise_get_line(&ln_state);
}

/**
 * @brief Get the current line length.
 * @return Length in bytes
 */
size_t ui_term_linenoise_get_len(void) {
    return bp_linenoise_get_len(&ln_state);
}

/**
 * @brief Clear history.
 */
void ui_term_linenoise_clear_history(void) {
    bp_linenoise_history_clear(&ln_state);
}
