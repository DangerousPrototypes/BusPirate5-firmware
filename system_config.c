#include <stdio.h>
#include "pico/stdlib.h"
#include <stdint.h>
#include "bytecode.h"
#include "pirate.h"
#include "opt_args.h"
#include "commands.h"
#include "modes.h"
#include "ui/ui_const.h"
#include "system_config.h"
#include "mjson/mjson.h"
#include "pirate/mem.h" //defines for buffer owner
#include "ui/ui_term.h"
#include "pirate/rgb.h"

struct _system_config system_config;

void system_init(void)
{
	system_config.terminal_language=0;
	system_config.config_loaded_from_file=false;
	system_config.terminal_usb_enable=true; 		//enable USB CDC terminal

	system_config.terminal_uart_enable=false; 		//enable UART terminal on IO pins
	system_config.terminal_uart_number=1; 	//which UART to use (0 or 1)

	system_config.debug_uart_enable=false;			//initializes a UART for general developer use
	system_config.debug_uart_number=0;		//which UART to use (0 or 1)

    system_config.lcd_screensaver_active=false;
    system_config.lcd_timeout=0;
    
	// NOTE: LED effect is an enum.
    system_config.led_effect=LED_EFFECT_DISABLED;
    system_config.led_color=0xFF0000u; // now stores actual RGB value directly (not an index)
    system_config.led_brightness_divisor=10; // Divisor for the led's brightness.   10 = 10%, 5 = 20%, 4 = 25%, etc.

	system_config.terminal_ansi_rows=24;
	system_config.terminal_ansi_columns=80;
	system_config.terminal_ansi_statusbar=0;
	system_config.terminal_ansi_statusbar_update=false;
	system_config.terminal_ansi_color=UI_TERM_NO_COLOR;
	system_config.terminal_update=0;
	system_config.terminal_hide_cursor=false;
	system_config.terminal_ansi_statusbar_pause=false;

	set_nand_volume_state(NAND_VOLUME_STATE_EJECTED, false);

	system_config.storage_mount_error=3;
	system_config.storage_fat_type=5;
	system_config.storage_size=0;

	//start in auto format
	system_config.display_format=df_auto;

	system_config.hiz=1;
 	system_config.mode=0;
 	system_config.display=0;
	system_config.subprotocol_name=0;
	for(int i=0;i<HW_PINS;i++)
	{
		system_config.pin_labels[i]=0;
	}
    //TODO: connect to a struct with info on each pin features and options, label automatically
	system_config.pin_labels[0]=ui_const_pin_states[0];
	system_config.pin_labels[9]=ui_const_pin_states[2];

    system_config.pin_func[0]=BP_PIN_VREF;
    system_config.pin_func[9]=BP_PIN_GROUND;
    for(int i=1;i<HW_PINS-1; i++)
    {
        system_config.pin_func[i]=BP_PIN_IO;
    }
    

	system_config.pin_changed=0xffffffff;
    system_config.info_bar_changed=0;

	system_config.num_bits=8;
	system_config.bit_order=0;
	system_config.write_with_read=0;
	system_config.open_drain=0;

	system_config.pullup_enabled=0;
	system_config.mode_active=0;
	system_config.pwm_active=0; //todo: store the period and duty cycle in array of structs
	system_config.freq_active=0;//todo: store ranging info in array of structs
	system_config.aux_active=0;

	system_config.psu=0;
    system_config.psu_current_limit_en=false;     
    system_config.psu_voltage=0;               
    system_config.psu_current_limit=0;  
    system_config.psu_current_error=false;
    system_config.psu_error=false;   
	system_config.psu_irq_en=false;   
	
	
	system_config.error=0;

	system_config.mosiport=0;
	system_config.mosipin=0;
	system_config.misoport=0;
	system_config.misopin=0;
	system_config.csport=0;
	system_config.cspin=0;
	system_config.clkport=0;
	system_config.clkpin=0;

	system_config.big_buffer_owner=BP_BIG_BUFFER_NONE;

	system_config.rts=0;

	system_config.binmode_select=0;
	system_config.binmode_lock_terminal=false;
	system_config.binmode_usb_rx_queue_enable=true; 	//enable the binmode RX queue, disable to handle USB directly with tinyusb functions
	system_config.binmode_usb_tx_queue_enable=true; 	//enable the binmode TX queue, disable to handle USB directly with tinyusb functions

}

bool system_pin_claim(bool enable, uint8_t pin, enum bp_pin_func func, const char* label)
{
  	//#ifdef BP_DEBUG_ENABLED
    	//don't blow up our debug UART settings
	    if(system_config.pin_func[pin]==BP_PIN_DEBUG)
        {
            return false;
        }
	//#endif	 

    if(enable)
    {
        system_config.pin_labels[pin]=label;
        system_config.pin_func[pin]=func;	
    }
    else
    {
        system_config.pin_labels[pin]=0;
        system_config.pin_func[pin]=BP_PIN_IO;	
    }

    system_config.pin_changed|=(0x01<<((uint8_t)pin));
    return true;
}

bool system_bio_claim(bool enable, uint8_t bio_pin, enum bp_pin_func func, const char* label)
{
    return system_pin_claim(enable, bio_pin+1, func, label);
}

bool system_set_active(bool active, uint8_t bio_pin, uint8_t* function_register)
{
    /*#ifdef BP_DEBUG_ENABLED
	    if(bio_pin==BP_DEBUG_UART_TX || bio_pin==BP_DEBUG_UART_RX)
        {
            return false;
        }
	#endif	 */
	if(system_config.pin_func[bio_pin+1]==BP_PIN_DEBUG)
	{
		return false;
	}
    
    if(active)
    {
        (*function_register)|=(0x01<<((uint8_t)bio_pin));
    }
    else
    {
        (*function_register)&=~(0x01<<((uint8_t)bio_pin)); 
    }
    return true;
}
