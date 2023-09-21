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

void adc_measure(struct command_attributes *attributes, struct command_response *response, int refresh);

uint32_t adc_print(uint8_t bio_pin, uint8_t refresh)
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

void adc_measure_single(struct command_attributes *attributes, struct command_response *response)
{
    adc_measure(attributes, response, false);
}

void adc_measure_cont(struct command_attributes *attributes, struct command_response *response)
{
    adc_measure(attributes, response, true);
}

void adc_measure(struct command_attributes *attributes, struct command_response *response, int refresh)
{
	if(!attributes->has_dot) //show voltage on all pins
	{
		if(refresh)
		{
			//TODO: use the ui_promt_continue function, but how to deal with the names and labels????
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

		//for single measurement, also adds final \n for continous
		ui_info_print_pin_voltage(false);			


	}
	else //single pin measurement
	{
		//pin bounds check
		if(attributes->dot>=count_of(bio2bufiopin))
		{
			printf("Error: Pin IO%d is invalid", attributes->dot);
            response->error=true;
			return;
		}

		if(refresh)
		{
			//continous measurement on this pin
			// press any key to continue
            prompt_result result;
			ui_prompt_any_key_continue(&result, 250, &adc_print, attributes->dot, true);
		}
		//single measurement, also adds final \n for continous mode
		adc_print(attributes->dot,false);

	}
}



