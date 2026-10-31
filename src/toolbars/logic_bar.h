#ifndef LOGIC_BAR_H
#define LOGIC_BAR_H

#include <stdbool.h>
#include <stdint.h>

/**
 * @brief Redraw logic bar with sample data.
 * @param start_pos     Starting sample position in the buffer.
 * @param total_samples Total number of samples captured.
 */
void logic_bar_redraw(uint32_t start_pos, uint32_t total_samples);

/**
 * @brief Start the logic bar (register toolbar, draw frame).
 * @return true on success, false if FALA registration or toolbar registry fails.
 */
bool logic_bar_start(void);

/**
 * @brief Stop the logic bar (unregister FALA + teardown toolbar).
 */
void logic_bar_stop(void);

/**
 * @brief Hide the logic bar without stopping FALA capture.
 */
void logic_bar_hide(void);

/**
 * @brief Show a previously hidden logic bar.
 */
void logic_bar_show(void);

/**
 * @brief Enter interactive navigation mode (arrow keys to scroll).
 */
void logic_bar_navigate(void);

/**
 * @brief Periodic update callback — redraws if visible.
 */
void logic_bar_update(void);

/**
 * @brief Configure graph display characters.
 * @param low  Character for logic-low (0 = keep current).
 * @param high Character for logic-high (0 = keep current).
 */
void logic_bar_config(char low, char high);

#endif // LOGIC_BAR_H