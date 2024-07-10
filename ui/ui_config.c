#include <stdio.h>
#include "pico/stdlib.h"
#include <stdint.h>
#include "pirate.h"
#include "system_config.h"
#include "opt_args.h"
#include "bytecode.h"
#include "modes.h"
#include "hardware/uart.h"
#include "ui/ui_term.h"
#include "pirate/storage.h"
#include "ui/ui_const.h"
#include "ui/ui_info.h"
#include "ui/ui_term.h"
#include "ui/ui_prompt.h"
#include "ui/ui_statusbar.h"
#include "ui/ui_flags.h"
#include "pirate/rgb.h" // for LED effect enum

bool ui_config_menu(const struct ui_prompt * menu);

// menu items options
static const struct prompt_item menu_items_disable_enable[]=
{
    {T_CONFIG_DISABLE},
    {T_CONFIG_ENABLE},
};

// LED effect
static const struct prompt_item menu_items_led_effect[]=
{
    // clang-format off
    [LED_EFFECT_DISABLED      ] = {T_CONFIG_DISABLE},
    [LED_EFFECT_SOLID         ] = {T_CONFIG_LEDS_EFFECT_SOLID},
    [LED_EFFECT_ANGLE_WIPE    ] = {T_CONFIG_LEDS_EFFECT_ANGLEWIPE},
    [LED_EFFECT_CENTER_WIPE   ] = {T_CONFIG_LEDS_EFFECT_CENTERWIPE},
    [LED_EFFECT_CLOCKWISE_WIPE] = {T_CONFIG_LEDS_EFFECT_CLOCKWISEWIPE},
    [LED_EFFECT_TOP_SIDE_WIPE ] = {T_CONFIG_LEDS_EFFECT_TOPDOWNWIPE},
    [LED_EFFECT_SCANNER       ] = {T_CONFIG_LEDS_EFFECT_SCANNER},
    [LED_EFFECT_PARTY_MODE    ] = {T_CONFIG_LEDS_EFFECT_CYCLE},
    // clang-format on
};
static_assert(count_of(menu_items_led_effect) == MAX_LED_EFFECT, "menu_items_led_effect mismatch vs. enum of available effects");

uint32_t ui_config_action_led_effect(uint32_t a, uint32_t b)
{
    if (b < count_of(menu_items_led_effect)) {
        system_config.led_effect = b;
    }
}

// LED color (?) Rainbow short, rainbow long, ROYGBIV
// Convenience ... a shortened list of HSV colors (as RGB) from fastLED
// Any color can be configured in the configuration file
// as a 24-bit RGB value; The menu simply provides common
// colors as a convenience.
static const struct prompt_item menu_items_led_color[]=
{
    {T_CONFIG_LEDS_COLOR_RED},
    {T_CONFIG_LEDS_COLOR_ORANGE},
    {T_CONFIG_LEDS_COLOR_YELLOW},
    {T_CONFIG_LEDS_COLOR_GREEN},
    {T_CONFIG_LEDS_COLOR_BLUE},
    {T_CONFIG_LEDS_COLOR_PURPLE},
    {T_CONFIG_LEDS_COLOR_PINK},
};

uint32_t ui_config_action_led_color(uint32_t a, uint32_t b)
{
    // This needs to stay in sync with the above list of colors
    static const uint32_t menu_based_colors[]= {
        0xFF0000, // aka T_CONFIG_LEDS_COLOR_RED  
        0xD52A00, // aka T_CONFIG_LEDS_COLOR_ORANGE  
        0xAB7F00, // aka T_CONFIG_LEDS_COLOR_YELLOW  
        0x00FF00, // aka T_CONFIG_LEDS_COLOR_GREEN  
        0x0000FF, // aka T_CONFIG_LEDS_COLOR_BLUE  
        0x5500AB, // aka T_CONFIG_LEDS_COLOR_PURPLE  
        0xAB0055, // aka T_CONFIG_LEDS_COLOR_PINK 
    };
 
    static_assert(count_of(menu_based_colors) == count_of(menu_items_led_color), "menu_based_colors and menu_items_led_color must have the same number of items");
    if (b < count_of(menu_items_led_color)) {
        system_config.led_color = menu_based_colors[b];
    }
}

// LED brightness %?
static const struct prompt_item menu_items_led_brightness[]=
{
    {T_CONFIG_LEDS_BRIGHTNESS_10},
    {T_CONFIG_LEDS_BRIGHTNESS_20},
    {T_CONFIG_LEDS_BRIGHTNESS_30},
    //{T_CONFIG_LEDS_BRIGHTNESS_40},
    //{T_CONFIG_LEDS_BRIGHTNESS_50},
    //{T_CONFIG_LEDS_BRIGHTNESS_100},
};

uint32_t ui_config_action_led_brightness(uint32_t a, uint32_t b)
{
    if (b < count_of(menu_items_led_brightness)) {
        // Was: system_config.led_brightness_divisor = 10/(b+1)
        // The following is a lot easier to understand, though:
        // clang-format off
        if (b == 0) {
            system_config.led_brightness_divisor = 10; // 10% brightness == divide by 10
        } else if (b == 1) {
            system_config.led_brightness_divisor =  5; // 20% brightness == divide by  5
        } else if (b == 2) {
            system_config.led_brightness_divisor =  3; // 30% brightness ~= divide by  3
        } else {
            assert(false);
            static_assert(count_of(menu_items_led_brightness) == 3, "must update this switch statement");
        }
        // clang-format on
    }
}


// config menu helper functions
static const struct prompt_item menu_items_screensaver[]=
{
    {T_CONFIG_DISABLE},
    {T_CONFIG_SCREENSAVER_5},
    {T_CONFIG_SCREENSAVER_10},
    {T_CONFIG_SCREENSAVER_15},
};

uint32_t ui_config_action_screensaver(uint32_t a, uint32_t b)
{
    if (b < count_of(menu_items_screensaver)) {
        system_config.lcd_timeout = b;
    }
}

static const struct prompt_item menu_items_ansi_color[] =
{
    {T_CONFIG_DISABLE}, 
    {T_CONFIG_ANSI_COLOR_FULLCOLOR},
#ifdef ANSI_COLOR_256
    {T_CONFIG_ANSI_COLOR_256}
#endif
};
uint32_t ui_config_action_ansi_color(uint32_t a, uint32_t b)
{
    if (b < count_of(menu_items_ansi_color)) {
        system_config.terminal_ansi_color = b;
        if(!b)
        {
            system_config.terminal_ansi_statusbar=0; // disable the toolbar if ansi is disabled....
        }
        else
        {
            ui_term_detect(); // Do we detect a VT100 ANSI terminal? what is the size?
        }
    }
}

uint32_t ui_config_action_ansi_toolbar(uint32_t a, uint32_t b)
{
    // NOTE: `b` is treated as a boolean value
    b = !!b;

    system_config.terminal_ansi_statusbar = b;
    if (b)
    {   
        if (!system_config.terminal_ansi_color) {
            // enable ANSI color mode
            system_config.terminal_ansi_color=UI_TERM_FULL_COLOR;
        }
        ui_term_detect(); // Do we detect a VT100 ANSI terminal? what is the size?
        ui_term_init();   // Initialize VT100 if ANSI terminal
        ui_statusbar_update(UI_UPDATE_ALL);
    }
}

static const struct prompt_item menu_items_language[]=
{
    { T_CONFIG_LANGUAGE_ENGLISH },
    { T_CONFIG_LANGUAGE_POLISH },
    { T_CONFIG_LANGUAGE_BOSNIAN },
    { T_CONFIG_LANGUAGE_ITALIAN },
    //{ T_CONFIG_LANGUAGE_CHINESE },
};

uint32_t ui_config_action_language(uint32_t a, uint32_t b)
{
    if (b < count_of(menu_items_language)) {
        system_config.terminal_language = b;
        translation_set(b);
    }
}

static const struct ui_prompt_config cfg=
{
    // clang-format on
	.allow_prompt_text   = false,
	.allow_prompt_defval = false,
	.allow_defval        = false,
	.allow_exit          = true,
    .menu_print          = &ui_prompt_menu_ordered_list,
	.menu_prompt         = &ui_prompt_prompt_ordered_list,
	.menu_validate       = &ui_prompt_validate_ordered_list,
    // clang-format off
};

static const struct ui_prompt sub_prompts[]={
    // clang-format off
    {T_CONFIG_LANGUAGE,          menu_items_language,       count_of(menu_items_language),       0,0,0,0, &ui_config_action_language,       &cfg},
    {T_CONFIG_ANSI_COLOR_MODE,   menu_items_ansi_color,     count_of(menu_items_ansi_color),     0,0,0,0, &ui_config_action_ansi_color,     &cfg},
    {T_CONFIG_ANSI_TOOLBAR_MODE, menu_items_disable_enable, count_of(menu_items_disable_enable), 0,0,0,0, &ui_config_action_ansi_toolbar,   &cfg},
    {T_CONFIG_SCREENSAVER,       menu_items_screensaver,    count_of(menu_items_screensaver),    0,0,0,0, &ui_config_action_screensaver,    &cfg},
    {T_CONFIG_LEDS_EFFECT,       menu_items_led_effect,     count_of(menu_items_led_effect),     0,0,0,0, &ui_config_action_led_effect,     &cfg},
    {T_CONFIG_LEDS_COLOR,        menu_items_led_color,      count_of(menu_items_led_color),      0,0,0,0, &ui_config_action_led_color,      &cfg},
    {T_CONFIG_LEDS_BRIGHTNESS,   menu_items_led_brightness, count_of(menu_items_led_brightness), 0,0,0,0, &ui_config_action_led_brightness, &cfg},
    // clang-format on
};

static const struct ui_prompt_config main_cfg=
{
    // clang-format off
	.allow_prompt_text   = false,
	.allow_prompt_defval = false,
	.allow_defval        = false,
	.allow_exit          = true,
	.menu_print          = &ui_config_menu,
	.menu_prompt         = &ui_prompt_prompt_ordered_list,
	.menu_validate       = &ui_prompt_validate_int,
    // clang-format on
};

static const struct ui_prompt main_prompt=
{
    // clang-format off
    .description      = T_CONFIG_CONFIGURATION_OPTIONS,
	.menu_items       = NULL,
    .menu_items_count = count_of(sub_prompts),
    .prompt_text      = 0,
    .minval           = 1,
    .maxval           = count_of(sub_prompts),
    .defval           = 0,
	.menu_action      = NULL,
    .config           = &main_cfg,
    // clang-format on
};


bool ui_config_menu(const struct ui_prompt * menu)
{
    for (uint i = 0; i < count_of(sub_prompts); i++)
    {
        printf(" %d. %s%s%s\r\n", i+1, ui_term_color_info(), t[sub_prompts[i].description], ui_term_color_reset()); 
    }   
}

void ui_config_main_menu(struct command_result *res)
{
    while(1)
    {
        uint32_t temp;
        prompt_result result;
        ui_prompt_uint32(&result, &main_prompt, &temp);
        if(result.exit)
        {
            break;
        }
        temp--;
        printf("\r\n");

        uint32_t temp2;

        ui_prompt_uint32(&result, &sub_prompts[temp], &temp2);
        if(result.exit)
        {
            break;
        }
        temp2--;
        sub_prompts[temp].menu_action(temp, temp2);
        printf("\r\n%s %sset to%s %s\r\n", t[sub_prompts[temp].description], ui_term_color_info(), ui_term_color_reset(), t[sub_prompts[temp].menu_items[temp2].description]);
    }

    //if TF flash card is present, saves configuration settings
    //TODO: present as an option to save or not
    if(storage_save_config())
    {
        printf("\r\n\r\n%s%s:%s %s\r\n", ui_term_color_info(), t[T_CONFIG_FILE], ui_term_color_reset(), t[T_SAVED] );
    }

}
