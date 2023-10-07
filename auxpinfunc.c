#include <stdint.h>
#include "pico/stdlib.h"
#include "pirate.h"
#include "auxpinfunc.h"
#include "bio.h"
#include "system_config.h"
#include "ui/ui_term.h"

void auxpinfunc_write(opt_args (*args), struct command_result *res, bool output, bool level);

static const char labels[][5]={"AUXL","AUXH"};

void auxpinfunc_high(opt_args (*args), struct command_result *res)
{
    auxpinfunc_write(args, res, true, 1);
}

void auxpinfunc_low(opt_args (*args), struct command_result *res)
{
    auxpinfunc_write(args, res, true, 0);
}

void auxpinfunc_input(opt_args (*args), struct command_result *res)
{
    auxpinfunc_write(args, res, false, 0);
}

void auxpinfunc_write(opt_args (*args), struct command_result *res, bool output, bool level)
{
	if(!args[0].i)
	{
		printf("%sError:%s specify an IO pin (a.1, A.5, @.0)", ui_term_color_error(), ui_term_color_reset());
		res->error=true;
        return;
	}

    uint32_t pin=args[0].i;

    // first make sure the pin is present and available
    if(pin>=count_of(bio2bufiopin))
    {
        printf("%sError:%s pin IO%d is invalid", ui_term_color_error(), ui_term_color_reset(), pin);
		res->error=true;
        return;
    }	
    // pin is in use for any purposes 
    if(system_config.pin_labels[pin+1]!=0 && !(system_config.aux_active & (0x01<<((uint8_t)pin))))
    {
        printf("%sError:%s IO%d is in use by %s", ui_term_color_error(), ui_term_color_reset(), pin, system_config.pin_labels[pin+1]);
		res->error=true;
        return;
    }

	if(output) // output
	{
		bio_output(pin);
		bio_put(pin,level);
		printf("IO%s%d%s set to%s OUTPUT: %s%d%s\r\n", 
			ui_term_color_num_float(), pin, ui_term_color_notice(),ui_term_color_reset(),
			ui_term_color_num_float(), level, ui_term_color_reset());
		system_bio_claim(true, pin, BP_PIN_IO, labels[level]);
        system_set_active(true, pin, &system_config.aux_active);
	}
	else // input
	{
		bio_input(pin);
		printf("IO%s%d%s set to%s INPUT: %s%d%s\r\n", 
			ui_term_color_num_float(), pin, ui_term_color_notice(),ui_term_color_reset(),
			ui_term_color_num_float(), bio_get(pin), ui_term_color_reset());		
		system_bio_claim(false, pin, BP_PIN_IO, 0);
        system_set_active(false, pin, &system_config.aux_active);
	}

    return;

}

