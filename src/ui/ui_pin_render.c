/**
 * @file ui_pin_render.c
 * @brief Unified buffer-only pin name / label / voltage rendering.
 * @details Always writes into a caller-provided buffer via snprintf — no
 *          printf path.  Behaviour is controlled through pin_render_flags_t.
 *
 *          Core0 (v/V command) and Core1 (statusbar) each pass their own
 *          buffer.  Core0 pushes the result through tx_fifo_write(); Core1
 *          commits via tx_sb_start().
 *
 *          The builder reads raw data directly:
 *           - Voltages:  *hw_pin_voltage_ordered[i]  (millivolts)
 *           - Current:   hw_adc_raw[HW_ADC_CURRENT_SENSE]
 *           - Freq/PWM:  freq_display_hz()
 *
 *          Change tracking (PIN_RENDER_CHANGE_TRACK) uses a static shadow
 *          buffer.  Only Core1 (statusbar) sets this flag — no lock needed.
 */

#include <stdio.h>
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
#include "pirate/psu.h"
#include "pirate/amux.h"
#include "ui/ui_pin_render.h"

/* -------------------------------------------------------------------------
 * Shadow buffers for change-tracking (PIN_RENDER_CHANGE_TRACK).
 * Only Core1 (statusbar) uses CHANGE_TRACK, so these are single-writer.
 * ------------------------------------------------------------------------- */
static uint16_t shadow_voltage_mv[HW_PINS];
static uint32_t shadow_current_raw;

/* -------------------------------------------------------------------------
 * Remaining-space helper — guards against snprintf overflow.
 * ------------------------------------------------------------------------- */
#define REM(len, total) ((len) < (total) ? (total) - (len) : 0u)

/* -------------------------------------------------------------------------
 * Names row:  "1.IO0  2.IO1  ..."
 * ------------------------------------------------------------------------- */
uint32_t ui_pin_render_names(char* buf, size_t buf_len, pin_render_flags_t flags) {
    uint32_t len = 0;

    for (int i = 0; i < HW_PINS; i++) {
        len += (uint32_t)ui_term_color_text_background_buf(
            buf + len, REM(len, buf_len),
            hw_pin_label_ordered_color[i][0],
            hw_pin_label_ordered_color[i][1]);

        if (flags & PIN_RENDER_CLEAR_CELLS) {
            len += ui_term_erase_chars_buf(buf + len, REM(len, buf_len), 8);
        }
        len += (uint32_t)snprintf(buf + len, REM(len, buf_len),
                                  "%d.%s\t", i + 1, hw_pin_label_ordered[i]);
    }

    len += (uint32_t)snprintf(buf + len, REM(len, buf_len), "%s", ui_term_color_reset());

    if (flags & PIN_RENDER_NEWLINE) {
        len += (uint32_t)snprintf(buf + len, REM(len, buf_len), "\r\n");
    }

    return len;
}

/* -------------------------------------------------------------------------
 * Labels row: current mA for Vout, mode labels for other pins.
 * ------------------------------------------------------------------------- */
uint32_t ui_pin_render_labels(char* buf, size_t buf_len, pin_render_flags_t flags) {
    uint32_t len = 0;
    bool track = (flags & PIN_RENDER_CHANGE_TRACK) != 0;

    for (uint i = 0; i < HW_PINS; i++) {
        bool changed = (system_config.pin_changed & (0x01 << (uint8_t)i)) != 0;

        switch (system_config.pin_func[i]) {

            case BP_PIN_VOUT: {
                if (psu_status.enabled) {
                    uint32_t raw = hw_adc_raw[HW_ADC_CURRENT_SENSE];
                    bool current_changed = (raw != shadow_current_raw);

                    if (track && !changed && !current_changed) {
                        len += (uint32_t)snprintf(buf + len, REM(len, buf_len), "\t");
                        break;
                    }

                    if (track) {
                        shadow_current_raw = raw;
                    }

                    uint32_t isense = ((raw >> 1) * ((500 * 1000) / 2048));

                    if (flags & PIN_RENDER_CLEAR_CELLS) {
                        len += ui_term_erase_chars_buf(buf + len, REM(len, buf_len), 8);
                    }
                    len += (uint32_t)snprintf(buf + len, REM(len, buf_len),
                                              "%s%03u.%01u%smA\t",
                                              ui_term_color_num_float(),
                                              (isense / 1000),
                                              ((isense % 1000) / 100),
                                              ui_term_color_reset());
                } else {
                    if (track && !changed) {
                        len += (uint32_t)snprintf(buf + len, REM(len, buf_len), "\t");
                    } else {
                        if (flags & PIN_RENDER_CLEAR_CELLS) {
                            len += ui_term_erase_chars_buf(buf + len, REM(len, buf_len), 8);
                        }
                        len += (uint32_t)snprintf(buf + len, REM(len, buf_len), "-\t");
                    }
                }
                break;
            }

            default: {
                const char* lbl = system_config.pin_labels[i] == 0
                                       ? "-"
                                       : (const char*)system_config.pin_labels[i];
                if (track && !changed) {
                    len += (uint32_t)snprintf(buf + len, REM(len, buf_len), "\t");
                } else {
                    if (flags & PIN_RENDER_CLEAR_CELLS) {
                        len += ui_term_erase_chars_buf(buf + len, REM(len, buf_len), 8);
                    }
                    len += (uint32_t)snprintf(buf + len, REM(len, buf_len), "%s\t", lbl);
                }
                break;
            }
        }
    }

    if (flags & PIN_RENDER_NEWLINE) {
        len += (uint32_t)snprintf(buf + len, REM(len, buf_len), "\r\n");
    }

    return len;
}

/* -------------------------------------------------------------------------
 * Values row: voltage (V), frequency (Hz), PWM, GND, or current (mA).
 *
 * The caller must call amux_sweep() before this function to ensure fresh
 * ADC readings are available in hw_pin_voltage_ordered[].
 *
 * Returns 0 when CHANGE_TRACK is set and nothing changed — caller can
 * skip the TX.
 * ------------------------------------------------------------------------- */
uint32_t ui_pin_render_values(char* buf, size_t buf_len, pin_render_flags_t flags) {
    uint32_t len = 0;
    bool track = (flags & PIN_RENDER_CHANGE_TRACK) != 0;
    bool any_update = false;

    for (uint i = 0; i < HW_PINS; i++) {
        bool changed = (system_config.pin_changed & (0x01 << (uint8_t)i)) != 0;

        switch (system_config.pin_func[i]) {

            case BP_PIN_FREQ: {
                /* Freq measurement: always show (it ticks continuously). */
                float freq_val;
                uint8_t freq_units;
                freq_display_hz(&system_config.freq_config[i - 1].period, &freq_val, &freq_units);

                if (flags & PIN_RENDER_CLEAR_CELLS) {
                    len += ui_term_erase_chars_buf(buf + len, REM(len, buf_len), 8);
                }
                len += (uint32_t)snprintf(buf + len, REM(len, buf_len),
                                          "%s%3.1f%s%c\t",
                                          ui_term_color_num_float(), freq_val,
                                          ui_term_color_reset(),
                                          *ui_const_freq_labels_short[freq_units]);
                any_update = true;
                break;
            }

            case BP_PIN_PWM: {
                /* PWM: static until reconfigured. With change-tracking, skip if unchanged. */
                if (track && !changed) {
                    len += (uint32_t)snprintf(buf + len, REM(len, buf_len), "\t");
                    break;
                }
                float freq_val;
                uint8_t freq_units;
                freq_display_hz(&system_config.freq_config[i - 1].period, &freq_val, &freq_units);

                if (flags & PIN_RENDER_CLEAR_CELLS) {
                    len += ui_term_erase_chars_buf(buf + len, REM(len, buf_len), 8);
                }
                len += (uint32_t)snprintf(buf + len, REM(len, buf_len),
                                          "%s%3.1f%s%c\t",
                                          ui_term_color_num_float(), freq_val,
                                          ui_term_color_reset(),
                                          *ui_const_freq_labels_short[freq_units]);
                any_update = true;
                break;
            }

            case BP_PIN_GROUND: {
                if (track && !changed) {
                    len += (uint32_t)snprintf(buf + len, REM(len, buf_len), "\t");
                } else {
                    if (flags & PIN_RENDER_CLEAR_CELLS) {
                        len += ui_term_erase_chars_buf(buf + len, REM(len, buf_len), 8);
                    }
                    len += (uint32_t)snprintf(buf + len, REM(len, buf_len), "%s", GET_T(T_GND));
                    any_update = true;
                }
                break;
            }

            default: {
                /* Voltage — read raw millivolts from hw_pin_voltage_ordered[]. */
                uint16_t mv = (uint16_t)(*hw_pin_voltage_ordered[i]);

                if (track) {
                    if (mv == shadow_voltage_mv[i] && !changed) {
                        len += (uint32_t)snprintf(buf + len, REM(len, buf_len), "\t");
                        break;
                    }
                    shadow_voltage_mv[i] = mv;
                }

                if (flags & PIN_RENDER_CLEAR_CELLS) {
                    len += ui_term_erase_chars_buf(buf + len, REM(len, buf_len), 8);
                }
                len += (uint32_t)snprintf(buf + len, REM(len, buf_len),
                                          "%s%d.%d%sV\t",
                                          ui_term_color_num_float(),
                                          mv / 1000,
                                          (mv % 1000) / 100,
                                          ui_term_color_reset());
                any_update = true;
                break;
            }
        }
    }

    if ((flags & PIN_RENDER_NEWLINE) && any_update) {
        len += (uint32_t)snprintf(buf + len, REM(len, buf_len), "\r\n");
    }

    /* With CHANGE_TRACK, return 0 if nothing changed — caller can skip TX. */
    return (track && !any_update) ? 0 : len;
}
