/**
 * @file ui_statusbar.h
 * @brief Status bar display interface.
 * @details Provides bottom-of-screen status bar showing hardware state:
 *          pin voltages, PSU status, frequency, pullups, etc.
 */

#pragma once

/**
 * @brief Initialize status bar subsystem.
 */
void ui_statusbar_init(void);

/**
 * @brief Deinitialize status bar subsystem.
 */
void ui_statusbar_deinit(void);
