#include <stdio.h>
#include "pico/stdlib.h"
#include <stdint.h>
#include "pirate.h"
#include "system_config.h"
#include "opt_args.h"
#include "bytecode.h"
#include "modes.h"
#include "ui/ui_term.h"
#include "storage.h"
#include "freq.h"
#include "ui/ui_const.h"
#include "ui/ui_info.h"
#include "mcu/rp2040.h"
#include "fatfs/ff.h"
#include "fatfs/diskio.h"
#include "fatfs/tf_card.h"
#include "amux.h"

// todo: move
extern bool ejected;


// display ui_info_print_info about the buspirate
// when not in HiZ mode it dumps info about the pins/voltags etc.
void ui_info_print_info(opt_args (*args), struct command_result *res)
{
	/*LBA_t	maxsector;
	uint32_t sectorsiz;
	DRESULT	res;
	uint8_t buffer[512];*/

	// ------
	int i;
    amux_sweep();

	// Hardware information
	printf("\r\nThis device complies with part 15 of the FCC Rules. Operation is subject to the following two conditions: (1) this device may not cause harmful interference, and (2) this device must accept any interference received, including interference that may cause undesired operation.\r\n\r\n");
	
	printf("\r\n%s\r\n", BP_HARDWARE_VERSION);
	printf("%s %s%s%s (%s%s%s)\r\n", 
		t[T_INFO_FIRMWARE], 
		ui_term_color_num_float(),
		BP_FIRMWARE_VERSION, 
		ui_term_color_reset(),
		ui_term_color_num_float(),
		BP_FIRMWARE_HASH, 
		ui_term_color_reset()
	);
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
		t[T_INFO_FLASH]
	);
	printf("%s: %s%016llX%s\r\n", 
		t[T_INFO_SN], 
		ui_term_color_num_float(),
		mcu_get_unique_id(),
		ui_term_color_reset()
	);
	printf("%s\r\n", t[T_INFO_WEBSITE]);

	// SD Card information
	if(system_config.storage_available)
	{
		printf("%s: %s%6.2fGB%s (%s %s)\r\n", 
			t[T_INFO_SD_CARD],
			ui_term_color_num_float(),
			system_config.storage_size, 
			ui_term_color_reset(),
			storage_fat_type_labels[system_config.storage_fat_type-1],
			t[T_INFO_FILE_SYSTEM]
		);

	}
	else
	{
		printf("%s: %s\r\n", t[T_INFO_SD_CARD], t[T_NOT_DETECTED]);
	}

	//config file loaded
	printf("\r\n%s%s:%s %s\r\n", ui_term_color_info(), t[T_CONFIG_FILE], ui_term_color_reset(), system_config.config_loaded_from_file?t[T_LOADED]:t[T_NOT_DETECTED]);

	// Installed modes
	printf("%s%s:%s", ui_term_color_info(), t[T_INFO_AVAILABLE_MODES], ui_term_color_reset());
	for(i=0; i<MAXPROTO; i++)
	{
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
        ui_const_display_formats[system_config.display_format]
	);
	
    // State of peripherals
	if(system_config.mode!=HIZ)
	{
        // Data settings and configuration
		printf("%s%s:%s %d %s, %s %s\r\n",
			ui_term_color_info(),
			t[T_INFO_DATA_FORMAT], 
			ui_term_color_reset(),
			system_config.num_bits, 
			t[T_INFO_BITS], 
			ui_const_bit_orders[system_config.bit_order], 
			t[T_INFO_BITORDER]
		);

		printf("%s%s:%s %s\r\n", ui_term_color_info(), t[T_INFO_PULLUP_RESISTORS], ui_term_color_reset(), ui_const_pin_states[system_config.pullup_enabled]);

        if(system_config.psu && !system_config.psu_error)
        {
            printf("%s%s:%s %s (%u.%uV/%u.%uV)\r\n", 
                ui_term_color_info(), t[T_INFO_POWER_SUPPLY], ui_term_color_reset(), t[T_ON],
                (*hw_pin_voltage_ordered[0])/1000, ((*hw_pin_voltage_ordered[0])%1000)/100, 
                (system_config.psu_voltage)/10000, ((system_config.psu_voltage)%10000)/100
            );

            uint32_t isense=((hw_adc_raw[HW_ADC_CURRENT_SENSE]) * ((500 * 1000)/4095)); //TODO: move this to a PSU function for all calls
            printf("%s%s:%s OK (%u.%umA/%u.%umA)\r\n", 
                ui_term_color_info(), t[T_INFO_CURRENT_LIMIT], ui_term_color_reset(), 
                (isense/1000), ((isense%1000)/100), 
                (system_config.psu_current_limit)/10000, ((system_config.psu_current_limit)%10000)/100 
            );
        }
        else
        {
            printf("%s%s:%s %s\r\n", ui_term_color_info(), t[T_INFO_POWER_SUPPLY], ui_term_color_reset(), t[T_OFF]);

            if(system_config.psu_error)
            {
                printf("%s%s:%s %s (exceeded %u.%umA)\r\n", 
                    ui_term_color_info(), t[T_INFO_CURRENT_LIMIT], ui_term_color_reset(), 
                    ui_const_pin_states[5],
                    (system_config.psu_current_limit)/10000, ((system_config.psu_current_limit)%10000)/100 
                );               
            }


        }

		printf("%s%s:%s %s\r\n", ui_term_color_info(), t[T_INFO_FREQUENCY_GENERATORS], ui_term_color_reset(), !system_config.pwm_active?t[T_OFF]:" ");
		
        // PWMs
        if(system_config.pwm_active)
		{
			for(i=0;i<BIO_MAX_PINS;i++)
			{
				if(system_config.pwm_active& (0x01<<i))
				{
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
		if(system_config.terminal_ansi_statusbar==0)
		{
            printf("\r\n");
			ui_info_print_pin_names();
			ui_info_print_pin_labels();
			ui_info_print_pin_voltage(false);
		}
	}
	
	// sjaak 
	#if 0
	printf("-------Sjaak-\r\n");

	
	if(ejected)
		printf(" Card in use by BP\r\n");
	else
		printf(" Card in use by USB\r\n");
	#endif

}

// show voltages/pinstates
void ui_info_print_pin_names(void)
{
	// pin list
	for(int i=0; i<HW_PINS; i++)
	{
		ui_term_color_text_background(hw_pin_label_ordered_color[i][0],hw_pin_label_ordered_color[i][1]);
        printf("\e[8X%d.%s\t", i+1, hw_pin_label_ordered[i]);
	}
	printf("%s\r\n", ui_term_color_reset());
}

void ui_info_print_pin_labels(void)
{
    uint8_t j=0;
	// pin function
    
    //TODO: combine this with the version above in seperate function
    if(system_config.psu)
    {
        uint32_t isense=((hw_adc_raw[HW_ADC_CURRENT_SENSE]) * ((500 * 1000)/4095));
        printf("%s%d.%d%smA\t",ui_term_color_num_float(), (isense/1000), ((isense%1000)/100), ui_term_color_reset());         
        j=1;
    }

	// pin function
	for(int i=j; i<HW_PINS; i++)
	{
		printf("%s\t", system_config.pin_labels[i]==0?"-":(char*)system_config.pin_labels[i]);
	}
    printf("\r\n");
}		

void ui_info_print_pin_voltage(bool refresh)
{	
	// pin voltage
	// take a reading from all adc channels
	// updates the values available at hw_pin_voltage_ordered[] pointer array
	amux_sweep(); 
	
	//the Vout pin
	//HACK: way too hardware dependent. This is getting to be a mess
	printf("%s%d.%d%sV\t", 
		ui_term_color_num_float(), 
		(*hw_pin_voltage_ordered[0])/1000,
		((*hw_pin_voltage_ordered[0])%1000)/100,
		ui_term_color_reset()
	);

		// show state of IO pins
	for(uint i=1; i<HW_PINS-1; i++)
	{
		//TODO: global function for integer division
		//TODO: struct with pin type, label, units, and all the info buffered, just write it out
		//feature specific values (freq/pwm/V/?)
		if(system_config.freq_active & (0x01<<((uint8_t)(i-1))) || system_config.pwm_active & (0x01<<((uint8_t)(i-1))))
		{
			float freq_friendly_value;
			uint8_t freq_friendly_units;
			freq_display_hz(&system_config.freq_config[i-1].period, &freq_friendly_value, &freq_friendly_units);
			printf("%s%3.1f%s%c\t", 
				ui_term_color_num_float(), 
				freq_friendly_value,
				ui_term_color_reset(),
				*ui_const_freq_labels_short[freq_friendly_units]
			);
		}
		else
		{
			printf("%s%d.%d%sV\t", 
				ui_term_color_num_float(), 
				(*hw_pin_voltage_ordered[i])/1000,
				((*hw_pin_voltage_ordered[i])%1000)/100,
				ui_term_color_reset()
			);				
		}

	}
	//ground pin
	printf("%s\r%s",t[T_GND],!refresh?"\n":""); //TODO: pin type struct and handle things like this automatically 
	
}

typedef struct ui_info_help
{
	uint help;
	const char command[9];
	uint description;
} ui_info_help;

const struct ui_info_help help_commands[22]={
	{0,"=X/|X",T_HELP_1_2},	
	{0,"~", T_HELP_1_3},
	{0,"#", T_HELP_1_4},
	{0,"$", T_HELP_1_5},
	{0,"d/D", T_HELP_1_6},
	{0,"a/A/@.x", T_HELP_1_7},
	{0,"b", T_HELP_1_8},
	{0,"c", T_HELP_1_9},
	{0,"v.x/V.x", T_HELP_1_22},
	{0,"v/V", T_HELP_1_10},
	{0,"f.x/F.x", T_HELP_1_11},
	{0,"f/F", T_HELP_1_23},
	{0,"g.x/G", T_HELP_1_12},
	{0,"h/H/?", T_HELP_1_13},
	{0,"i", T_HELP_1_14},
	{0,"l/L", T_HELP_1_15},
	{0,"m", T_HELP_1_16},
	{0,"o", T_HELP_1_17},
	{0,"p/P", T_HELP_1_18},
	{0,"q", T_HELP_1_19},
	{0,"s", T_HELP_1_20},
	{0,"w/W", T_HELP_1_21}
};

const struct ui_info_help help_protocol[22]={
	{0,"(x)/(0)", T_HELP_2_1},
	{0,"[", T_HELP_2_3},
	{0,"]", T_HELP_2_4},
	{0,"{", T_HELP_2_5},
	{0,"}", T_HELP_2_6},
	{0,"\"abc\"", T_HELP_2_7},
	{0,"123", T_HELP_2_8},
	{0,"0x123", T_HELP_2_9},
	{0,"0b110", T_HELP_2_10},
	{0,"r", T_HELP_2_11},
	{0,"/", T_HELP_2_12},
	{0,"\\", T_HELP_2_13},
	{0,"^", T_HELP_2_14},
	{0,"-", T_HELP_2_15},
	{0,"_", T_HELP_2_16},
	{0,".", T_HELP_2_17},
	{0,"!", T_HELP_2_18},
	{0,":", T_HELP_2_19},
	{0,".", T_HELP_2_20},
	{0,"<x>/<0>", T_HELP_2_21},
	{0,"<x= >", T_HELP_2_22},
	{0,"",T_HELP_BLANK}
};


// displays the help
void ui_info_print_help(opt_args (*args), struct command_result *res)
{
	printf("\t%s%s%s\r\n", ui_term_color_info(), t[T_HELP_TITLE], ui_term_color_reset());
	printf("----------------------------------------------------------------------------\r\n");
	
	for(uint i=0; i<count_of(help_commands); i++)
	{
		printf("%s%s%s\t%s%s%s\t%s%s%s\t%s%s%s\r\n",
			ui_term_color_prompt(), help_commands[i].command, ui_term_color_reset(),
			ui_term_color_info(), t[help_commands[i].description], ui_term_color_reset(),
			ui_term_color_prompt(), help_protocol[i].command, ui_term_color_reset(),
			ui_term_color_info(), t[help_protocol[i].description], ui_term_color_reset()
		);
	}

}

void ui_info_print_error(uint32_t error)
{
	printf("\x07\r\n%sError:%s %s\r\n",ui_term_color_error(), ui_term_color_reset(), t[error]);
}


#if(0)
const char pinstates[][4] = {
"0\0",
"1\0",
"N/A\0"
};

const char pinmodes[][5] ={
"ANA.\0",		// analogue
"I-FL\0",		// input float
"I-UD\0",		// input pullup/down
"???\0",		// illegal
"O-PP\0",		// output pushpull
"O-OD\0",		// output opendrain
"O PP\0",		// output pushpull peripheral
"O OD\0",		// output opendrain peripheral
"----\0"		// pin is not used 	
};


uint8_t ui_info_get_pin_mode(uint32_t port, uint16_t pin)
{
	uint32_t crl, crh;
	uint8_t pinmode, crpin, i;

	//crl = GPIO_CRL(port);
	//crh = GPIO_CRH(port);
	crpin=0;

	for(i=0; i<16; i++)
	{
		if((pin>>i)&0x0001)
		{
			crpin=(i<8?(crl>>(i*4)):(crh>>((i-8)*4)));
			crpin&=0x000f;
		}
	}

	pinmode=crpin>>2;

	if(crpin&0x03)		// >1 is output
	{
		pinmode+=4;
	}

	return pinmode;
}



// show voltages/pinstates
void ui_info_print_pin_states(int refresh)
{
	uint8_t auxstate, csstate, misostate, clkstate, mosistate;
	uint8_t auxmode, csmode, misomode, clkmode, mosimode;

	if(!refresh)
	{
		// pin list
		for(int i=0; i<HW_PINS; i++)
		{
			ui_term_color_text_background(hw_pin_label_ordered_color[i][0],hw_pin_label_ordered_color[i][1]);
			printf("%d.%s%s", i+1,hw_pin_label_ordered[i], (i<HW_PINS-1?"\t":ui_term_color_reset()));
		}
		printf("\r\n");
	
		// pin function
		for(int i=0; i<HW_PINS; i++){
			printf("%s%s", system_config.pin_labels[i]==0?"-   ":(char*)system_config.pin_labels[i], (i<HW_PINS-1?"\t":"\r\n"));
		}
		
/*
		// pin states (loop through)
		// read pindirection
		//auxmode=ui_info_get_pin_mode(BP_AUX_PORT, BP_AUX_PIN);
		if(system_config.csport)
			csmode=ui_info_get_pin_mode(system_config.csport, system_config.cspin);
		else
			csmode=9;

		if(system_config.misoport)
			misomode=ui_info_get_pin_mode(system_config.misoport, system_config.misopin);
		else
			misomode=9;

		if(system_config.clkport)
			clkmode=ui_info_get_pin_mode(system_config.clkport, system_config.clkpin);
		else
			clkmode=9;
		if(system_config.mosiport)
			mosimode=ui_info_get_pin_mode(system_config.mosiport, system_config.mosipin);
		else
			mosimode=9;

		//printf("PWR\tPWR\tPWR\tPWR\tAN\t%s\t%s\t%s\t%s\t%s\r\n", pinmodes[auxmode], pinmodes[csmode], pinmodes[misomode], pinmodes[clkmode], pinmodes[mosimode]);

		// pinstates
		auxstate=(gpio_get(BP_AUX_PORT, BP_AUX_PIN)?1:0);
		if(system_config.csport)
			csstate=(gpio_get(system_config.csport, system_config.cspin)?1:0);
		else
			csstate=2;

		if(system_config.misoport)
			misostate=(gpio_get(system_config.misoport, system_config.misopin)?1:0);
		else
			misostate=2;

		if(system_config.clkport)
			clkstate=(gpio_get(system_config.clkport, system_config.clkpin)?1:0);
		else
			clkstate=2;
		if(system_config.mosiport)
			mosistate=(gpio_get(system_config.mosiport, system_config.mosipin)?1:0);
		else
			mosistate=2;
	*/
	}
	
	// pin voltage
	// take a reading from all adc channels
	// updates the values available at hw_pin_voltage_ordered
	amux_sweep();
	// show state of pin
	for(int i=0; i<HW_PINS-1; i++)
	{
		//TODO: global function for integer division
		printf("%s%d.%d%sV\t", 
			ui_term_color_num_float(), 
			(*hw_pin_voltage_ordered[i])/1000,
			((*hw_pin_voltage_ordered[i])%1000)/100,
			ui_term_color_reset()
		);

	}
	printf("%s\r%s",T_GND,!refresh?"\n":""); //TODO: pin type struct and handle things like this automatically 
	
}
#endif
