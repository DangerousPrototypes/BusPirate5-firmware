#include <string.h>
#include "pico/stdlib.h"
#include "pirate.h"
#include "system_config.h"
#include "pirate/amux.h"
#include "display/scope.h"
#include "system_monitor.h"

static monitor_snapshot_t snapshot;
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
