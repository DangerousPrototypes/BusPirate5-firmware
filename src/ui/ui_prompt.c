/**
 * @file ui_prompt.c
 * @brief User interface prompt and menu system implementation.
 * @details Implements interactive menu system with multiple input types:
 *          - Ordered lists (numbered menu items)
 *          - Integer input with min/max validation
 *          - I/O pin selection with availability checking
 *          
 *          Features:
 *          - Default value support
 *          - Input validation
 *          - Multiple number formats (hex, decimal, binary)
 *          - Custom validators and formatters
 *          - Exit/cancel support
 */

#include <stdbool.h>
#include <stdio.h>
#include "pico/stdlib.h"
#include <stdint.h>
#include "pirate.h"
#include "system_config.h"
#include "command_struct.h"
#include "ui/ui_prompt.h"
#include "ui/ui_parse.h"
#include "ui/ui_term.h"
#include "ui/ui_term_linenoise.h"
#include "ui/ui_info.h"
#include "ui/ui_cmdln.h"
#include "usb_rx.h"
#include "ui_help.h"

static const struct prompt_result empty_result;

void ui_prompt_invalid_option(void) {
    ui_help_error(T_MODE_INVALID_OPTION);
}

// INTEGER
bool ui_prompt_validate_int(const struct ui_prompt* menu, uint32_t* value) {
    return (((*value) >= (*menu).minval) && ((*value) <= (*menu).maxval));
}

// ORDERED LISTS
bool ui_prompt_menu_ordered_list(const struct ui_prompt* menu) {
    for (uint i = 0; i < (*menu).menu_items_count; i++) {
        printf(" %d. %s%s%s%s\r\n",
               i + 1,
               ui_term_color_info(),
               GET_T((*menu).menu_items[i].description),
               (*menu).defval == i + 1 ? "*" : "",
               ui_term_color_reset());
    }
    return true;
}

bool ui_prompt_prompt_ordered_list(const struct ui_prompt* menu) {
    // prompt
    printf("%s", ui_term_color_prompt());
    if ((*menu).config->allow_prompt_text) {
        printf("%s", GET_T((*menu).prompt_text));
    }
    if ((*menu).config->allow_defval && (*menu).config->allow_prompt_defval) {
        printf(" (%s%d%s)", ui_term_color_reset(), (*menu).defval, ui_term_color_prompt());
    }
    printf(" >%s \x03", ui_term_color_reset());

    return true;
}

bool ui_prompt_validate_ordered_list(const struct ui_prompt* menu, uint32_t* value) {
    return (((*value) >= 1) && ((*value) <= (*menu).menu_items_count)); // menu style //TODO: minus one, user min/max?
}

// used internally in ui_prompt
// gets user input until <enter> or return false if system error
bool ui_prompt_user_input(void) {
    // Use linenoise for input (empty prompt - caller already printed it)
    if (!ui_prompt_linenoise_input("")) {
        return false;  // Cancelled or error
    }
    ui_parse_consume_whitespace();
    return true;
}

/**
 * @brief Start non-blocking VT100 y/n prompt.
 * @param prompt  Prompt string to display
 */
void ui_prompt_vt100_mode_start(const char *prompt) {
    ui_prompt_linenoise_start(prompt);
}

/**
 * @brief Poll non-blocking VT100 y/n prompt.
 * @param[out] value  Receives 'y' or 'n' when complete
 * @return true when user has answered, false while still editing
 *
 * Only 'y' and 'n' are accepted;
 */
bool ui_prompt_vt100_mode_feed(uint32_t *value) {
    uint32_t ln_res = ui_prompt_linenoise_feed();
    if (ln_res == 0xff) {
        *value = 0x00; //invalid input
        // Enter pressed — read first char
        char ch;
        if (cmdln_try_remove(&ch)) {
            ch |= 0x20; // lowercase
            if(ch == 'y' || ch == 'n') {
                *value = ch;
            }
        }
        return true;
    }
    return false; // still editing
}

// ask user for integer until it falls between minval and maxval, enter returns the default value, x exits
bool ui_prompt_uint32(prompt_result* result, const struct ui_prompt* menu, uint32_t* value) {
    printf("\r\n");
    while (true) {
        *result = empty_result;

        // description
        printf("%s%s%s\r\n", ui_term_color_info(), GET_T((*menu).description), ui_term_color_reset());
        // options
        if (!(*menu).config->menu_print(menu)) {
            result->error = true;
            return false;
        }

        // exit
        if ((*menu).config->allow_exit) {
            printf(" x. %s%s%s\r\n", ui_term_color_info(), GET_T(T_EXIT), ui_term_color_reset());
        }
        (*menu).config->menu_prompt(menu);

        if (!ui_prompt_user_input()) {
            result->exit = true; // a little hackish, but we do want to exit right?
            return false;
        }

        ui_parse_get_uint32(result, value);

        if ((*menu).config->allow_exit && result->exit) {
            return false;
        }

        if ((*menu).config->allow_defval && result->no_value) // user pressed enter
        {
            (*value) = (*menu).defval;
            result->default_value = true;
            return true;
        } else if (result->success && ((*menu).config->menu_validate(menu, value))) {
            return true;
        } else {
            ui_prompt_invalid_option();
        }
    }
}

// keep the user asking the menu until it falls between minval and maxval, enter returns the default value, x optionally
// exits
bool ui_prompt_float(
    prompt_result* result, float minval, float maxval, float defval, bool allow_exit, float* user_value, bool none) {
    while (true) {
        printf("\r\n%s%s ", ui_term_color_prompt(), (allow_exit ? "x to exit" : ""));
        if (!none) {
            printf("(%1.2f)", defval);
        } else {
            printf("(none)");
        }
        printf(" >%s \x03", ui_term_color_reset());

        if (!ui_prompt_user_input()) {
            result->exit = true; // a little hackish, but we do want to exit right?
            return false;
        }

        ui_parse_get_float(result, user_value);

        printf("\r\n");

        if (allow_exit && result->exit) {
            return false;
        }

        if (result->no_value) // assume user pressed enter
        {
            (*user_value) = defval;
            result->default_value = true;
            return true;
        } else if (result->success && ((*user_value) >= minval) && ((*user_value) <= maxval)) {
            return true;
        } else {
            ui_prompt_invalid_option();
        }
    }
}

// returns float value in user_units
bool ui_prompt_float_units(prompt_result* result, const char* menu, float* user_value, uint8_t* user_units) {
    while (1) {
        printf("\r\n%s%s >%s \x03", ui_term_color_prompt(), menu, ui_term_color_reset());

        if (!ui_prompt_user_input()) {
            result->exit = true; // a little hackish, but we do want to exit right?
            return false;
        }

        ui_parse_get_float(result, user_value);

        if (result->exit) {
            return true;
        }

        // get the trailing type
        char units[4] = { 0, 0, 0, 0 };

        if (ui_parse_get_units(result, units, 4, user_units)) {
            return true;
        }

        ui_prompt_invalid_option();
        // TODO: loop these out from the units const array...
        printf("%sError:%s Unknown units '%s'\r\n%sValid units: ns, us, ms, Hz, kHz, MHz, %%%s\r\n",
               ui_term_color_error(),
               ui_term_color_reset(),
               units,
               ui_term_color_info(),
               ui_term_color_reset());
    }
}

bool ui_prompt_any_key_continue(prompt_result* result,
                                uint32_t delay,
                                uint32_t (*refresh_func)(uint8_t pin, bool refresh),
                                uint8_t pin,
                                bool refresh) {
    *result = empty_result;
    // press any key to continue
    printf("%s%s%s\r\n%s",
           ui_term_color_notice(),
           GET_T(T_PRESS_ANY_KEY_TO_EXIT),
           ui_term_color_reset(),
           ui_term_cursor_hide());
    char c;

    do {
        refresh_func(pin, refresh);
        busy_wait_ms(delay);
    } while (!rx_fifo_try_get(&c) && !system_config.error);

    printf("%s", ui_term_cursor_show()); // show cursor

    result->success = 1;
    return true;
}


