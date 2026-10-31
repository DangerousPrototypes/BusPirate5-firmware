/**
 * @file sys_stats.h
 * @brief 1-line system statistics toolbar.
 * @details Shows uptime, PSU state, USB connection, and toolbar count.
 */
#pragma once

#include <stdbool.h>

/**
 * @brief Start the sys_stats toolbar (register + draw).
 * @return true on success.
 */
bool sys_stats_start(void);

/**
 * @brief Stop and remove the sys_stats toolbar.
 */
void sys_stats_stop(void);

/**
 * @brief Check if the sys_stats toolbar is currently active.
 * @return true if active.
 */
bool sys_stats_is_active(void);

