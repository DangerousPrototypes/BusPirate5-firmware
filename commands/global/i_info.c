#include <stdint.h>
#include "pico/stdlib.h"
#include "pirate.h"
#include "opt_args.h"
#include "pirate/bio.h"
#include "system_config.h"
#include "ui/ui_term.h"
#include "ui/ui_cmdln.h"
#include "ui/ui_help.h"
#include "pirate/amux.h"
#include "pirate/mcu.h"
#include "pirate/storage.h"
#include "pirate/mem.h"
#include "bytecode.h"
#include "modes.h"
#include "ui/ui_const.h"
#include "ui/ui_info.h"
#include "freq.h"
/*
static const char * const usage[]= 
{
    "a/A/@ <io> [-h(elp)]",   
    "Pin 0 ouput, low: a 0",
    "Pin 2 output, high: A 2",
	"Pin 5 input, read value: @ 5",
};

static const struct ui_help_options options[]={
{1,"", T_HELP_AUXIO}, //command help
    {0,"a",T_HELP_AUXIO_LOW }, 
    {0,"A", T_HELP_AUXIO_HIGH}, 
	{0,"@",T_HELP_AUXIO_INPUT }, 
	{0,"<io>", T_HELP_AUXIO_IO},
};*/
// display ui_info_print_info about the buspirate
// when not in HiZ mode it dumps info about the pins/voltags etc.
void i_info_handler(struct command_result *res){
    //if(ui_help_show(res->help_flag,usage,count_of(usage), &options[0],count_of(options) )) return;
	int i;
    amux_sweep();

	// Hardware information
	printf("\r\n%sThis device complies with part 15 of the FCC Rules. Operation is subject to the following two conditions: (1) this device may not cause harmful interference, and (2) this device must accept any interference received, including interference that may cause undesired operation.%s\r\n\r\n", ui_term_color_info(), ui_term_color_reset());
	
	printf("\r\n%s\r\n", BP_HARDWARE_VERSION);
	printf("%s %s%s%s (%s%s%s)\r\n", 
		t[T_INFO_FIRMWARE], 
		ui_term_color_num_float(),
		BP_FIRMWARE_VERSION, 
		ui_term_color_reset(),
		ui_term_color_num_float(),
		BP_FIRMWARE_HASH, 
		ui_term_color_reset());
	printf("%s%s%s %s %s%s%s %s, %s%s%s %s\r\n", 
		ui_term_color_num_float(),
		BP_HARDWARE_MCU, 
		ui_term_color_reset(),
		t[T_INFO_WITH], 
		ui_term_color_num_float(),
		BP_HARDWARE_RAM, 
		ui_term_color_reset(),
		t[T_INFO_RAM], 
		ui_term_color_num_float(),
		BP_HARDWARE_FLASH, 
		ui_term_color_reset(),
		t[T_INFO_FLASH]);
	printf("%s: %s%016llX%s\r\n", 
		t[T_INFO_SN], 
		ui_term_color_num_float(),
		mcu_get_unique_id(),
		ui_term_color_reset());
	printf("%s\r\n", t[T_INFO_WEBSITE]);

	// TF flash card information
	if(system_config.storage_available){
		printf("%s: %s%6.2fGB%s (%s %s)\r\n", 
			t[T_INFO_TF_CARD],
			ui_term_color_num_float(),
			system_config.storage_size, 
			ui_term_color_reset(),
			storage_fat_type_labels[system_config.storage_fat_type-1],
			t[T_INFO_FILE_SYSTEM]
		);

	}else{
		printf("%s: %s\r\n", t[T_INFO_TF_CARD], t[T_NOT_DETECTED]);
	}

	//config file loaded
	printf("\r\n%s%s:%s %s\r\n", ui_term_color_info(), t[T_CONFIG_FILE], ui_term_color_reset(), system_config.config_loaded_from_file?t[T_LOADED]:t[T_NOT_DETECTED]);

	if(system_config.big_buffer_owner!=BP_BIG_BUFFER_NONE){
		printf("%sBig buffer allocated to:%s #%d\r\n", ui_term_color_info(), ui_term_color_reset(), system_config.big_buffer_owner);
	}


	// Installed modes
	printf("%s%s:%s", ui_term_color_info(), t[T_INFO_AVAILABLE_MODES], ui_term_color_reset());
	for(i=0; i<MAXPROTO; i++){
		printf(" %s", modes[i].protocol_name);
	}
	printf("\r\n");

	// Current mode configuration
	printf("%s%s:%s ", ui_term_color_info(), t[T_INFO_CURRENT_MODE], ui_term_color_reset());
	//TODO: change to a return type and stick this in the fprint
	modes[system_config.mode].protocol_settings();
	printf("\r\n");

    printf("%s%s:%s %s\r\n", 
        ui_term_color_info(), 
        t[T_INFO_DISPLAY_FORMAT], 
        ui_term_color_reset(), 
        ui_const_display_formats[system_config.display_format]);
	
    // State of peripherals
	if(system_config.mode!=HIZ){
        // Data settings and configuration
		printf("%s%s:%s %d %s, %s %s\r\n",
			ui_term_color_info(),
			t[T_INFO_DATA_FORMAT], 
			ui_term_color_reset(),
			system_config.num_bits, 
			t[T_INFO_BITS], 
			ui_const_bit_orders[system_config.bit_order], 
			t[T_INFO_BITORDER]);

		printf("%s%s:%s %s\r\n", ui_term_color_info(), t[T_INFO_PULLUP_RESISTORS], ui_term_color_reset(), ui_const_pin_states[system_config.pullup_enabled]);

        if(system_config.psu && !system_config.psu_error){
            printf("%s%s:%s %s (%u.%uV/%u.%uV)\r\n", 
                ui_term_color_info(), t[T_INFO_POWER_SUPPLY], ui_term_color_reset(), t[T_ON],
                (*hw_pin_voltage_ordered[0])/1000, ((*hw_pin_voltage_ordered[0])%1000)/100, 
                (system_config.psu_voltage)/10000, ((system_config.psu_voltage)%10000)/100);

            uint32_t isense=((hw_adc_raw[HW_ADC_CURRENT_SENSE]) * ((500 * 1000)/4095)); //TODO: move this to a PSU function for all calls
            printf("%s%s:%s OK (%u.%umA/%u.%umA)\r\n", 
                ui_term_color_info(), t[T_INFO_CURRENT_LIMIT], ui_term_color_reset(), 
                (isense/1000), ((isense%1000)/100), 
                (system_config.psu_current_limit)/10000, ((system_config.psu_current_limit)%10000)/100 );
        }else{
            printf("%s%s:%s %s\r\n", ui_term_color_info(), t[T_INFO_POWER_SUPPLY], ui_term_color_reset(), t[T_OFF]);

            if(system_config.psu_error){
                printf("%s%s:%s %s (exceeded %u.%umA)\r\n", 
                    ui_term_color_info(), t[T_INFO_CURRENT_LIMIT], ui_term_color_reset(), 
                    ui_const_pin_states[5],
                    (system_config.psu_current_limit)/10000, ((system_config.psu_current_limit)%10000)/100 );               
            }


        }

		printf("%s%s:%s %s\r\n", ui_term_color_info(), t[T_INFO_FREQUENCY_GENERATORS], ui_term_color_reset(), !system_config.pwm_active?t[T_OFF]:" ");
		
        // PWMs
        if(system_config.pwm_active){
			for(i=0;i<BIO_MAX_PINS;i++){
				if(system_config.pwm_active& (0x01<<i)){
                    float freq_friendly_value;
                    uint8_t freq_friendly_units;
                    freq_display_hz(&system_config.freq_config[i].period, &freq_friendly_value, &freq_friendly_units);
                    printf("IO%s%d%s: %s%3.3f%s%s, %s%2.2f%s%% %sduty cycle%s\r\n", 
                        ui_term_color_num_float(), i, ui_term_color_reset(),
                        ui_term_color_num_float(),freq_friendly_value, ui_term_color_reset(),
                        ui_const_freq_labels[freq_friendly_units],
                        ui_term_color_num_float(), system_config.freq_config[i].dutycycle,ui_term_color_reset(),
                        ui_term_color_info(),ui_term_color_reset()
                    );

				}

			}
		}

		// Pin settings information
		// Only print if the toolbar is disabled to avoid redundancy
		if(system_config.terminal_ansi_statusbar==0){ 
            printf("\r\n");
			ui_info_print_pin_names();
			ui_info_print_pin_labels();
			ui_info_print_pin_voltage(false);
		}
	}
}