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

bool ui_config_menu(const struct ui_prompt * menu);

// menu items options
static const struct prompt_item menu_items_disable_enable[]=
{
    {T_CONFIG_DISABLE},{T_CONFIG_ENABLE}
};

// LED effect
static const struct prompt_item menu_items_led_effect[]=
{
    {T_CONFIG_DISABLE},{T_CONFIG_LEDS_EFFECT_SOLID},{T_CONFIG_LEDS_EFFECT_ANGLEWIPE},{T_CONFIG_LEDS_EFFECT_CENTERWIPE},{T_CONFIG_LEDS_EFFECT_CLOCKWISEWIPE},{T_CONFIG_LEDS_EFFECT_TOPDOWNWIPE},{T_CONFIG_LEDS_EFFECT_SCANNER},{T_CONFIG_LEDS_EFFECT_CYCLE}
};

uint32_t ui_config_action_led_effect(uint32_t a, uint32_t b)
{
    system_config.led_effect=b;
}

// LED color (?) Rainbow short, rainbow long, ROYGBIV
static const struct prompt_item menu_items_led_color[]=
{
    {T_CONFIG_LEDS_COLOR_RED},{T_CONFIG_LEDS_COLOR_ORANGE},{T_CONFIG_LEDS_COLOR_YELLOW},{T_CONFIG_LEDS_COLOR_GREEN},{T_CONFIG_LEDS_COLOR_BLUE},{T_CONFIG_LEDS_COLOR_PURPLE},{T_CONFIG_LEDS_COLOR_PINK}
};

uint32_t ui_config_action_led_color(uint32_t a, uint32_t b)
{
    system_config.led_color=b;    
}

// LED brightness %?
static const struct prompt_item menu_items_led_brightness[]=
{
    {T_CONFIG_LEDS_BRIGHTNESS_10},{T_CONFIG_LEDS_BRIGHTNESS_20},{T_CONFIG_LEDS_BRIGHTNESS_30}//,{T_CONFIG_LEDS_BRIGHTNESS_40},{T_CONFIG_LEDS_BRIGHTNESS_50},{T_CONFIG_LEDS_BRIGHTNESS_100},
};

uint32_t ui_config_action_led_brightness(uint32_t a, uint32_t b)
{
    if(b==5) system_config.led_brightness=1;
    else system_config.led_brightness=10/(b+1);     
}


// config menu helper functions
static const struct prompt_item menu_items_screensaver[]=
{
    {T_CONFIG_DISABLE}, {T_CONFIG_SCREENSAVER_5}, {T_CONFIG_SCREENSAVER_10}, {T_CONFIG_SCREENSAVER_15}
};

uint32_t ui_config_action_screensaver(uint32_t a, uint32_t b)
{
    system_config.lcd_timeout=b;
}

uint32_t ui_config_action_ansi_color(uint32_t a, uint32_t b)
{
    system_config.terminal_ansi_color=b;
    if(!b)
    {
        system_config.terminal_ansi_statusbar=0; //disable the toolbar if ansi is disabled....
    }
    else
    {
        ui_term_detect(); // Do we detect a VT100 ANSI terminal? what is the size?
    }

}

uint32_t ui_config_action_ansi_toolbar(uint32_t a, uint32_t b)
{
    system_config.terminal_ansi_statusbar=b;
    if(b)
    {   
        system_config.terminal_ansi_color=1; //enable ANSI color more
        ui_term_detect(); // Do we detect a VT100 ANSI terminal? what is the size?
        ui_term_init(); // Initialize VT100 if ANSI terminal
        ui_statusbar_update(UI_UPDATE_ALL);
    }
}

static const struct prompt_item menu_items_language[]=
{
    {T_CONFIG_LANGUAGE_ENGLISH},{T_CONFIG_LANGUAGE_POLISH},{T_CONFIG_LANGUAGE_BOSNIAN}//,{T_CONFIG_LANGUAGE_CHINESE}
};

uint32_t ui_config_action_language(uint32_t a, uint32_t b)
{
    system_config.terminal_language=b; 
    translation_set(b);
}

static const struct ui_prompt_config cfg=
{
	false, //bool allow_prompt_text;
	false, //bool allow_prompt_defval;
	false, //bool allow_defval; 
	true, //bool allow_exit;
	&ui_prompt_menu_ordered_list, //bool (*menu_print)(const struct ui_prompt* menu);
	&ui_prompt_prompt_ordered_list,     //bool (*menu_prompt)(const struct ui_prompt* menu);
	&ui_prompt_validate_ordered_list //bool (*menu_validate)(const struct ui_prompt* menu, uint32_t* value);
};

static const struct ui_prompt sub_prompts[]={
    {T_CONFIG_LANGUAGE, menu_items_language, count_of(menu_items_language),0,0,0,0,&ui_config_action_language, &cfg},
    {T_CONFIG_ANSI_COLOR_MODE,menu_items_disable_enable,count_of(menu_items_disable_enable),0,0,0,0,&ui_config_action_ansi_color,&cfg},
    {T_CONFIG_ANSI_TOOLBAR_MODE,menu_items_disable_enable,count_of(menu_items_disable_enable),0,0,0,0,&ui_config_action_ansi_toolbar,&cfg},
    {T_CONFIG_SCREENSAVER,menu_items_screensaver,count_of(menu_items_screensaver),0,0,0,0,&ui_config_action_screensaver,&cfg},
    {T_CONFIG_LEDS_EFFECT, menu_items_led_effect, count_of(menu_items_led_effect),0,0,0,0, &ui_config_action_led_effect, &cfg},
    {T_CONFIG_LEDS_COLOR, menu_items_led_color, count_of(menu_items_led_color),0,0,0,0, &ui_config_action_led_color, &cfg},
    {T_CONFIG_LEDS_BRIGHTNESS, menu_items_led_brightness, count_of(menu_items_led_brightness),0,0,0,0, &ui_config_action_led_brightness, &cfg}
};

static const struct ui_prompt_config main_cfg=
{
	false, //bool allow_prompt_text;
	false, //bool allow_prompt_defval;
	false, //bool allow_defval; 
	true, //bool allow_exit;
	&ui_config_menu, //bool (*menu_print)(const struct ui_prompt* menu);
	&ui_prompt_prompt_ordered_list,     //bool (*menu_prompt)(const struct ui_prompt* menu);
	&ui_prompt_validate_int //bool (*menu_validate)(const struct ui_prompt* menu, uint32_t* value);
};

static const struct ui_prompt main_prompt=
{
	T_CONFIG_CONFIGURATION_OPTIONS,
	0,count_of(sub_prompts),0,
    1,count_of(sub_prompts),0,
	0, &main_cfg
};

bool ui_config_menu(const struct ui_prompt * menu)
{
    for(uint i=0; i<count_of(sub_prompts); i++)
    {
        printf(" %d. %s%s%s\r\n", i+1, ui_term_color_info(),t[sub_prompts[i].description], ui_term_color_reset()); 
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