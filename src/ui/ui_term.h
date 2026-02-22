/**
 * @file ui_term.h
 * @brief Terminal control and VT100 support interface.
 * @details Provides terminal detection, color support, cursor control,
 *          and command-line editing functionality.
 */

#ifndef UI_TERM_H
#define UI_TERM_H

#include <stdbool.h>
#include <stdint.h>

/**
 * @brief Terminal color support type.
 */
typedef enum ui_term_type {
    UI_TERM_NO_COLOR = 0,  ///< No color support (plain text)
    UI_TERM_FULL_COLOR,    ///< Full RGB color support
#ifdef ANSI_COLOR_256
    UI_TERM_256            ///< 256-color ANSI support
#endif
} ui_term_type_e;

/**
 * @brief Predefined color identifiers for use with ui_term_color().
 */
typedef enum {
    UI_COLOR_RESET = 0, ///< Reset all attributes
    UI_COLOR_PROMPT,    ///< Command prompt color
    UI_COLOR_INFO,      ///< Informational message color
    UI_COLOR_NOTICE,    ///< Notice message color
    UI_COLOR_WARNING,   ///< Warning message color
    UI_COLOR_ERROR,     ///< Error message color
    UI_COLOR_NUM_FLOAT, ///< Numeric / float value color
    UI_COLOR_GREY,      ///< Grey / dim color
    UI_COLOR_PACMAN,    ///< Pacman animation color
    UI_COLOR_COUNT      ///< Number of color entries (must be last)
} ui_color_id_t;

/**
 * @brief Return the ANSI escape sequence for a predefined color.
 * @param id  Color identifier from ui_color_id_t.
 * @return Pointer to a null-terminated escape string, or "" when color is disabled.
 */
char* ui_term_color(ui_color_id_t id);

/**
 * @brief Initialize terminal subsystem.
 */
void ui_term_init(void);

/**
 * @brief Detect terminal capabilities (VT100, color support).
 * @return true if terminal detected, false otherwise
 */
bool ui_term_detect(void);

/**
 * @brief Set text color using RGB value.
 * @param rgb  24-bit RGB color (0xRRGGBB)
 */
void ui_term_color_text(uint32_t rgb);

/**
 * @brief Set background color using RGB value.
 * @param rgb  24-bit RGB color (0xRRGGBB)
 */
void ui_term_color_background(uint32_t rgb);

/**
 * @brief Set both text and background colors.
 * @param rgb_text        Text RGB color
 * @param rgb_background  Background RGB color
 * @return Combined color code
 */
uint32_t ui_term_color_text_background(uint32_t rgb_text, uint32_t rgb_background);

/**
 * @brief Generate color escape sequence into buffer.
 * @param buf             Output buffer
 * @param buffLen         Buffer length
 * @param rgb_text        Text RGB color
 * @param rgb_background  Background RGB color
 * @return Combined color code
 */
uint32_t ui_term_color_text_background_buf(char* buf, size_t buffLen, uint32_t rgb_text, uint32_t rgb_background);

/**
 * @name Color preset functions
 * @details Return ANSI escape sequences for predefined colors.
 * @{
 */
char* ui_term_color_reset(void);
char* ui_term_color_prompt(void);
char* ui_term_color_info(void);
char* ui_term_color_notice(void);
char* ui_term_color_warning(void);
char* ui_term_color_error(void);
char* ui_term_color_num_float(void);
char * ui_term_color_grey(void);
char* ui_term_color_pacman(void);
/** @} */

/**
 * @name Cursor control
 * @{
 */
char* ui_term_cursor_show(void);
char* ui_term_cursor_hide(void);
/** @} */

/**
 * @name VT100 primitive helpers (printf variants)
 * @details Each function emits a VT100 escape sequence directly via printf.
 *          All functions are no-ops when color/VT100 support is disabled.
 * @{
 */

/**
 * @brief Erase from cursor to end of line (\033[K).
 */
void ui_term_erase_line_printf(void);

/**
 * @brief Enable or disable reverse-video screen flash (\033[?5h / \033[?5l).
 * @param on  true to enable flash, false to disable.
 */
void ui_term_screen_flash_printf(bool on);

/**
 * @brief Move cursor to an absolute row/column position (\033[row;colH).
 * @param row  1-based row number.
 * @param col  1-based column number.
 */
void ui_term_cursor_position_printf(uint16_t row, uint16_t col);

/**
 * @brief Save cursor position (\0337).
 */
void ui_term_cursor_save_printf(void);

/**
 * @brief Restore previously saved cursor position (\0338).
 */
void ui_term_cursor_restore_printf(void);

/**
 * @brief Set the scrollable region (\033[top;bottomr).
 * @param top     First line of the scroll region (1-based).
 * @param bottom  Last line of the scroll region (1-based).
 */
void ui_term_scroll_region_printf(uint16_t top, uint16_t bottom);

/**
 * @brief Disable automatic line wrap (\033[7l).
 */
void ui_term_line_wrap_disable_printf(void);

/**
 * @brief Move cursor down N lines (\033[nB).
 * @param n  Number of lines to move down.
 */
void ui_term_cursor_move_down_printf(uint16_t n);

/**
 * @brief Move cursor right N columns (\033[nC).
 * @param n  Number of columns to move right.
 */
void ui_term_cursor_move_right_printf(uint16_t n);

/**
 * @brief Move cursor left N columns (\033[nD).
 * @param n  Number of columns to move left.
 */
void ui_term_cursor_move_left_printf(uint16_t n);

/** @} */

#ifndef UI_TERM_STRUCT
/**
 * @brief Progress bar state tracking.
 */
typedef struct ui_term_progress_bar_struct {
    uint8_t previous_pct;   ///< Previous percentage displayed
    uint8_t progress_cnt;   ///< Progress counter for animation
    bool indicator_state;   ///< Indicator animation state
} ui_term_progress_bar_t;
#define UI_TERM_STRUCT
#endif

/**
 * @brief Draw progress bar.
 * @param pb  Progress bar state structure
 */
void ui_term_progress_bar_draw(ui_term_progress_bar_t* pb);

/**
 * @brief Update progress bar with current progress.
 * @param current  Current progress value
 * @param total    Total progress value (100%)
 * @param pb       Progress bar state structure
 */
void ui_term_progress_bar_update(uint32_t current, uint32_t total, ui_term_progress_bar_t* pb);

/**
 * @brief Clean up and hide progress bar.
 * @param pb  Progress bar state structure
 */
void ui_term_progress_bar_cleanup(ui_term_progress_bar_t* pb);

/**
 * @brief Wait for a specific char from the terminal (or any if NUL).
 * @param c  Character to wait for, or '\\0' for any character
 * @return The character received
 */
char ui_term_cmdln_wait_char(char c);

#endif
