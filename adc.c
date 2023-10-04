#include <stdio.h>
#include "pico/stdlib.h"
#include "hardware/adc.h"
#include "pirate.h"
#include "bio.h"
#include "ui/ui_prompt.h"
#include "ui/ui_const.h"
#include "ui/ui_term.h"
#include "ui/ui_info.h"
#include "system_config.h"
#include "freq.h"
//#include "buf.h"
#include "usb_tx.h"
#include "usb_rx.h"

void adc_measure(struct opt_args *args, struct command_result *res, bool refresh);

uint32_t adc_print(uint8_t bio_pin, bool refresh)
{
	//sweep adc
	hw_adc_sweep();
    printf("%s%s IO%d:%s %s%d.%d%sV\r%s",
        ui_term_color_info(), t[T_MODE_ADC_VOLTAGE], bio_pin, ui_term_color_reset(), ui_term_color_num_float(),
		((*hw_pin_voltage_ordered[bio_pin+1])/1000), (((*hw_pin_voltage_ordered[bio_pin+1])%1000)/100),
		 ui_term_color_reset(), (refresh?"":"\n")
    ); 
    return 1;	
}

void adc_measure_single(struct opt_args *args, struct command_result *res)
{
    adc_measure(args, res, false);
}

void adc_measure_cont(struct opt_args *args, struct command_result *res)
{
    adc_measure(args, res, true);
}

void adc_measure(struct opt_args *args, struct command_result *res, bool refresh)
{
	if(args[0].no_value) //show voltage on all pins
	{
		if(refresh)
		{
			//TODO: use the ui_prompt_continue function, but how to deal with the names and labels????
			printf("%s%s%s\r\n%s", ui_term_color_notice(), t[T_PRESS_ANY_KEY_TO_EXIT], ui_term_color_reset(), ui_term_cursor_hide());
		}

		ui_info_print_pin_names();
		ui_info_print_pin_labels();
		
		if(refresh)
		{
            char c;
			do
			{
				ui_info_print_pin_voltage(true);
				delayms(250);
			}while(!rx_fifo_try_get(&c));
			printf("%s", ui_term_cursor_show()); //show cursor
		}

		//for single measurement, also adds final \n for continuous
		ui_info_print_pin_voltage(false);			


	}
	else //single pin measurement
	{
		//pin bounds check
		if(args[0].i>=count_of(bio2bufiopin))
		{
			printf("Error: Pin IO%d is invalid", args[0].i);
            res->error=true;
			return;
		}

		if(refresh)
		{
			//continuous measurement on this pin
			// press any key to continue
            prompt_result result;
			ui_prompt_any_key_continue(&result, 250, &adc_print, args[0].i, true);
		}
		//single measurement, also adds final \n for cont mode
		adc_print(args[0].i,false);

	}
}



