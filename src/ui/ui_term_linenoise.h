/**
 * @file ui_term_linenoise.h
 * @brief Linenoise integration for Bus Pirate terminal.
 */

#ifndef UI_TERM_LINENOISE_H
#define UI_TERM_LINENOISE_H

#include <stdint.h>
#include <stddef.h>

/**
 * @brief Initialize linenoise for terminal use.
 * @param cols  Terminal width in columns
 */
void ui_term_linenoise_init(size_t cols);

/**
 * @brief Update terminal width (after resize detection).
 * @param cols  New column count
 */
void ui_term_linenoise_set_cols(size_t cols);

/**
 * @brief Start a new line editing session with prompt.
 * @param prompt  Prompt string to display
 */
void ui_term_linenoise_start(const char *prompt);

/**
 * @brief Feed input to linenoise (non-blocking).
 * @return Result code:
 *         0 = still editing (no complete line)
 *         1 = key pressed (for screensaver reset)  
 *         0xff = Enter pressed, line complete
 *         0xfe = Ctrl+C pressed
 *         0xfd = screen refresh requested (Ctrl+B)
 */
uint32_t ui_term_linenoise_feed(void);

/**
 * @brief Get the current line buffer.
 * @return Pointer to null-terminated line
 */
const char* ui_term_linenoise_get_line(void);

/**
 * @brief Get the current line length.
 * @return Length in bytes
 */
size_t ui_term_linenoise_get_len(void);

/**
 * @brief Clear history.
 */
void ui_term_linenoise_clear_history(void);

/*
 * =============================================================================
 * Sub-prompt support (ui_prompt.c)
 * =============================================================================
 */

/**
 * @brief Get user input for sub-prompt (blocking, with line editing).
 * @param prompt  Prompt to display (or NULL for no prompt)
 * @return true on Enter (line ready), false on Ctrl+C/error
 */
bool ui_prompt_linenoise_input(const char *prompt);

/**
 * @brief Start a sub-prompt editing session (non-blocking).
 * @param prompt  Prompt to display (or NULL/"" for no prompt)
 */
void ui_prompt_linenoise_start(const char *prompt);

/**
 * @brief Feed input to sub-prompt linenoise (non-blocking).
 * @return Same codes as ui_term_linenoise_feed()
 */
uint32_t ui_prompt_linenoise_feed(void);

#endif // UI_TERM_LINENOISE_H
