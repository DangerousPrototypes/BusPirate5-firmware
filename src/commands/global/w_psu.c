/**
 * @file w_psu.c
 * @brief Power supply (PSU) control command implementation.
 * @details Implements the w/W commands for controlling the programmable power supply:
 *          - w: Disable PSU
 *          - W: Enable PSU with interactive menu or command-line arguments
 *          
 *          Command syntax:
 *          - W <volts> <mA> [-u <percent>]
 *          - Voltage range: 0.8V to 5.0V
 *          - Current range: 0 to 500mA (0 disables current limiting)
 *          - Undervoltage: 1-100% (100 disables undervoltage protection)
 *          
 *          Features:
 *          - Overcurrent protection with automatic shutdown
 *          - Configurable undervoltage monitoring
 *          - Real-time voltage and current measurement
 *          - Integration with system status bar
 */

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
#include "lib/bp_args/bp_cmd.h"
#include "pirate/psu.h"
#include "pirate/amux.h"
#include "ui/ui_help.h"

const char* const psucmd_usage[] = {
    "w|W\t<v> <i> [-u <%%>]",
    "Disable:%s w",
    "Enable, with menu:%s W",
    "Enable 5v, 50mA fuse, 10%% default undervoltage limit:%s W 5 50",
    "Enable 3.3v, 300mA default fuse, 10%% default undervoltage limit:%s W 3.3",
    "Enable 3.3v, 100mA fuse, 20%% undervoltage limit:%s W 3.3 100 -u 20",
    "Enable 3.3v, no fuse, no undervoltage limit:%s W 3.3 0 -u 100",
};

static const bp_command_opt_t psucmd_opts[] = {
    { "undervoltage", 'u', BP_ARG_REQUIRED, "<%>", T_HELP_GCMD_W_UNDERVOLTAGE },
    { 0 }
};

static const bp_command_positional_t psucmd_enable_positionals[] = {
    { "voltage", "volts", 0, false },
    { "current", "mA",   0, false },
    { 0 }
};

const bp_command_def_t psucmd_enable_def = {
    .name = "W",
    .description = T_CMDLN_PSU_EN,
    .opts = psucmd_opts,
    .positionals      = psucmd_enable_positionals,
    .positional_count = 2,
    .usage = psucmd_usage,
    .usage_count = count_of(psucmd_usage)
};

const bp_command_def_t psucmd_disable_def = {
    .name = "w",
    .description = T_CMDLN_PSU_DIS,
    .usage = psucmd_usage,
    .usage_count = count_of(psucmd_usage)
};

// current limit fuse tripped
void psucmd_irq_callback(void) {
    //psu_disable(); // also sets system_config.psu=
    system_config.error = true;
    system_config.info_bar_changed = true;
    system_pin_update_purpose_and_label(true, BP_VOUT, BP_PIN_VREF, ui_const_pin_states[5]);
}

// zero return code = success
uint32_t psucmd_enable(float volts, float current, bool current_limit_override, uint8_t undervoltage_percent) {
    system_config.pin_labels[0] = 0;
    system_config.pin_changed = 0xff;
    system_pin_update_purpose_and_label(false, BP_VOUT, 0, 0);

    uint32_t psu_result = psu_enable(volts, current, current_limit_override, undervoltage_percent);

    // any error codes starting the PSU?
    if (psu_result != PSU_OK) {
        psucmd_disable();
        return psu_result;
    }

    system_config.info_bar_changed = true;
    system_pin_update_purpose_and_label(true, BP_VOUT, BP_PIN_VOUT, ui_const_pin_states[1]);
    monitor_clear_current(); // reset current so the LCD gets all characters

    return psu_result; // should be PSU OK
}

void psucmd_enable_handler(struct command_result* res) {
    float volts, current;
    bool current_limit_override = false;

    // check help
    if (bp_cmd_help_check(&psucmd_enable_def, res->help_flag)) {
        return;
    }

    // todo: add to system config?
    if (scope_running) { // scope is using the analog subsystem
        printf("Can't turn the power supply on when the scope is using the analog subsystem - use the 'ss' command to "
               "stop the scope\r\n");
        return;
    }

    bool has_volts = bp_cmd_get_positional_float(&psucmd_enable_def, 1, &volts);
    bool has_current = bp_cmd_get_positional_float(&psucmd_enable_def, 2, &current);
    if (has_volts && !has_current) {
        // default current limit when no argument given
        current = 300.0f;
    }

    // user selected to disable PSU when the voltage sags below X % 
    uint32_t undervoltage_percent = 10;
    bool has_undervoltage_alarm = bp_cmd_get_uint32(&psucmd_enable_def, 'u', &undervoltage_percent);

    //if(has_undervoltage_alarm) {
        if(undervoltage_percent < 1 || undervoltage_percent > 100) {
            printf("%sInvalid undervoltage alarm value: 1-100%%%s\r\n", ui_term_color_info(), ui_term_color_reset());
            res->error = true;
            return;
        }
    //}

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
        printf("%sMaximum current (1mA-500mA), 0 for unlimited%s", ui_term_color_info(), ui_term_color_reset());
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

    uint32_t psu_result = psucmd_enable(volts, current, current_limit_override, undervoltage_percent);

    // x.xV requested, closest value: x.xV
    printf("%s%1.2f%sV%s requested, closest value: %s%1.2f%sV\r\n",
           ui_term_color_num_float(),
           psu_status.voltage_requested_float,
           ui_term_color_reset(),
           ui_term_color_info(),
           ui_term_color_num_float(),
           psu_status.voltage_actual_float,
           ui_term_color_reset());

    if (current_limit_override) {
        // current limit disabled
        printf("%s%s: %s%s\r\n",
               ui_term_color_notice(),
               GET_T(T_INFO_CURRENT_LIMIT),
               ui_term_color_reset(),
               GET_T(T_MODE_DISABLED));
    } else {
        // x.xmA requested, closest value: x.xmA
        printf("%s%1.1f%smA%s requested, closest value: %s%3.1f%smA\r\n",
               ui_term_color_num_float(),
               psu_status.current_requested_float,
               ui_term_color_reset(),
               ui_term_color_info(),
               ui_term_color_num_float(),
               psu_status.current_actual_float,
               ui_term_color_reset());
    }

    printf("%sUndervoltage limit: ", ui_term_color_notice());

    if(undervoltage_percent == 100) {
        printf("%s%s\r\n", ui_term_color_reset(), GET_T(T_MODE_DISABLED));
    } else {
        printf("%s%d.%d%sV (%d%%)\r\n",
            ui_term_color_num_float(),
            psu_status.undervoltage_limit_int/10000,
            ( psu_status.undervoltage_limit_int%10000)/100,
            ui_term_color_reset(),
            undervoltage_percent);
    }

    // power supply: enabled
    printf("\r\n%s%s:  %s%s\r\n",
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
    //there is a chance for a race condition if an ADC conversion is ongoing when we disable the PSU
    //this causes a spurious short circuit warning because the ADC reads 0V on VOUT during the disable process
    psu_disable();
    system_pin_update_purpose_and_label(true, BP_VOUT, BP_PIN_VREF, ui_const_pin_states[0]); // change back to vref type pin
    monitor_clear_current(); // reset current so the LCD gets all characters next time
    system_config.info_bar_changed = true;
}

void psucmd_disable_handler(struct command_result* res) {
    // check help
    if (bp_cmd_help_check(&psucmd_disable_def, res->help_flag)) {
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
    psu_init();
    return true;
}

void psucmd_show_clear_error(void) {
    if (psu_status.error_pending && (psu_status.error_overcurrent || psu_status.error_undervoltage)){
        psu_clear_error_flag();
        printf("\033[?5h\r\n");
        if(psu_status.error_overcurrent) {
            ui_help_error(T_PSU_CURRENT_LIMIT_ERROR);
        }
        if(psu_status.error_undervoltage) {
            ui_help_error(T_PSU_SHORT_ERROR);
        }
        busy_wait_ms(500);
        printf("\033[?5l");
    }
}
