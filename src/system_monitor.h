/**
 * @file system_monitor.h
 * @brief Thin numeric ADC cache for voltage and current monitoring.
 * @details Provides a read-only snapshot of current measurements updated by
 *          monitor_update().  Consumers compare against their own shadow
 *          copies to detect changes — the monitor does NOT track dirty
 *          state per-consumer.
 *
 *          Dirty flags for pin-config and info-bar changes are managed
 *          through monitor_mark_pin_changed() / monitor_mark_info_changed()
 *          and consumed atomically by monitor_consume_dirty().
 */

#pragma once

#include <stdint.h>
#include <stdbool.h>
#include "pirate.h"

/**
 * @brief Read-only snapshot of current measurements.
 *
 * Updated by monitor_update().  Consumers compare against their own
 * shadow copies to detect changes.
 */
typedef struct {
    uint16_t voltage_mv[HW_PINS]; ///< Per-pin voltage in millivolts
    uint32_t current_raw;         ///< Raw ADC count for current sense
    uint32_t current_ua;          ///< Current in microamps (derived)
} monitor_snapshot_t;

/**
 * @brief Dirty flags for pin configuration and info bar changes.
 *
 * Set by core0 commands, consumed atomically by core1.
 */
typedef struct {
    uint32_t pin_config; ///< Bitmask: which pins' label/function changed
    bool info_bar;       ///< PSU/pullup/scope status changed
} monitor_dirty_t;

/// Read ADC (amux_sweep), update snapshot.  Returns true if any value changed.
bool monitor_update(void);

/// Get pointer to current snapshot (valid until next monitor_update).
const monitor_snapshot_t* monitor_get_snapshot(void);

/// Initialise snapshot to zeros.
void monitor_init(void);

/// Called by commands (core0) to mark a pin's config as changed.
void monitor_mark_pin_changed(uint8_t pin);

/// Called by commands (core0) to mark all pins as changed.
void monitor_mark_all_pins_changed(void);

/// Called by commands (core0) to mark the info bar as changed.
void monitor_mark_info_changed(void);

/// Called by core1 after dispatching updates.  Returns the flags and clears them.
monitor_dirty_t monitor_consume_dirty(void);
