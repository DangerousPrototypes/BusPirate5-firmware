#include <stdio.h>
#include "pico/stdlib.h"
#include "pirate.h"
#include "system_config.h"
#include "opt_args.h"
#include "ui/ui_term.h"
#include "ui/ui_statusbar.h"
#include "ui/ui_help.h"
#include "ui/ui_flags.h"


static const char * const usage[]={
    "clr",  
    "Clear and refresh the terminal screen: clr", 
};

static const struct ui_help_options options[]={
};

void ui_display_clear(struct command_result *res){
    if(ui_help_show(res->help_flag,usage,count_of(usage), &options[0],count_of(options) )) return;

    ui_term_detect(); // Do we detect a VT100 ANSI terminal? what is the size?
    ui_term_init();   // Initialize VT100 if ANSI terminal
    ui_statusbar_update(UI_UPDATE_ALL);
}
