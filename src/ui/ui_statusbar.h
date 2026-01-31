/**
 * @file ui_statusbar.h
 * @brief Status bar display interface.
 * @details Provides bottom-of-screen status bar showing hardware state:
 *          pin voltages, PSU status, frequency, pullups, etc.
 */

#pragma once

/**
 * @brief Update status bar (blocking, from main core).
 */
void ui_statusbar_update_blocking();

/**
 * @brief Update status bar from core1 (non-blocking).
 * @param update_flags  Flags indicating which elements to update
 */
void ui_statusbar_update_from_core1(uint32_t update_flags);

/**
 * @brief Initialize status bar subsystem.
 */
void ui_statusbar_init(void);

/**
 * @brief Deinitialize status bar subsystem.
 */
void ui_statusbar_deinit(void);
