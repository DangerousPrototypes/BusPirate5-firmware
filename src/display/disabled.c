#include <stdio.h>
#include <string.h>
#include "pico/stdlib.h"
#include "pirate.h"
#include "system_config.h"
#include "command_struct.h"
#include "pirate/bio.h"
#include "pico/stdlib.h"
#include "ui/ui_flags.h"
#include "ui/ui_lcd.h"
#include "ui/ui_prompt.h"
#include "ui/ui_parse.h"
#include "ui/ui_cmdln.h"

void disp_disabled_settings(void) {}

void disp_disabled_help(void) {}

void disp_disabled_cleanup(void) {}

uint32_t disp_disabled_periodic(void) {
    return 0;
}

uint32_t disp_disabled_setup(void) {
    return 1;
}

uint32_t disp_disabled_setup_exc(void) {
    lcd_disable();
    return 1;
}

uint32_t disp_disabled_commands(struct command_result* result) {
    return 0;
}

void disp_disabled_lcd_update(uint32_t flags) {
    ui_lcd_update(flags);
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
