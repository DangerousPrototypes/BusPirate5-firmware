#include <stdint.h>
#include <string.h>
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
#include "ui/ui_help.h"

bool ui_mode_list(const struct ui_prompt* menu) {
    for (uint i = 0; i < (*menu).menu_items_count; i++) {
        printf(" %d. %s%s%s\r\n", i + 1, ui_term_color_info(), modes[i].protocol_name, ui_term_color_reset());
    }
    return true;
}

static const char* const usage[] = {
    "m [mode name|mode number] [-h]",
    "Change mode with menu: m",
    "Change mode to I2C: m i2c",
    "Change mode to menu option 5: m 5",
};

static const struct ui_help_options options[] = { 0 };

void ui_mode_enable_args(struct command_result* res) {
    if (ui_help_show(res->help_flag, usage, count_of(usage), &options[0], count_of(options))) {
        return;
    }    
    uint32_t mode;
    bool error=true;

    //bool has_value = cmdln_args_uint32_by_position(1, &mode);

    char action_str[32];
    bool has_string = cmdln_args_string_by_position(1, sizeof(action_str), action_str);

    if(has_string){
        uint32_t action_len = strlen(action_str);
        if(action_len>2){ //parse text
            strupr(action_str);
            for(uint8_t i=0; i<count_of(modes); i++) {
                //create a strupr version of the protocol name
                char protocol_name_upper[32];
                strcpy(protocol_name_upper, modes[i].protocol_name);
                strupr(protocol_name_upper);
                if (strcmp(action_str, protocol_name_upper) == 0) {
                    mode = i;
                    error = false;
                    goto mode_configure;
                }
            }
            ui_prompt_invalid_option();            
        }else{ //try to parse number
            for(uint8_t i=0; i<action_len; i++) {
                if(action_str[i] < '0' || action_str[i] > '9') {
                    ui_prompt_invalid_option();
                    error = true;
                    goto mode_configure;
                }
            }
            mode = atoi(action_str);
            if(mode > MAXPROTO || mode == 0){
                ui_prompt_invalid_option();
                error = true;
            }else{
                mode--;
                error = false;
            }
        }
    }

mode_configure:

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

    printf("\r\n%s%s:%s %s",
        ui_term_color_info(),
        GET_T(T_MODE_MODE),
        ui_term_color_reset(),
        modes[mode].protocol_name);

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
