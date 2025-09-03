#include <stdio.h>
#include <string.h>
#include "pico/stdlib.h"
#include "pico/multicore.h"
#include "pirate.h"
#include "system_config.h"
#include "command_struct.h"
#include "hardware/uart.h"
// #include "hiz.h"
#include "pirate/bio.h"
#include "pico/stdlib.h"
#include "hardware/pio.h"
#include "hardware/dma.h"
#include "hardware/adc.h"
#include "hardware/irq.h"
#include "hardware/spi.h"
#include "font/font.h"
// #include "font/hunter-23pt-24h24w.h"
// #include "font/hunter-20pt-21h21w.h"
// #include "font/hunter-14pt-19h15w.h"
// #include "font/hunter-12pt-16h13w.h"
// #include "font/background.h"
// #include "font/background_image_v4.h"
#include "ui/ui_flags.h"
#include "ui/ui_lcd.h"
#include "ui/ui_prompt.h"
#include "ui/ui_parse.h"
#include "ui/ui_cmdln.h"
#include "usb_rx.h"
#include "pirate/intercore_helpers.h"

void disp_default_settings(void) {}

void disp_default_help(void) {}

void disp_default_cleanup(void) {}

uint32_t disp_default_periodic(void) {
    return 0;
}

uint32_t disp_default_setup(void) {
    return 1;
}

uint32_t disp_default_setup_exc(void) {
    icm_core0_send_message_synchronous(BP_ICM_DISABLE_LCD_UPDATES);
    icm_core0_send_message_synchronous(BP_ICM_FORCE_LCD_UPDATE);
    lcd_enable();
    return 1;
}

uint32_t disp_default_commands(struct command_result* result) {
    return 0;
}

void disp_default_lcd_update(uint32_t flags) {
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
