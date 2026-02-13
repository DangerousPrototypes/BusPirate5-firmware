/**
 * @file ui_term.h
 * @brief Terminal control and VT100 support interface.
 * @details Provides terminal detection, color support, cursor control,
 *          and command-line editing functionality.
 */

#ifndef UI_TERM_H
#define UI_TERM_H

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
