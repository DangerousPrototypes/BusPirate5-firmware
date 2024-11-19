#include <stdio.h>
#include "pico/stdlib.h"
#include <stdint.h>
#include "pirate.h"
#include "system_config.h"
#include "command_struct.h"
#include "ui/ui_const.h"
#include "ui/ui_term.h"
#include "usb_tx.h"
#include "commands/global/freq.h"
#include "ui/ui_flags.h"
#include "system_monitor.h"
#include "display/scope.h"

uint32_t ui_statusbar_info(char* buf, size_t buffLen) {
    uint32_t len = 0;
    uint32_t temp = 0;
    uint32_t cnt = 0;

    len += ui_term_color_text_background_buf(&buf[len], buffLen - len, 0x000000, BP_COLOR_GREY);

    if (system_config.psu) {
        temp = snprintf(&buf[len],
                        buffLen - len,
                        "Vout: %u.%uV",
                        (system_config.psu_voltage) / 10000,
                        ((system_config.psu_voltage) % 10000) / 100);
        len += temp;
        cnt += temp;
        if (system_config.psu_current_limit_en) {
            temp = snprintf(&buf[len],
                            buffLen - len,
                            "/%u.%umA max",
                            (system_config.psu_current_limit) / 10000,
                            ((system_config.psu_current_limit) % 10000) / 100);
            len += temp;
            cnt += temp;
        }
        temp = snprintf(&buf[len], buffLen - len, " | ");
        len += temp;
        cnt += temp;
    }

    if (system_config.psu_error) {
        // show Power Supply: ERROR
        temp = snprintf(&buf[len],
                        buffLen - len,
                        "Vout: ERROR > %u.%umA | ",
                        (system_config.psu_current_limit) / 10000,
                        ((system_config.psu_current_limit) % 10000) / 100);
        len += temp;
        cnt += temp;
    }

    if (system_config.pullup_enabled) {
        // show Pull-up resistors ON
        temp = snprintf(&buf[len],
                        buffLen - len,
                        "Pull-ups: ON | ",
                        (system_config.psu_voltage) / 10000,
                        ((system_config.psu_voltage) % 10000) / 100,
                        (system_config.psu_current_limit) / 10000,
                        ((system_config.psu_current_limit) % 10000) / 100);
        len += temp;
        cnt += temp;
    }
    if (scope_running) { // scope is using the analog subsystem
        temp = snprintf(&buf[len], buffLen - len, "V update slower when scope running");
        len += temp;
        cnt += temp;
    }
    // fill in blank space
    len += snprintf(&buf[len], buffLen - len, "\e[%dX", system_config.terminal_ansi_columns - cnt);
    len += snprintf(&buf[len], buffLen - len, "%s", ui_term_color_reset()); // snprintf to buffer
    return len;
}

// show voltages/pinstates
uint32_t ui_statusbar_names(char* buf, size_t buffLen) {
    uint32_t len = 0;
    // pin list
    for (int i = 0; i < HW_PINS; i++) {
        len += ui_term_color_text_background_buf(
            &buf[len], buffLen - len, hw_pin_label_ordered_color[i][0], hw_pin_label_ordered_color[i][1]);
        len += snprintf(&buf[len], buffLen - len, "\e[8X"); // clear existing
        len += snprintf(&buf[len], buffLen - len, "%d.%s\t", i + 1, hw_pin_label_ordered[i]);
    }

    len += snprintf(&buf[len], buffLen - len, "%s", ui_term_color_reset());
    return len;
}

bool label_default(uint32_t* len, size_t buffLen, char* buf, uint32_t i) {
    if (system_config.pin_changed & (0x01 << (uint8_t)i)) {
        *len += snprintf(&buf[*len],
                         buffLen - *len,
                         "\e[8X%s\t",
                         system_config.pin_labels[i] == 0 ? "-" : (char*)system_config.pin_labels[i]);
        return true;
    }
    return false;
}

bool label_current(uint32_t* len, size_t buffLen, char* buf, uint32_t i) {
    // uint32_t isense;
    char* c;
    if (monitor_get_current_ptr(&c) || (system_config.pin_changed & (0x01 << (uint8_t)i))) {
        *len += snprintf(
            &buf[*len], buffLen - *len, "\e[8X%s%s%smA\t", ui_term_color_num_float(), c, ui_term_color_reset());
        return true;
    }
    return false;
}

bool value_voltage(uint32_t* len, size_t buffLen, char* buf, uint32_t i) {
    char* c;
    if (monitor_get_voltage_ptr(i, &c)) {
        *len += snprintf(&buf[*len], buffLen - *len, "%s%s%sV\t", ui_term_color_num_float(), c, ui_term_color_reset());
        return true;
    }

    return false;
}

// TODO: freq function (update on change), pwm function (write once, untill update)
bool value_freq(uint32_t* len, size_t buffLen, char* buf, uint32_t i) {
    *len += snprintf(&buf[*len], buffLen - *len, "\e[8X"); // clear out tab, return to tab
    float freq_friendly_value;
    uint8_t freq_friendly_units;
    freq_display_hz(&system_config.freq_config[i - 1].period, &freq_friendly_value, &freq_friendly_units);
    *len += snprintf(&buf[*len],
                     buffLen - *len,
                     "%s%3.1f%s%c\t",
                     ui_term_color_num_float(),
                     freq_friendly_value,
                     ui_term_color_reset(),
                     *ui_const_freq_labels_short[freq_friendly_units]);
    return true;
}

bool value_pwm(uint32_t* len, size_t buffLen, char* buf, uint32_t i) {

    if (!(system_config.pin_changed & (0x01 << (uint8_t)(i)))) {
        return false;
    }

    return value_freq(len, buffLen, buf, i);
}

bool value_ground(uint32_t* len, size_t buffLen, char* buf, uint32_t i) {
    if (!(system_config.pin_changed & (0x01 << (uint8_t)(i)))) {
        return false;
    }

    *len += snprintf(&buf[*len], buffLen - *len, "%s", GET_T(T_GND));
    return true;
}
struct _iopins {
    // bool (*name)(uint32_t *len, char *buf, uint32_t i);
    bool (*label)(uint32_t* len, size_t buffLen, char* buf, uint32_t i);
    bool (*value)(uint32_t* len, size_t buffLen, char* buf, uint32_t i);
};

const struct _iopins ui_statusbar_pin_functions[] = {
    [BP_PIN_IO] = { &label_default, &value_voltage },    [BP_PIN_MODE] = { &label_default, &value_voltage },
    [BP_PIN_PWM] = { &label_default, &value_pwm },       [BP_PIN_FREQ] = { &label_default, &value_freq },
    [BP_PIN_VREF] = { &label_default, &value_voltage },  [BP_PIN_VOUT] = { &label_current, &value_voltage },
    [BP_PIN_GROUND] = { &label_default, &value_ground }, [BP_PIN_DEBUG] = { &label_default, &value_voltage }

};

uint32_t ui_statusbar_labels(char* buf, size_t buffLen) {
    uint32_t len = 0;
    uint8_t j = 0;

    // show state of IO pins
    for (uint i = 0; i < HW_PINS; i++) {

        if (system_config.pin_changed & (0x01 << (uint8_t)i)) {
            len += snprintf(&buf[len], buffLen - len, "\e[8X"); // clear out tab, return to tab
        }

        if (!ui_statusbar_pin_functions[system_config.pin_func[i]].label(&len, buffLen, buf, i)) {
            len += snprintf(&buf[len], buffLen - len, "\t"); // todo: just handle this
        }
    }

    return len;
}

uint32_t ui_statusbar_value(char* buf, size_t buffLen) {
    uint32_t len = 0;

    bool do_update = false;

    // show state of IO pins
    for (uint i = 0; i < HW_PINS; i++) {

        if (system_config.pin_changed & (0x01 << (uint8_t)i)) {
            len += snprintf(&buf[len], buffLen - len, "\e[8X"); // clear out tab, return to tab
        }

        if (ui_statusbar_pin_functions[system_config.pin_func[i]].value(&len, buffLen, buf, i)) {
            do_update = true;
        } else {
            len += snprintf(&buf[len], buffLen - len, "\t"); // todo: just handle this
        }
    }

    return (do_update ? len : 0);
}

void ui_statusbar_update(uint32_t update_flags) {
    uint32_t len = 0;
    size_t buffLen = sizeof(tx_sb_buf);

    if (!update_flags) // nothing to update
    {
        return;
    }

    // save cursor, hide cursor
    len += snprintf(&tx_sb_buf[len], buffLen - len, "\e7\e[?25l");

    // print each line of the toolbar
    if (update_flags & UI_UPDATE_INFOBAR) {
        monitor_force_update(); // we want to repaint the whole screen if we're doing the pin names...
        len += snprintf(&tx_sb_buf[len],
                        buffLen - len,
                        "\e[%d;0H",
                        system_config.terminal_ansi_rows - 3); // position at row-3 col=0
        len += ui_statusbar_info(&tx_sb_buf[len], buffLen - len);
    }

    if (update_flags & UI_UPDATE_NAMES) {
        len += snprintf(&tx_sb_buf[len], buffLen - len, "\e[%d;0H", system_config.terminal_ansi_rows - 2);
        len += ui_statusbar_names(&tx_sb_buf[len], buffLen - len);
    }

    if ((update_flags & UI_UPDATE_CURRENT) && !(update_flags & UI_UPDATE_LABELS)) // show current under Vout
    {
        char* c;
        if (monitor_get_current_ptr(&c)) {
            len += snprintf(&tx_sb_buf[len],
                            buffLen - len,
                            "\e[%d;0H%s%s%smA",
                            system_config.terminal_ansi_rows - 1,
                            ui_term_color_num_float(),
                            c,
                            ui_term_color_reset());
        }
    }

    if (update_flags & UI_UPDATE_LABELS) {
        len += snprintf(&tx_sb_buf[len], buffLen - len, "\e[%d;0H", system_config.terminal_ansi_rows - 1);
        len += ui_statusbar_labels(&tx_sb_buf[len], buffLen - len);
    }

    if (update_flags & UI_UPDATE_VOLTAGES) {
        len += snprintf(&tx_sb_buf[len], buffLen - len, "\e[%d;0H", system_config.terminal_ansi_rows - 0);
        len += ui_statusbar_value(&tx_sb_buf[len], buffLen - len);
    }

    // restore cursor, show cursor
    len += snprintf(&tx_sb_buf[len], buffLen - len, "\e8");

    if (!system_config.terminal_hide_cursor) {
        len += snprintf(&tx_sb_buf[len], buffLen - len, "\e[?25h");
    }

    tx_sb_start(len);
}

void ui_statusbar_init(void) {
    if (system_config.terminal_ansi_color && system_config.terminal_ansi_statusbar) {
        printf("\e[%d;%dr", 1, system_config.terminal_ansi_rows - 4);
    }
}

void ui_statusbar_deinit(void) {
    if (system_config.terminal_ansi_color && system_config.terminal_ansi_statusbar) {
        printf("\e7\e[0;%dr", system_config.terminal_ansi_rows); // save cursor, disable region block
        for (uint8_t i = 0; i < 4; i++) {
            printf("\e[%d;0H\e[K", system_config.terminal_ansi_rows - i); // clear screen
        }
        printf("\e8"); // restore cursor
    }
}
