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
#include "command_struct.h"
#include "commands.h"
#include "bytecode.h"  // For struct _bytecode used in modes.h
#include "modes.h"

// Main command line linenoise state (has history)
static bp_linenoise_state_t ln_state;
static bool ln_initialized = false;

// Sub-prompt linenoise state (no history, simpler prompts)
static bp_linenoise_state_t ln_prompt_state;
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

/**
 * @brief Tab completion callback for Bus Pirate commands.
 * @details Matches the current input against global commands and
 *          mode-specific commands for the active protocol.
 */
static void bp_completion_callback(const char *buf, size_t len, bp_linenoise_completions_t *lc) {
    if (len == 0) {
        return;  // Don't complete empty input
    }
    
    // Match against global commands
    for (uint32_t i = 0; i < commands_count; i++) {
        if (strncmp(buf, commands[i].command, len) == 0) {
            bp_linenoise_add_completion(lc, commands[i].command);
        }
    }
    
    // Match against mode-specific commands (if any)
    const struct _mode_command_struct *mode_cmds = modes[system_config.mode].mode_commands;
    const uint32_t *mode_count_ptr = modes[system_config.mode].mode_commands_count;
    if (mode_cmds && mode_count_ptr && *mode_count_ptr > 0) {
        uint32_t mode_count = *mode_count_ptr;
        for (uint32_t i = 0; i < mode_count; i++) {
            if (mode_cmds[i].func && strncmp(buf, mode_cmds[i].command, len) == 0) {
                bp_linenoise_add_completion(lc, mode_cmds[i].command);
            }
        }
    }
}

/**
 * @brief Hints callback for inline ghost-text completion.
 * @details Finds the best (longest) matching command and returns the
 *          remaining suffix to display as dim ghost text.
 *          E.g. user types "hel" → returns "p" (for "help").
 */
static const char* bp_hints_callback(const char *buf, size_t len) {
    if (len == 0) {
        return NULL;
    }
    
    const char *best = NULL;
    size_t best_len = 0;
    
    // Search global commands for best (longest) prefix match
    for (uint32_t i = 0; i < commands_count; i++) {
        size_t cmd_len = strlen(commands[i].command);
        if (cmd_len > len && strncmp(buf, commands[i].command, len) == 0) {
            // Prefer the longest matching command (e.g. "help" over "h")
            if (cmd_len > best_len) {
                best = commands[i].command;
                best_len = cmd_len;
            }
        }
    }
    
    // Search mode-specific commands
    const struct _mode_command_struct *mode_cmds = modes[system_config.mode].mode_commands;
    const uint32_t *mode_count_ptr = modes[system_config.mode].mode_commands_count;
    if (mode_cmds && mode_count_ptr && *mode_count_ptr > 0) {
        uint32_t mode_count = *mode_count_ptr;
        for (uint32_t i = 0; i < mode_count; i++) {
            if (mode_cmds[i].func) {
                size_t cmd_len = strlen(mode_cmds[i].command);
                if (cmd_len > len && strncmp(buf, mode_cmds[i].command, len) == 0) {
                    if (cmd_len > best_len) {
                        best = mode_cmds[i].command;
                        best_len = cmd_len;
                    }
                }
            }
        }
    }
    
    if (best) {
        return best + len;  // Return suffix after what user typed
    }
    return NULL;
}

/**
 * @brief Initialize linenoise for terminal use.
 * @param cols  Terminal width in columns
 */
void ui_term_linenoise_init(size_t cols) {
    bp_linenoise_init(&ln_state, ln_try_read, ln_read_blocking, ln_write, cols);
    bp_linenoise_set_completion(&ln_state, bp_completion_callback);
    bp_linenoise_set_hints(&ln_state, bp_hints_callback);
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
            cmdln_enable_linear_mode();  // Enable linear mode for parsing
            
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
        bp_linenoise_init(&ln_prompt_state, ln_try_read, ln_read_blocking, ln_write, 
                          system_config.terminal_ansi_columns);
        bp_linenoise_set_simple_mode(&ln_prompt_state, true);
        ln_prompt_initialized = true;
    }
}

/**
 * @brief Get user input for a sub-prompt (blocking, with line editing).
 * @param prompt  The prompt string to display (already printed by caller typically)
 * @return true if line complete (Enter), false if cancelled (Ctrl+C) or error
 * 
 * After return, the linear buffer reader (bp_cmdln) is set up for parsing.
 */
bool ui_prompt_linenoise_input(const char *prompt) {
    ui_prompt_linenoise_init();
    
    // Start editing with prompt (if provided, otherwise empty)
    bp_linenoise_start(&ln_prompt_state, prompt ? prompt : "");
    
    // Block until line complete or cancelled
    while (true) {
        if (system_config.error) {
            bp_linenoise_stop(&ln_prompt_state);
            return false;
        }
        
        bp_linenoise_result_t result = bp_linenoise_feed(&ln_prompt_state);
        
        switch (result) {
            case BP_LN_ENTER:
                bp_linenoise_stop(&ln_prompt_state);
                bp_cmdln_init_reader(ln_prompt_state.buf, ln_prompt_state.len);
                cmdln_enable_linear_mode();  // Enable linear mode for parsing
                return true;
                
            case BP_LN_CTRL_C:
            case BP_LN_CTRL_D:
                bp_linenoise_stop(&ln_prompt_state);
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
    bp_linenoise_result_t result = bp_linenoise_feed(&ln_prompt_state);
    
    switch (result) {
        case BP_LN_CONTINUE:
            return 0;
            
        case BP_LN_ENTER:
            bp_linenoise_stop(&ln_prompt_state);
            bp_cmdln_init_reader(ln_prompt_state.buf, ln_prompt_state.len);
            cmdln_enable_linear_mode();  // Enable linear mode for parsing
            return 0xff;
            
        case BP_LN_CTRL_C:
        case BP_LN_CTRL_D:
            bp_linenoise_stop(&ln_prompt_state);
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
    bp_linenoise_start(&ln_prompt_state, prompt ? prompt : "");
}