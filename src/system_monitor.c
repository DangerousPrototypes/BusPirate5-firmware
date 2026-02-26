#include <string.h>
#include "pico/stdlib.h"
#include "pirate.h"
#include "system_config.h"
#include "pirate/amux.h"
#include "display/scope.h"
#include "system_monitor.h"

static monitor_snapshot_t snapshot;
/*
 * Dirty flag synchronisation note:
 * Core0 only sets bits (|=) in dirty.pin_config and sets dirty.info_bar to true.
 * Core1 only clears (= 0 / false) via monitor_consume_dirty().
 * Single-word reads/writes are naturally atomic on Cortex-M0+.
 * The |= read-modify-write is NOT atomic, so in theory a clear by core1 could
 * be overwritten.  This matches the previous system_config.pin_changed pattern
 * and is acceptable: the worst case is an extra UI update cycle.
 */
static volatile monitor_dirty_t dirty;

void monitor_init(void) {
    memset(&snapshot, 0, sizeof(snapshot));
    dirty.pin_config = 0xffffffff;
    dirty.info_bar = false;
}

const monitor_snapshot_t* monitor_get_snapshot(void) {
    return &snapshot;
}

bool monitor_update(void) {
    if (scope_running) {
        return false;
    }

    amux_sweep();

    bool changed = false;
    for (uint8_t i = 0; i < HW_PINS; i++) {
        uint16_t mv = (uint16_t)(*hw_pin_voltage_ordered[i]);
        if (mv != snapshot.voltage_mv[i]) {
            snapshot.voltage_mv[i] = mv;
            changed = true;
        }
    }

    uint32_t raw = hw_adc_raw[HW_ADC_CURRENT_SENSE];
    if (raw != snapshot.current_raw) {
        snapshot.current_raw = raw;
        snapshot.current_ua = ((raw >> 1) * ((500 * 1000) / 2048));
        changed = true;
    }

    return changed;
}

void monitor_mark_pin_changed(uint8_t pin) {
    dirty.pin_config |= (1u << pin);
}

void monitor_mark_all_pins_changed(void) {
    dirty.pin_config = 0xffffffff;
}

void monitor_mark_info_changed(void) {
    dirty.info_bar = true;
}

monitor_dirty_t monitor_consume_dirty(void) {
    monitor_dirty_t d = dirty;
    dirty.pin_config = 0;
    dirty.info_bar = false;
    return d;
}
