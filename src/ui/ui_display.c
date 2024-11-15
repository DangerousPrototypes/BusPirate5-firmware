#include <stdint.h>
#include "pico/stdlib.h"
#include "pirate.h"
#include "system_config.h"
#include "command_struct.h"
#include "bytecode.h"
#include "displays.h"
#include "ui/ui_prompt.h"
#include "ui/ui_parse.h"
#include "ui/ui_term.h"
#include "ui/ui_const.h"
#include "ui/ui_display.h"
#include "ui/ui_cmdln.h"

bool ui_display_list(const struct ui_prompt* menu) {
    for (uint8_t i = 0; i < (*menu).menu_items_count; i++) {
        printf(" %d. %s%s%s\r\n", i + 1, ui_term_color_info(), displays[i].display_name, ui_term_color_reset());
    }
}

void ui_display_enable_args(struct command_result* res) {
    uint32_t display;
    bool error;
    bool has_value = cmdln_args_uint32_by_position(1, &display);
    if (!has_value || ((display) > MAXDISPLAY) || ((display) == 0)) {
        if (has_value && (display) > MAXDISPLAY) {
            ui_prompt_invalid_option();
        }
        error = true;
    } else {
        (display)--; // adjust down one from user choice
        error = false;
    }

    if (error) // no integer found
    {
        static const struct ui_prompt_config cfg = {
            true,                            // bool allow_prompt_text;
            false,                           // bool allow_prompt_defval;
            false,                           // bool allow_defval;
            true,                            // bool allow_exit;
            &ui_display_list,                // bool (*menu_print)(const struct ui_prompt* menu);
            &ui_prompt_prompt_ordered_list,  // bool (*menu_prompt)(const struct ui_prompt* menu);
            &ui_prompt_validate_ordered_list // bool (*menu_validate)(const struct ui_prompt* menu, uint32_t* value);
        };

        static const struct ui_prompt display_menu = {
            T_MODE_DISPLAY_SELECTION, 0, MAXDISPLAY, T_MODE_DISPLAY, 0, 0, 0, 0, &cfg
        };

        prompt_result result;
        ui_prompt_uint32(&result, &display_menu, &display);
        if (result.exit) // user bailed, stay in current mode
        {
            //(*response).error=true;
            return;
        }
        display--;
    }

    // ok, start setup dialog
    displays[system_config.display].display_cleanup(); // switch to HiZ
    if (!displays[display].display_setup())            // user bailed on setup steps
    {
        system_config.display = 0; // switch to default
        displays[system_config.display].display_setup();
        displays[system_config.display].display_setup_exc();
        //(*response).error=true;
        printf("\r\n%s%s:%s %s",
               ui_term_color_info(),
               GET_T(T_MODE_DISPLAY),
               ui_term_color_reset(),
               displays[system_config.display].display_name);
        return;
    }

    system_config.display = display;                     // setup the new mode
    displays[system_config.display].display_setup_exc(); // execute the mode setup

    if (system_config.mode == 0) // TODO: do something to show the mode (LED? LCD?)
    {
        // gpio_clear(BP_MODE_LED_PORT, BP_MODE_LED_PIN);
    } else {
        // gpio_set(BP_MODE_LED_PORT, BP_MODE_LED_PIN);
    }

    printf("\r\n%s%s:%s %s",
           ui_term_color_info(),
           GET_T(T_MODE_DISPLAY),
           ui_term_color_reset(),
           displays[system_config.display].display_name);
}

extern bool int_display_menu(const struct ui_prompt* menu);

// set display mode  (hex, bin, octa, dec)
void ui_display_int_display_format(struct command_result* res) {
    uint32_t display;
    bool error;

    prompt_result result;
    ui_parse_get_attributes(&result, &display, 1);
    printf("result.error=%d result.no_value=%d result.exit=%d display=%d\n",
           result.error,
           result.no_value,
           result.exit,
           display);

    if (result.error || result.no_value || result.exit || ((display) > count_of(ui_const_display_formats)) ||
        ((display) == 0)) {
        if (result.success && (display) > count_of(ui_const_display_formats)) {
            ui_prompt_invalid_option();
        }
        error = 1;
    } else {
        (display)--; // adjust down one from user choice
        error = 0;
    }

    if (error) // no integer found
    {
        static const struct ui_prompt_config cfg = {
            true,                            // bool allow_prompt_text;
            false,                           // bool allow_prompt_defval;
            false,                           // bool allow_defval;
            true,                            // bool allow_exit;
            &int_display_menu,               // bool (*menu_print)(const struct ui_prompt* menu);
            &ui_prompt_prompt_ordered_list,  // bool (*menu_prompt)(const struct ui_prompt* menu);
            &ui_prompt_validate_ordered_list // bool (*menu_validate)(const struct ui_prompt* menu, uint32_t* value);
        };

        static const struct ui_prompt display_menu = {
            T_MODE_NUMBER_DISPLAY_FORMAT, 0, count_of(ui_const_display_formats), T_MODE_DISPLAY, 0, 0, 0, 0, &cfg
        };

        prompt_result result;
        ui_prompt_uint32(&result, &display_menu, &display);
        if (result.exit) // user bailed
        {
            (*res).error = true;
            return;
        }
        display--;
    }

    system_config.display_format = (uint8_t)display;

    printf("\r\n%s%s:%s %s",
           ui_term_color_info(),
           GET_T(T_MODE_DISPLAY),
           ui_term_color_reset(),
           ui_const_display_formats[system_config.display_format]);
}

/* For Emacs:
 * Local Variables:
 * mode:c
 * indent-tabs-mode:t
 * tab-width:4
 * c-basic-offset:4
 * End:
 * For VIM:
 * vim:set softtabstop=4 shiftwidth=4 tabstop=4:
 */
