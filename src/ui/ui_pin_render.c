/**
 * @file ui_pin_render.c
 * @brief Unified pin name / label / voltage rendering.
 * @details Single implementation shared by the `v`/`V` command (printf mode,
 *          buf == NULL) and the status bar (buffer mode, buf != NULL).
 *          This eliminates the duplicate implementations that previously lived
 *          in ui_info.c and ui_statusbar.c.
 *
 *          In printf mode the caller is responsible for calling amux_sweep()
 *          before ui_pin_render_values() to get fresh ADC readings.
 *          In buffer mode the system monitor provides the voltage strings.
 */

#include <stdio.h>
#include <stdarg.h>
#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include "pico/stdlib.h"
#include "pirate.h"
#include "system_config.h"
#include "command_struct.h"
#include "ui/ui_term.h"
#include "ui/ui_const.h"
#include "commands/global/freq.h"
#include "system_monitor.h"
#include "pirate/psu.h"
#include "ui/ui_pin_render.h"

/* -------------------------------------------------------------------------
 * Output helpers: write to buffer or stdout depending on buf argument.
 * ------------------------------------------------------------------------- */

static uint32_t out_str(char* buf, size_t buf_len, const char* s) {
    if (buf) {
        return (uint32_t)snprintf(buf, buf_len, "%s", s);
    }
    printf("%s", s);
    return 0;
}

static uint32_t out_fmt(char* buf, size_t buf_len, const char* fmt, ...)
    __attribute__((format(printf, 3, 4)));
static uint32_t out_fmt(char* buf, size_t buf_len, const char* fmt, ...) {
    va_list ap;
    va_start(ap, fmt);
    uint32_t n = 0;
    if (buf) {
        n = (uint32_t)vsnprintf(buf, buf_len, fmt, ap);
    } else {
        vprintf(fmt, ap);
    }
    va_end(ap);
    return n;
}

/* -------------------------------------------------------------------------
 * Names row:  "1.IO0  2.IO1  ..."
 * Identical layout for both printf and buffer paths.
 * ------------------------------------------------------------------------- */
uint32_t ui_pin_render_names(char* buf, size_t buf_len) {
    uint32_t len = 0;

    for (int i = 0; i < HW_PINS; i++) {
        if (buf) {
            len += (uint32_t)ui_term_color_text_background_buf(
                buf + len, buf_len - len,
                hw_pin_label_ordered_color[i][0],
                hw_pin_label_ordered_color[i][1]);
        } else {
            ui_term_color_text_background(hw_pin_label_ordered_color[i][0],
                                          hw_pin_label_ordered_color[i][1]);
        }
        len += out_fmt(buf ? buf + len : NULL, buf ? buf_len - len : 0,
                       "\033[8X%d.%s\t", i + 1, hw_pin_label_ordered[i]);
    }

    if (buf) {
        len += out_str(buf + len, buf_len - len, ui_term_color_reset());
    } else {
        printf("%s\r\n", ui_term_color_reset());
    }

    return len;
}

/* -------------------------------------------------------------------------
 * Labels row: current mA for Vout, mode labels for other pins.
 *
 * Buffer mode (statusbar): only writes a cell when the pin has changed,
 *   otherwise outputs a bare tab so the existing on-screen value stays.
 * Printf mode (v command): always writes all cells; no change-tracking needed.
 * ------------------------------------------------------------------------- */
uint32_t ui_pin_render_labels(char* buf, size_t buf_len) {
    uint32_t len = 0;

    for (uint i = 0; i < HW_PINS; i++) {
        bool changed = (system_config.pin_changed & (0x01 << (uint8_t)i)) != 0;

        switch (system_config.pin_func[i]) {

            case BP_PIN_VOUT: {
                char* c;
                bool have_current = monitor_get_current_ptr(&c);
                if (buf) {
                    if (have_current || changed) {
                        len += out_fmt(buf + len, buf_len - len,
                                       "\033[8X%s%s%smA\t",
                                       ui_term_color_num_float(), c, ui_term_color_reset());
                    } else {
                        len += out_str(buf + len, buf_len - len, "\t");
                    }
                } else {
                    /* printf mode: show current if PSU on, else dash */
                    if (have_current) {
                        printf("%s%s%smA\t", ui_term_color_num_float(), c, ui_term_color_reset());
                    } else {
                        printf("-\t");
                    }
                }
                break;
            }

            default: {
                const char* lbl = system_config.pin_labels[i] == 0
                                       ? "-"
                                       : (const char*)system_config.pin_labels[i];
                if (buf) {
                    if (changed) {
                        len += out_fmt(buf + len, buf_len - len, "\033[8X%s\t", lbl);
                    } else {
                        len += out_str(buf + len, buf_len - len, "\t");
                    }
                } else {
                    printf("%s\t", lbl);
                }
                break;
            }
        }
    }

    if (!buf) {
        printf("\r\n");
    }

    return len;
}

/* -------------------------------------------------------------------------
 * Values row: voltage (V), current (mA), frequency (Hz), or GND label.
 *
 * The caller must call amux_sweep() before this function in printf mode
 * to ensure fresh ADC readings are available in hw_pin_voltage_ordered[].
 *
 * Returns 0 in buffer mode when no pin has changed (skip the TX update).
 * ------------------------------------------------------------------------- */
uint32_t ui_pin_render_values(char* buf, size_t buf_len, bool refresh) {
    uint32_t len = 0;
    bool any_update = false;

    for (uint i = 0; i < HW_PINS; i++) {
        bool changed = (system_config.pin_changed & (0x01 << (uint8_t)i)) != 0;

        if (buf && changed) {
            len += out_fmt(buf + len, buf_len - len, "\033[8X");
        }

        switch (system_config.pin_func[i]) {

            case BP_PIN_FREQ: {
                /* Freq measurement: always show (it ticks continuously). */
                float freq_val;
                uint8_t freq_units;
                freq_display_hz(&system_config.freq_config[i - 1].period, &freq_val, &freq_units);
                len += out_fmt(buf ? buf + len : NULL, buf ? buf_len - len : 0,
                               "%s%3.1f%s%c\t",
                               ui_term_color_num_float(), freq_val,
                               ui_term_color_reset(),
                               *ui_const_freq_labels_short[freq_units]);
                any_update = true;
                break;
            }

            case BP_PIN_PWM: {
                /* PWM: show frequency only when config changed. */
                if (!changed) {
                    len += out_str(buf ? buf + len : NULL, buf ? buf_len - len : 0, "\t");
                    break;
                }
                float freq_val;
                uint8_t freq_units;
                freq_display_hz(&system_config.freq_config[i - 1].period, &freq_val, &freq_units);
                len += out_fmt(buf ? buf + len : NULL, buf ? buf_len - len : 0,
                               "\033[8X%s%3.1f%s%c\t",
                               ui_term_color_num_float(), freq_val,
                               ui_term_color_reset(),
                               *ui_const_freq_labels_short[freq_units]);
                any_update = true;
                break;
            }

            case BP_PIN_GROUND: {
                if (buf) {
                    if (changed) {
                        len += out_str(buf + len, buf_len - len, GET_T(T_GND));
                        any_update = true;
                    } else {
                        len += out_str(buf + len, buf_len - len, "\t");
                    }
                } else {
                    printf("%s\r%s", GET_T(T_GND), refresh ? "" : "\n");
                    any_update = true;
                }
                break;
            }

            default: {
                /* Voltage reading from the system monitor. */
                char* c;
                if (monitor_get_voltage_ptr(i, &c)) {
                    len += out_fmt(buf ? buf + len : NULL, buf ? buf_len - len : 0,
                                   "%s%s%sV\t",
                                   ui_term_color_num_float(), c, ui_term_color_reset());
                    any_update = true;
                } else {
                    len += out_str(buf ? buf + len : NULL, buf ? buf_len - len : 0, "\t");
                }
                break;
            }
        }
    }

    /* In buffer mode return 0 if nothing changed — caller can skip the TX. */
    return (buf && !any_update) ? 0 : len;
}
