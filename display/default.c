#include <stdio.h>
#include <string.h>
#include "pico/stdlib.h"
#include "pirate.h"
#include "system_config.h"
#include "opt_args.h"
#include "hardware/uart.h"
//#include "hiz.h"
#include "bio.h"	
#include "psu.h"
#include "pullups.h"
#include "pico/stdlib.h"
#include "hardware/pio.h"
#include "hardware/dma.h"
#include "hardware/adc.h"
#include "hardware/irq.h"
#include "hardware/spi.h"
#include "font/font.h"
//#include "font/hunter-23pt-24h24w.h"
//#include "font/hunter-20pt-21h21w.h"
//#include "font/hunter-14pt-19h15w.h"
//#include "font/hunter-12pt-16h13w.h"
//#include "font/background.h"
//#include "font/background_image_v4.h"
#include "ui/ui_flags.h"
#include "ui/ui_lcd.h"
#include "ui/ui_prompt.h"
#include "ui/ui_parse.h"
#include "ui/ui_cmdln.h"
#include "usb_rx.h"


void
disp_default_settings(void)
{
}

void
disp_default_help(void)
{
}


void disp_default_cleanup(void)
{
}

uint32_t disp_default_periodic(void)
{
	return 0;
}

uint32_t disp_default_setup(void)
{
	return 1;
}

static bool switching=0;
uint32_t disp_default_setup_exc(void)
{
	switching = 1;
	ui_lcd_update(UI_UPDATE_ALL|UI_UPDATE_FORCE);
	switching = 0;
	return 1;
}


uint32_t
disp_default_commands(struct opt_args *args, struct command_result *result)
{
	return 0;
}

void
disp_default_lcd_update(uint32_t flags)
{
	if (!switching)
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
