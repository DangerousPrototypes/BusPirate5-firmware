#include <stdio.h>
#include "pico/stdlib.h"
#include <stdint.h>
#include "pirate.h"
#include "system_config.h"
#include "opt_args.h"
#include "ui/ui_const.h"
#include "ui/ui_term.h"
#include "usb_tx.h"
#include "commands/global/freq.h"
#include "ui/ui_flags.h"
#include "system_monitor.h"
#include "display/scope.h"


// maximum return value: 150
uint32_t ui_statusbar_info(char *buf)
{
    uint32_t len=0;
    uint32_t temp=0;
    uint32_t cnt=0;  // visible characters added (not including VT100 codes)

    // max_len += 36
	len += ui_term_color_text_background_buf(&buf[len], 0x000000, BP_COLOR_GREY);
    
    // max_len += 31
    if(system_config.psu)
    {
        // max_len += 14 (6 + 4 + 1 + 2 + 1)
        temp=sprintf(&buf[len],"Vout: %u.%02uV",
           (system_config.psu_voltage)/10000u, ((system_config.psu_voltage)%10000u)/100u
        );
        len+=temp;
        cnt+=temp;

        // max_len += 14 (1 + 4 + 1 + 2 + 6)
        if(system_config.psu_current_limit_en)
        {
            temp=sprintf(&buf[len],"/%u.%02umA max",
                (system_config.psu_current_limit)/10000u, ((system_config.psu_current_limit)%10000u)/100u 
            );
            len+=temp;
            cnt+=temp;           
        }

        // max_len += 3
        temp=sprintf(&buf[len]," | ");
        len+=temp;
        cnt+=temp;
    }

    // max_len += 26 (14 + 4 + 1 + 2 + 5)
    if(system_config.psu_error)
    {
        //show Power Supply: ERROR
        temp=sprintf(&buf[len],"Vout: ERROR > %u.%02umA | ",
           (system_config.psu_current_limit)/10000u, ((system_config.psu_current_limit)%10000u)/100u
        );
        len+=temp;
        cnt+=temp;
    }
    
    // max_len += 15
    if(system_config.pullup_enabled)
    {
        //show Pull-up resistors ON
        temp=sprintf(&buf[len],"Pull-ups: ON | ");
        len+=temp;
        cnt+=temp;
    }

    // max_len += 34
    if (scope_running) { // scope is using the analog subsystem
        temp=sprintf(&buf[len],"V update slower when scope running");
        len+=temp;
        cnt+=temp;
    }
    
    // max_len at this point is thus: 142
    //fill in blank space using CSI sequence "ESC [ n X" for "erase `n` characters"

    // BUGFIX: previously, len could have been longer than number of columns.
    // max_len += 4
    if (cnt > system_config.terminal_ansi_columns) {
        // do nothing?  maybe output debug message to debug port?
    } else {
        len += sprintf(&buf[len], "\e[%hhuX", system_config.terminal_ansi_columns-cnt);	
    }
    // max_len += 4
	len+=sprintf(&buf[len], "%s", ui_term_color_reset()); //sprintf to buffer
    return len;
}

// show voltages/pinstates
// maximum return value: 484
uint32_t ui_statusbar_names(char *buf)
{

    uint32_t len=0;
	// pin list

    // max_len += 480
    // Ten pins x 480 chars each pin (worst case)
	for(uint8_t i=0; i<HW_PINS; i++)
	{
        // max_len += 36
		len+=ui_term_color_text_background_buf(&buf[len],hw_pin_label_ordered_color[i][0],hw_pin_label_ordered_color[i][1]);
        // max_len +=  4
		len+=sprintf(&buf[len],"\e[8X"); //clear existing
        // max_len +=  8 (3 + 1 + 4 for all pins except one)
        uint8_t cnt=sprintf(&buf[len],"%hhu.%s\t", i+1, hw_pin_label_ordered[i]);
		len+=cnt;
	}

    // max_len += 4
    len+=sprintf(&buf[len],"%s",ui_term_color_reset());
    return len;
}

// maximum increase of len: 10
bool label_default(uint32_t *len, char *buf, uint32_t i)
{
    if(system_config.pin_changed & (0x01<<(uint8_t)i))
    {
        // max_len += 10 (4 + 5 + 1)
        // pin_labels are maximum 5 characters long ... BUT NOT ENFORCED ANYWHERE ... sigh.
        // BUGBUG -- manually copy pin label string to buffer instead of using sprintf, to limit to five characters (and alert if ever longer)
	    *len += sprintf(&buf[*len],"\e[8X%s\t", system_config.pin_labels[i]==0?"-":(char*)system_config.pin_labels[i]);
        return true;
    }
    return false;
}

// maximum increase of len: 35
bool label_current(uint32_t *len, char *buf, uint32_t i)
{
    //uint32_t isense;
    char *c;
    if(monitor_get_current_ptr(&c) || (system_config.pin_changed & (0x01<<(uint8_t)i))) 
    {
        // c points to maximum 5 non-null characters
        // max_len += 35 (4 + 19 + 5 + 4 + 3)
        *len += sprintf(&buf[*len], "\e[8X%s%s%smA\t", ui_term_color_num_float(), c, ui_term_color_reset());
        return true;
    }           
    return false;
}

// maximum increase of len: 28
bool value_voltage(uint32_t *len, char *buf, uint32_t i)
{
    char *c;
    if(monitor_get_voltage_ptr(i, &c))
    {
        // max_len += 28 ( 19 + 3 + 4 + 2)
        *len += sprintf(&buf[*len], "%s%s%sV\t", ui_term_color_num_float(), c, ui_term_color_reset());
        return true;
    }

    return false;

}

// TODO: freq function (update on change), pwm function (write once, untill update)
// maximum increase of len: 34
bool value_freq(uint32_t *len, char *buf, uint32_t i)
{
    // max_len += 4
    *len += sprintf(&buf[*len], "\e[8X"); //clear out tab, return to tab 	
    float freq_friendly_value;
    uint8_t freq_friendly_units;
    freq_display_hz(&system_config.freq_config[i-1].period, &freq_friendly_value, &freq_friendly_units);
    // max_len += 30 ( 19 + 5 + 4 + 1 + 1)
    *len += sprintf(&buf[*len],"%s%3.1f%s%c\t", 
        ui_term_color_num_float(), 
        freq_friendly_value,
        ui_term_color_reset(),
        *ui_const_freq_labels_short[freq_friendly_units]
    );

    return true;
}

// maximum increase of len: 34
bool value_pwm(uint32_t *len, char *buf, uint32_t i)
{

    if(!(system_config.pin_changed & (0x01<<(uint8_t)(i))))
    {
        return false;
    }
    // max_len += 34
    return value_freq(len, buf, i);
}

// maximum increase of len: translation dependent; en-US-POSIX: 3
bool value_ground(uint32_t *len, char *buf, uint32_t i)
{
    // BUGBUG -- look at all callers to see if length of the translated string is of concern
    if(!(system_config.pin_changed & (0x01<<(uint8_t)(i))))
    {
        return false;
    }
    // max_len += 3 (but not enforced (yet); translations may overflow?)
    // BUGBUG -- does not enforce limits on translated strings
	*len += sprintf(&buf[*len], "%s", t[T_GND]);
    return true; 
}
struct _iopins 
{
    //bool (*name)(uint32_t *len, char *buf, uint32_t i); 
    bool (*label)(uint32_t *len, char *buf, uint32_t i); 
    bool (*value)(uint32_t *len, char *buf, uint32_t i); 
};

// N.B. - Although const, because this stores the address of other variables,
//        it likely get copied into RAM.
const struct _iopins ui_statusbar_pin_functions[]=
{
    [ BP_PIN_IO     ] = { .label = &label_default, .value = &value_voltage },
    [ BP_PIN_MODE   ] = { .label = &label_default, .value = &value_voltage },
    [ BP_PIN_PWM    ] = { .label = &label_default, .value = &value_pwm     },
    [ BP_PIN_FREQ   ] = { .label = &label_default, .value = &value_freq    },
    [ BP_PIN_VREF   ] = { .label = &label_default, .value = &value_voltage },
    [ BP_PIN_VOUT   ] = { .label = &label_current, .value = &value_voltage },
    [ BP_PIN_GROUND ] = { .label = &label_default, .value = &value_ground  },
    [ BP_PIN_DEBUG  ] = { .label = &label_default, .value = &value_voltage },
};

// maximum return value: 390
uint32_t ui_statusbar_labels(char *buf)
{
    uint32_t len=0;
    uint8_t j=0;

	// show state of IO pins
    // max_len += 390 (10 pins, each at +39)
	for(uint i=0; i<HW_PINS; i++)
	{

        // max_len += 4
        if(system_config.pin_changed & (0x01<<(uint8_t)i))
		{
			len+=sprintf(&buf[len], "\e[8X"); //clear out tab, return to tab 		
		}
        // max_len += 35
        // function called through a function pointer in the if() statement
        // label_default: // maximum increase of len: 10
        // label_current: // maximum increase of len: 35
        if(!ui_statusbar_pin_functions[system_config.pin_func[i]].label(&len, buf, i))
        {
            // if that function did not write any characters to the buffer,
            // then write a tab character instead
            // max_len += 1
            len+=sprintf(&buf[len], "\t"); //todo: just handle this
        }
	}

    return len;
}	

// maximum return value: 380
uint32_t ui_statusbar_value(char *buf)
{	
    uint32_t len=0;

    bool do_update=false;

	// show state of IO pins
    // max_len += 380 (10 pins, each at +38)
	for(uint i=0; i<HW_PINS; i++)
	{
        // max_len += 4
        if(system_config.pin_changed & (0x01<<(uint8_t)i))
		{
			len += sprintf(&buf[len], "\e[8X"); //clear out tab, return to tab 		
		}

        // max_len += 34
        // function called through a function pointer in the if() statement
        // value_voltage : 28
        // value_pwm     : 34
        // value_freq    : 34
        // value_ground  : typically 3 (translation dependent)
        // else statement:  1
        if(ui_statusbar_pin_functions[system_config.pin_func[i]].value(&len, buf, i))
        {
            do_update=true;
        }
        else
        {
            // if that function did not write any characters to the buffer,
            // then write a tab character instead
            len+=sprintf(&buf[len], "\t"); //todo: just handle this
        }

	}

    return (do_update?len:0);
	
}

// maximum buffer usage: 1492 characters ... which overflows the 1024 character buffer
void ui_statusbar_update(uint32_t update_flags)
{ 
    uint32_t len=0;

    if(!update_flags) //nothing to update
    {
        return;
    }

    // BUGBUG: len can exceed tx_sb_buf size.  Stop using unsafe function `sprintf()`
    //         and mark those unsafe functions deprecated.  Better if there was a way
    //         to detect at compilation time this overflow via static_assert().
    //         However, manual calculation (documented in code with this commit) shows
    //         worst-case buffer use results in overflow.  Thus, not currently safe.
    //         maybe assert assert len <= tx_sb_buf_count at the end of this function?

    //save cursor, hide cursor
    // max_len +=   8
    len += sprintf(&tx_sb_buf[len],"\e7\e[?25l");

    //print each line of the toolbar
    // max_len += 158
    if(update_flags & UI_UPDATE_INFOBAR)
    {
        monitor_force_update(); //we want to repaint the whole screen if we're doing the pin names...
        // %u with uint8_t max characters is 3.  max +8 to len here
        // max_len += 8
        len+=sprintf(&tx_sb_buf[len],"\e[%hhu;0H",system_config.terminal_ansi_rows-3); //position at row-3 col=0
        // max_len += 150
        len+=ui_statusbar_info(&tx_sb_buf[len]);
    }

    // max_len += 492
    if(update_flags & UI_UPDATE_NAMES)
    {
        // BUGBUG ... behaves poorly when terminal has only fewer than two rows
        // max_len += 8
        len+=sprintf(&tx_sb_buf[len],"\e[%hhu;0H",system_config.terminal_ansi_rows-2);
        // max_len += 484
        len+=ui_statusbar_names(&tx_sb_buf[len]);
    }

    // max_len +=  38
    if((update_flags & UI_UPDATE_CURRENT) && !(update_flags & UI_UPDATE_LABELS)) //show current under Vout
    {
        char *c;
        if(monitor_get_current_ptr(&c)) 
        {
            // "\e[%hhu;0H%s%s%smA"
            //  1 1   3111 _ 5 411
            // max_len += 38 (2 + 3 + 3 + 19 + 5 + 4 + 2)
            len+=sprintf(&tx_sb_buf[len],"\e[%hhu;0H%s%s%smA",
                system_config.terminal_ansi_rows-1,
                ui_term_color_num_float(),
                c,
                ui_term_color_reset()
            );
        }           

    }

    // max_len += 398
    if(update_flags & UI_UPDATE_LABELS)
    {
        // BUGBUG - behaves poorly when terminal has zero or one row
        // max_len += 8
        len+=sprintf(&tx_sb_buf[len],"\e[%hhu;0H",system_config.terminal_ansi_rows-1);
        // max_len += 390
        len+=ui_statusbar_labels(&tx_sb_buf[len]);
    }

    // max_len += 388
    if(update_flags & UI_UPDATE_VOLTAGES)
    {
        // max_len += 8
        len+=sprintf(&tx_sb_buf[len],"\e[%hhu;0H",system_config.terminal_ansi_rows-0);
        // max_len += 380
        len+=ui_statusbar_value(&tx_sb_buf[len]);
    }

    //restore cursor, show cursor
    // max_len +=   2
    len+=sprintf(&tx_sb_buf[len],"\e8"); 
    // max_len +=   6
    if(!system_config.terminal_hide_cursor)
    {
        len+=sprintf(&tx_sb_buf[len],"\e[?25h"); 
    }

    // BUGBUG -- Status bar buffer size overflow possible.
    // Maximum value of len at time of writing:
    // 8 + 158 + 492 + 38 + 398 + 388 + 2 + 6 = 1492
    // This is MUCH MORE than the buffer size of 1024.
    assert(len <= count_of(tx_sb_buf));

    tx_sb_start(len);

}
