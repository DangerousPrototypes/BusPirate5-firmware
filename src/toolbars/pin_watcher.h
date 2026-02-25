/**
 * @file pin_watcher.h
 * @brief 2-line GPIO pin state watcher toolbar.
 * @details Shows pin names and live HIGH/LOW states with per-pin colors.
 */
#pragma once

#include <stdbool.h>

/**
 * @brief Start the pin_watcher toolbar (register + draw).
 * @return true on success.
 */
bool pin_watcher_start(void);

/**
 * @brief Stop and remove the pin_watcher toolbar.
 */
void pin_watcher_stop(void);

/**
 * @brief Check if the pin_watcher toolbar is currently active.
 * @return true if active.
 */
bool pin_watcher_is_active(void);

/**
 * @brief Refresh the pin states row (call periodically or on change).
 */
void pin_watcher_update(void);

