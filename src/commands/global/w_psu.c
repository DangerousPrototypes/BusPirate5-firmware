#include <stdio.h>
#include "pico/stdlib.h"
#include "pirate.h"
#include "command_struct.h"
#include "system_config.h"
#include "ui/ui_term.h"
#include "ui/ui_prompt.h"
#include "ui/ui_const.h"
#include "system_monitor.h"
#include "commands/global/w_psu.h"
#include "display/scope.h"
#include "ui/ui_cmdln.h"
#include "pirate/psu.h"
#include "ui/ui_help.h"

const char* const psucmd_usage[] = {
    "w|W\t<v> <i>",
    "Disable: w",
    "Enable, with menu: W",
    "Enable 5v, 50mA limit: W 5 50",
    "Enable 3.3v, 300mA default limit: W 3.3",
    "Enable 3.3v, no limit: W 3.3 0",
};

const struct ui_help_options psucmd_options[] = {
    { 1, "", T_HELP_GCMD_W }, // command help
    { 0, "w", T_HELP_GCMD_W_DISABLE }, { 0, "W", T_HELP_GCMD_W_ENABLE },
    { 0, "<v>", T_HELP_GCMD_W_VOLTS }, { 0, "<i>", T_HELP_GCMD_W_CURRENT_LIMIT },
};

// current limit fuse tripped
void psucmd_irq_callback(void) {
    psu_disable(); // also sets system_config.psu=0
    system_config.psu_irq_en = false;
    system_config.psu_current_error = true;
    system_config.psu_error = true;
    system_config.error = true;
    system_config.info_bar_changed = true;
    system_pin_update_purpose_and_label(true, BP_VOUT, BP_PIN_VREF, ui_const_pin_states[5]);
}

// zero return code = success
uint32_t psucmd_enable(float volts, float current, bool current_limit_override) {
    system_config.psu = 0;
    system_config.pin_labels[0] = 0;
    system_config.pin_changed = 0xff;
    system_pin_update_purpose_and_label(false, BP_VOUT, 0, 0);

    uint32_t psu_result = psu_enable(volts, current, current_limit_override);

    // any error codes starting the PSU?
    if (psu_result != PSU_OK) {
        psucmd_disable();
        return psu_result;
    }

    system_config.psu_voltage = psu_status.voltage_actual_i;
    system_config.psu_current_limit_en = !current_limit_override;
    if (!current_limit_override) {
        system_config.psu_current_limit = psu_status.current_actual_i;
    }

    // todo: consistent interface to each label of toolbar and LCD, including vref/vout
    system_config.psu = 1;
    system_config.psu_error = false;
    system_config.psu_current_error = false;
    system_config.info_bar_changed = true;
    system_pin_update_purpose_and_label(true, BP_VOUT, BP_PIN_VOUT, ui_const_pin_states[1]);
    monitor_clear_current(); // reset current so the LCD gets all characters

    // since we dont have any more pins, the over current detect system is read through the
    // 4067 and ADC. It will be picked up in the second core loop
    system_config.psu_irq_en = system_config.psu_current_limit_en;

    return psu_result; // should be PSU OK
}

void psucmd_enable_handler(struct command_result* res) {
    float volts, current;
    bool current_limit_override = false;

    // check help
    if (ui_help_show(
            res->help_flag, psucmd_usage, count_of(psucmd_usage), &psucmd_options[0], count_of(psucmd_options))) {
        return;
    }

    // todo: add to system config?
    if (scope_running) { // scope is using the analog subsystem
        printf("Can't turn the power supply on when the scope is using the analog subsystem - use the 'ss' command to "
               "stop the scope\r\n");
        return;
    }

    bool has_volts = cmdln_args_float_by_position(1, &volts);
    bool has_current = cmdln_args_float_by_position(2, &current);
    if (has_volts && !has_current) {
        // default current limit when no argument given
        current = 300.0f;
    }

    if (!has_volts || volts < 0.8f || volts > 5.0f || (has_current && (current < 0.0f || current > 500.0f))) {
        prompt_result result;

        // prompt voltage (float)
        printf("%sPower supply\r\nVolts (0.80V-5.00V)%s", ui_term_color_info(), ui_term_color_reset());
        ui_prompt_float(&result, 0.8f, 5.0f, 3.3f, true, &volts, false);
        if (result.exit) {
            res->error = true;
            return;
        }

        // prompt current (float)
        printf("%sMaximum current (0mA-500mA), <enter> for 300mA or 0 for unlimited%s", ui_term_color_info(), ui_term_color_reset());
        ui_prompt_float(&result, 0.0f, 500.0f, 300.0f, true, &current, false);
        if (result.exit) {
            res->error = true;
            return;
        }
    }
    // explicit 0 for current means to disable the overcurrent limiting
    if (current == 0.0f) {
        current_limit_override = true;
    }

    uint32_t psu_result = psucmd_enable(volts, current, current_limit_override);

    // x.xV requested, closest value: x.xV
    printf("%s%1.2f%sV%s requested, closest value: %s%1.2f%sV\r\n",
           ui_term_color_num_float(),
           psu_status.voltage_requested,
           ui_term_color_reset(),
           ui_term_color_info(),
           ui_term_color_num_float(),
           psu_status.voltage_actual,
           ui_term_color_reset());

    if (current_limit_override) {
        // current limit disabled
        printf("%s%s:%s%s\r\n",
               ui_term_color_notice(),
               GET_T(T_INFO_CURRENT_LIMIT),
               ui_term_color_reset(),
               GET_T(T_MODE_DISABLED));
    } else {
        // x.xmA requested, closest value: x.xmA
        printf("%s%1.1f%smA%s requested, closest value: %s%3.1f%smA\r\n",
               ui_term_color_num_float(),
               psu_status.current_requested,
               ui_term_color_reset(),
               ui_term_color_info(),
               ui_term_color_num_float(),
               psu_status.current_actual,
               ui_term_color_reset());
    }

    // power supply: enabled
    printf("\r\n%s%s:%s%s\r\n",
           ui_term_color_notice(),
           GET_T(T_MODE_POWER_SUPPLY),
           ui_term_color_reset(),
           GET_T(T_MODE_ENABLED));

    // any error codes starting the PSU?
    if (psu_result != PSU_OK) {
        switch (psu_result) {
            case PSU_ERROR_FUSE_TRIPPED:
                ui_help_error(T_PSU_CURRENT_LIMIT_ERROR);
                break;
            case PSU_ERROR_VOUT_LOW:
                ui_help_error(T_PSU_SHORT_ERROR);
                break;
            case PSU_ERROR_BACKFLOW:
                printf("%s\r\nError: Vout > on-board power supply. Backflow prevention activated\r\n\tIs an external "
                       "voltage connected to Vout/Vref pin?\r\n%s",
                       ui_term_color_warning(),
                       ui_term_color_reset());
                break;
        }
        res->error = true;
        return;
    }

    // Vreg output: x.xV, Vref/Vout pin: x.xV, Current sense: x.xmA
    uint32_t vout, isense, vreg;
    bool fuse;
    psu_measure(&vout, &isense, &vreg, &fuse);
    printf("%sVreg output: %s%d.%d%sV%s, Vref/Vout pin: %s%d.%d%sV%s, Current: %s%d.%d%smA%s\r\n%s",
           ui_term_color_notice(),
           ui_term_color_num_float(),
           ((vreg) / 1000),
           (((vreg) % 1000) / 100),
           ui_term_color_reset(),
           ui_term_color_notice(),
           ui_term_color_num_float(),
           ((vout) / 1000),
           (((vout) % 1000) / 100),
           ui_term_color_reset(),
           ui_term_color_notice(),
           ui_term_color_num_float(),
           (isense / 1000),
           ((isense % 1000) / 100),
           ui_term_color_reset(),
           ui_term_color_notice(),
           ui_term_color_reset());
}

// cleanup on mode exit, etc
void psucmd_disable(void) {
    system_config.psu_irq_en = false;
    psu_disable();
    system_config.psu_error = false;
    system_config.psu_current_error = false;
    system_config.psu = 0;
    system_config.info_bar_changed = true;
    monitor_clear_current(); // reset current so the LCD gets all characters next time
    system_pin_update_purpose_and_label(true, BP_VOUT, BP_PIN_VREF, ui_const_pin_states[0]); // change back to vref type pin
}

void psucmd_disable_handler(struct command_result* res) {
    // check help
    if (ui_help_show(
            res->help_flag, psucmd_usage, count_of(psucmd_usage), &psucmd_options[0], count_of(psucmd_options))) {
        return;
    }

    psucmd_disable();
    printf("%s%s: %s%s\r\n",
           ui_term_color_notice(),
           GET_T(T_MODE_POWER_SUPPLY),
           ui_term_color_reset(),
           GET_T(T_MODE_DISABLED));
}

bool psucmd_init(void) {
    system_config.psu = 0;
    system_config.psu_error = true;
    psu_init();
    system_config.psu_error = false;
    return true;
}

void psucmd_over_current(void) {
    if (system_config.psu_current_error) {
        printf("\033[?5h\r\n");
        ui_help_error(T_PSU_CURRENT_LIMIT_ERROR);
        busy_wait_ms(500);
        printf("\033[?5l");
        system_config.psu_current_error = 0;
    }
}
