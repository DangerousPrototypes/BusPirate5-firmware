#include <stdint.h>
#include "pico/stdlib.h"
#include "pirate.h"
#include "system_config.h"
#include "command_struct.h"
#include "bytecode.h"
#include "modes.h"
#include "ui/ui_prompt.h"
#include "ui/ui_parse.h"
#include "ui/ui_term.h"
#include "ui/ui_const.h"
#include "ui/ui_cmdln.h"
#include "binmode/binmodes.h"
#include "binmode/fala.h"

bool ui_mode_list(const struct ui_prompt* menu) {
    for (uint i = 0; i < (*menu).menu_items_count; i++) {
        printf(" %d. %s%s%s\r\n", i + 1, ui_term_color_info(), modes[i].protocol_name, ui_term_color_reset());
    }
}

void ui_mode_enable_args(struct command_result* res) {
    uint32_t mode;
    bool error;

    bool has_value = cmdln_args_uint32_by_position(1, &mode);

    if (!has_value || ((mode) > MAXPROTO) || ((mode) == 0)) {
        if (has_value && (mode) > MAXPROTO) {
            ui_prompt_invalid_option();
        }
        error = true;
    } else {
        (mode)--; // adjust down one from user choice
        error = false;
    }

    if (error) { // no integer found

        static const struct ui_prompt_config cfg = {
            true,                            // bool allow_prompt_text;
            false,                           // bool allow_prompt_defval;
            false,                           // bool allow_defval;
            true,                            // bool allow_exit;
            &ui_mode_list,                   // bool (*menu_print)(const struct ui_prompt* menu);
            &ui_prompt_prompt_ordered_list,  // bool (*menu_prompt)(const struct ui_prompt* menu);
            &ui_prompt_validate_ordered_list // bool (*menu_validate)(const struct ui_prompt* menu, uint32_t* value);
        };

        static const struct ui_prompt mode_menu = {
            T_MODE_MODE_SELECTION, 0, count_of(modes), T_MODE_MODE, 0, 0, 0, 0, &cfg
        };

        prompt_result result;
        ui_prompt_uint32(&result, &mode_menu, &mode);
        if (result.exit) { // user bailed, stay in current mode
            //(*response).error=true;
            return;
        }
        mode--;
    }

    // ok, start setup dialog
    if (!modes[mode].protocol_setup()) { // user bailed on setup steps
        //(*response).error=true;
        return;
    }

    modes[system_config.mode].protocol_cleanup();   // switch to HiZ
    modes[0].protocol_setup_exc();                  // disables power suppy etc.
    system_config.mode = mode;                      // setup the new mode
    if(!modes[system_config.mode].protocol_setup_exc()){ // execute the mode setup
        printf("\r\nFailed to setup mode %s", modes[system_config.mode].protocol_name);
        //something went wrong
        modes[0].protocol_setup_exc();
        system_config.mode = 0;
    }
    fala_mode_change_hook();                        // notify follow along logic analyzer of new frequency

    if (system_config.mode == 0) { // TODO: do something to show the mode (LED? LCD?)
        // gpio_clear(BP_MODE_LED_PORT, BP_MODE_LED_PIN);
    } else {
        // gpio_set(BP_MODE_LED_PORT, BP_MODE_LED_PIN);
    }

    printf("\r\n%s%s:%s %s",
           ui_term_color_info(),
           GET_T(T_MODE_MODE),
           ui_term_color_reset(),
           modes[system_config.mode].protocol_name);
}

/*




void ui_mode_enable(struct command_attributes *attributes, struct command_response *response)
{
    uint32_t mode;
    bool error;

    prompt_result result;
    ui_parse_get_attributes(&result, &mode, 1);

    if( result.error || result.no_value || result.exit || ((mode)>MAXPROTO) || ((mode)==0) )
    {
        if( result.success && (mode)>MAXPROTO )
        {
            ui_prompt_invalid_option();
        }
        error=true;
    }
    else
    {
        (mode)--; //adjust down one from user choice
        error=false;
    }

    if(error)			// no integer found
    {
        static const struct ui_prompt_config cfg={
            true, //bool allow_prompt_text;
            false, //bool allow_prompt_defval;
            false, //bool allow_defval;
            true, //bool allow_exit;
            &ui_mode_list, //bool (*menu_print)(const struct ui_prompt* menu);
            &ui_prompt_prompt_ordered_list,     //bool (*menu_prompt)(const struct ui_prompt* menu);
            &ui_prompt_validate_ordered_list //bool (*menu_validate)(const struct ui_prompt* menu, uint32_t* value);
        };

        static const struct ui_prompt mode_menu={
            T_MODE_MODE_SELECTION,
            0,
            MAXPROTO,
            T_MODE_MODE,
            0,0,0,
            0,
            &cfg
        };

        prompt_result result;
        ui_prompt_uint32(&result, &mode_menu, &mode);
        if(result.exit) //user bailed, stay in current mode
        {
            (*response).error=true;
            return;
        }
        mode--;
    }

    //ok, start setup dialog
    if(!modes[mode].protocol_setup()) //user bailed on setup steps
    {
        (*response).error=true;
        return;
    }

    modes[system_config.mode].protocol_cleanup();   // switch to HiZ
    modes[0].protocol_setup_exc();			        // disables power suppy etc.
    system_config.mode=mode;                        // setup the new mode
    modes[system_config.mode].protocol_setup_exc(); // execute the mode setup

    if(system_config.mode==0) //TODO: do something to show the mode (LED? LCD?)
    {
        //gpio_clear(BP_MODE_LED_PORT, BP_MODE_LED_PIN);
    }
    else
    {
        //gpio_set(BP_MODE_LED_PORT, BP_MODE_LED_PIN);
    }

    printf("\r\n%s%s:%s %s", ui_term_color_info(), GET_T(T_MODE_MODE), ui_term_color_reset(),
modes[system_config.mode].protocol_name);

}*/

bool int_display_menu(const struct ui_prompt* menu) {
    printf(" %sCurrent setting: %s%s\r\n",
           ui_term_color_info(),
           ui_const_display_formats[system_config.display_format],
           ui_term_color_reset());
    for (uint i = 0; i < (*menu).menu_items_count; i++) {
        printf(" %d. %s%s%s\r\n", i + 1, ui_term_color_info(), ui_const_display_formats[i], ui_term_color_reset());
    }
}

// set display mode  (hex, bin, octa, dec)
void ui_mode_int_display_format(struct command_result* res) {
    uint32_t mode;
    bool error;

    prompt_result result;
    ui_parse_get_attributes(&result, &mode, 1);

    if (result.error || result.no_value || result.exit || ((mode) > count_of(ui_const_display_formats)) ||
        ((mode) == 0)) {
        if (result.success && (mode) > count_of(ui_const_display_formats)) {
            ui_prompt_invalid_option();
        }
        error = 1;
    } else {
        (mode)--; // adjust down one from user choice
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

        static const struct ui_prompt mode_menu = {
            T_MODE_NUMBER_DISPLAY_FORMAT, 0, count_of(ui_const_display_formats), T_MODE_MODE, 0, 0, 0, 0, &cfg
        };

        prompt_result result;
        ui_prompt_uint32(&result, &mode_menu, &mode);
        if (result.exit) // user bailed
        {
            (*res).error = true;
            return;
        }
        mode--;
    }

    system_config.display_format = (uint8_t)mode;

    printf("\r\n%s%s:%s %s",
           ui_term_color_info(),
           GET_T(T_MODE_MODE),
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
