#include <stdio.h>
#include "pico/stdlib.h"
#include "pirate.h"
#include "system_config.h"
#include "commands.h"
#include "shift.h"
#include "ui/ui_term.h"
#include "amux.h"

void pullups_enable(opt_args (*args), struct command_result *res)
{
    HW_BIO_PULLUP_ENABLE();
    system_config.pullup_enabled=1; 
    system_config.info_bar_changed=true;
    
    amux_sweep();
    
    printf("%s%s:%s %s (%s @ %s%d.%d%sV)\r\n", 
        ui_term_color_notice(), t[T_MODE_PULLUP_RESISTORS],	ui_term_color_reset(), 
        t[T_MODE_ENABLED], BP_HARDWARE_PULLUP_VALUE, 
        ui_term_color_num_float(), hw_adc_voltage[HW_ADC_MUX_VREF_VOUT]/1000, 
        (hw_adc_voltage[HW_ADC_MUX_VREF_VOUT]%1000)/100, ui_term_color_reset()
    );

    //TODO test outside debug mode
    if(hw_adc_raw[HW_ADC_MUX_VREF_VOUT]<250)//arbitrary
    {
        printf("Warning: no/low voltage detected. Enable power supply (W) or attached external supply to Vout/Vref");
    }


}

void pullups_cleanup(void)
{
    HW_BIO_PULLUP_DISABLE();
	system_config.pullup_enabled=0;
    system_config.info_bar_changed=true;
}

void pullups_disable(opt_args (*args), struct command_result *res)
{
    pullups_cleanup();

    printf("%s%s:%s %s",
        ui_term_color_notice(),t[T_MODE_PULLUP_RESISTORS],ui_term_color_reset(),
        t[T_MODE_DISABLED]
    );
    
}
