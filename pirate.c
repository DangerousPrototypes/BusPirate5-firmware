#include <stdlib.h>
#include <stdio.h>
#include <string.h>
//#include "pico/stdlib.h"
#include "pico/multicore.h"
#include "hardware/spi.h"
#include "hardware/i2c.h"
#include "hardware/dma.h"
#include "hardware/pio.h"
#include "hardware/interp.h"
#include "hardware/timer.h"
#include "hardware/watchdog.h"
#include "hardware/clocks.h"
#include "hardware/adc.h"
#include "hardware/uart.h"
#include "ws2812.pio.h"
#include "pirate.h"
#include "system_config.h"
#include "font/font.h"
#include "ui/ui_lcd.h"
#include "rgb.h"
#include "shift.h"
#include "psu.h"
#include "bio.h"
#include "amux.h"
#include "ui/ui_init.h"
#include "ui/ui_info.h"
#include "ui/ui_term.h"
#include "ui/ui_prompt.h"
#include "ui/ui_process.h"
#include "ui/ui_flags.h"
#include "buttons.h"
#include "storage.h"
#include "freq.h"
#include "queue.h"
//#include "buf.h"
#include "usb_tx.h"
#include "usb_rx.h"
#include "debug.h"
#include "ui/ui_cmdln.h"
#include "modes.h"
#include "system_monitor.h"
#include "ui/ui_statusbar.h"
#include "bsp/board.h"
#include "tusb.h"
#include "hardware/sync.h"
#include "pico/lock_core.h"
#include "helpers.h"

lock_core_t core;

void core1_entry(void);

int64_t ui_term_screensaver_enable(alarm_id_t id, void *user_data)
{
    system_config.lcd_screensaver_active=true;
    lcd_screensaver_enable();
    return 0;
}

int main()
{
    char c;

    //init buffered IO pins
    bio_init(); 
    
    // setup SPI0 for on board peripherals
    uint baud=spi_init(BP_SPI_PORT, 1000 * 1000);
    gpio_set_function(BP_SPI_CDI, GPIO_FUNC_SPI);
    gpio_set_function(BP_SPI_CLK, GPIO_FUNC_SPI);
    gpio_set_function(BP_SPI_CDO, GPIO_FUNC_SPI);

    // init shift register pins
    shift_init();
    
    //init psu pins
    psu_init();
   
    // LCD pin init
    lcd_init();

    // ADC pin init
    amux_init();

    // SD card CS pin init
    storage_init();

    // SPI bus is used from here
    // setup the spinlock for spi arbitration
    lock_init(&core, next_striped_spin_lock_num());

    // configure the defaults for shift register attached hardware
    shift_set_clear_wait( (AMUX_S3|AMUX_S1|DISPLAY_RESET|DAC_CS|CURRENT_EN|PULLUP_EN), CURRENT_EN_OVERRIDE);
    shift_output_enable(); //enable shift register outputs, also enabled level translator so don't do RGB LEDs before here!
    shift_set_clear_wait( 0, DISPLAY_RESET);
    busy_wait_ms(100);
    shift_set_clear_wait(DISPLAY_RESET,0);
    busy_wait_ms(100);

   
    // input buttons init
   //buttons_init();

    // setup the system_config defaults and the initial pin label references
    system_init();

    //setup the UI command buffers
    ui_init();

    monitor_init();

    // Now continue after init of all the pins and shift registers

    // Mount the SD card file system (and put into SPI mode)
    // This must be done before any other SPI communicatons
    storage_mount();

    if(storage_load_config())
    {
        system_config.config_loaded_from_file=true;
    }

    // RGB LEDs pins, pio, set to black
    //multicore_fifo_push_blocking(0x01);
    //this must be done after the 74hct245 is enabled during shift register setup
    rgb_init();

    // Read psu DAC resolution and check error
    psu_setup(); //TODO: handle error
   
    //uart
    //duplicate the terminal output on a debug uart on IO pins
    if(system_config.terminal_uart_enable)
    {
        debug_uart_init(system_config.terminal_uart_number, true, true, true);
    }
    //a transmit only uart for developers to debug (on IO pins)
    if(system_config.debug_uart_enable)
    {
        debug_uart_init(system_config.debug_uart_number, true, true, false);
    }
 
    multicore_launch_core1(core1_entry);

    // LCD setup
    spi_set_baudrate(BP_SPI_PORT, 1000*1000*32);
    lcd_configure();
    monitor(system_config.psu);
    ui_lcd_update(UI_UPDATE_ALL);
    shift_set_clear_wait( (DISPLAY_BACKLIGHT), 0); 

    translation_set(system_config.terminal_language); 
    
    multicore_fifo_push_blocking(0); //begin main loop on secondary core
    busy_wait_ms(100);

    enum bp_statmachine
    {
        BP_SM_DISPLAY_MODE,
        BP_SM_GET_INPUT,
        BP_SM_PROCESS_COMMAND,
        BP_SM_COMMAND_PROMPT,
    };
    
    uint8_t bp_state=0;
    uint32_t value;
    struct command_attributes attributes;
    struct command_response response;   
    struct prompt_result result; 
    alarm_id_t screensaver;

    while(1)
    {
        switch(bp_state)
        {
            case BP_SM_DISPLAY_MODE:
                
                if(system_config.terminal_ansi_color) //config file option loaded, wait for any key
                {
                    char c;
                    while(!rx_fifo_try_get(&c));
                    value='y';
                    result.success=true;
                }
                else
                {
                    ui_prompt_vt100_mode(&result, &value);
                }

                if(result.success)
                {
                    switch(value)
                    {
                        case 'y':
                            system_config.terminal_ansi_color=1;
                            system_config.terminal_ansi_statusbar=1;
                            ui_term_detect(); // Do we detect a VT100 ANSI terminal? what is the size?
                            ui_term_init(); // Initialize VT100 if ANSI terminal
                            monitor(system_config.psu);
                            ui_statusbar_update(UI_UPDATE_ALL);
                            break;
                        case 'n':
                            system_config.terminal_ansi_statusbar=0;
                            system_config.terminal_ansi_color=0;
                            break;
                        default:
                            break;
                    }
                    // show welcome
                    ui_info_print_info(&attributes, &response);
                    bp_state=BP_SM_COMMAND_PROMPT;
                }
                else if(result.error) // user hit enter but not a valid option
                {
                    printf("\r\n\r\nVT100 compatible color mode? (Y/n)> "); 
                }
                //printf("\r\n\r\nVT100 compatible color mode? (Y/n)> "); 
                break;
            
            case BP_SM_GET_INPUT:
                helpers_mode_periodic();
                switch(ui_term_get_user_input()) 
                {
                    case 0x01:// user pressed a key
                        if(system_config.lcd_timeout) 
                        {
                            if(system_config.lcd_screensaver_active)
                            {
                                lcd_screensaver_disable();
                                system_config.lcd_screensaver_active=false;
                            }
                            else
                            {
                                cancel_alarm(screensaver);
                            }
                            //TODO: figure out how to just reset the timer instead...
                            screensaver = add_alarm_in_ms(system_config.lcd_timeout*300000, ui_term_screensaver_enable, NULL, false);
                        }
                        break;
                    case 0xff: //user pressed enter
                        if(system_config.lcd_timeout) 
                        {
                            cancel_alarm(screensaver);
                        }
                        printf("\r\n");
                        bp_state=BP_SM_PROCESS_COMMAND;
                        break;
                }
                break;
            
            case BP_SM_PROCESS_COMMAND:
                if(!ui_process_commands())
                {
                    bp_state=BP_SM_COMMAND_PROMPT;
                }
                break;
            
            case BP_SM_COMMAND_PROMPT:
                if(system_config.subprotocol_name)
                {
                    printf("%s%s-(%s)>%s ", ui_term_color_prompt(), modes[system_config.mode].protocol_name, system_config.subprotocol_name, ui_term_color_reset());
                }
                else
                {
                    printf("%s%s>%s ", ui_term_color_prompt(), modes[system_config.mode].protocol_name, ui_term_color_reset());
                }
                
                cmdln_next_buf_pos();

                if(system_config.lcd_timeout)
                {
                    screensaver = add_alarm_in_ms(system_config.lcd_timeout*300000, ui_term_screensaver_enable, NULL, false);
                }

                bp_state=BP_SM_GET_INPUT;
                break;
            
            default:
                bp_state=BP_SM_COMMAND_PROMPT;
                break;
        }

        // shared multitasking stuff
        // system error, over current error, etc
        if(system_config.error)
        {
            printf("\x07");		// bell!

            if(system_config.psu_current_error)
            {
                printf("\x1b[?5h\r\n");
                ui_term_error_report(T_PSU_CURRENT_LIMIT_ERROR);
                busy_wait_ms(500);
                printf("\x1b[?5l");
                system_config.psu_current_error=0;
            }

            system_config.error=0;
            bp_state=BP_SM_COMMAND_PROMPT;
        }

    }
    return 0;
}

// refresh interrupt flag, serviced in the loop outside interrupt
bool lcd_update_request=false;

// begin of code execution for the second core (core1)
void core1_entry(void) 
{ 
    char c;
    uint32_t temp;

    tx_fifo_init();
    rx_fifo_init();

    // input buttons init
    //buttons_init();

    // USB init
    if(system_config.terminal_usb_enable)
    {
        tusb_init();
    }

    // wait for main core to signal start
    while(multicore_fifo_pop_blocking()!=0);

    lcd_irq_enable(BP_LCD_REFRESH_RATE_MS);

    //terminal debug uart enable
    if(system_config.terminal_uart_enable)
    {
        rx_uart_init_irq();
    }

    while(1)
    {
        //service (thread safe) tinyusb tasks
        if(system_config.terminal_usb_enable)
        {
            tud_task(); // tinyusb device task
        }

        //service the terminal TX queue
        tx_fifo_service();

        if(system_config.psu==1 && system_config.psu_irq_en==true && hw_adc_raw[HW_ADC_MUX_CURRENT_DETECT] < 100 )
        {
            system_config.psu_irq_en=false;
            psu_irq_callback();
            //ui_term_error_report(T_PSU_CURRENT_LIMIT_ERROR);
            //psu_reset();
        } 

        if(lcd_update_request)
        {
            monitor(system_config.psu); //TODO: fix monitor to return bool up_volts and up_current

            uint32_t update_flags=0;

            if(system_config.pin_changed) update_flags|= UI_UPDATE_LABELS; //pin labels
            if(monitor_voltage_changed()) update_flags|= UI_UPDATE_VOLTAGES; //pin voltages
            if(system_config.psu && monitor_current_changed()) update_flags|= UI_UPDATE_CURRENT; //psu current sense
            if(system_config.info_bar_changed) update_flags|=UI_UPDATE_INFOBAR; //info bar

            if(!system_config.lcd_screensaver_active) 
            {
                ui_lcd_update(update_flags);
            }
            
            if(system_config.terminal_ansi_color && system_config.terminal_ansi_statusbar && system_config.terminal_ansi_statusbar_update)
            {
                ui_statusbar_update(update_flags);
            }

            //TODO: where to put storage class text and systemconfig stuff???
            if(storage_detect())
            {

            }
            
            freq_measure_period_irq(); //update frequency periodically
            
            monitor_reset();
            
            lcd_update_request=false;
        }   
        
        // service any requests with priority
        while(multicore_fifo_rvalid())
        {
            temp=multicore_fifo_pop_blocking();
            switch(temp)
            {
                case 0xf0:
                    lcd_irq_disable();
                    lcd_update_request=false;
                    break;
                case 0xf1:
                    lcd_irq_enable(BP_LCD_REFRESH_RATE_MS);
                    lcd_update_request=true;
                    break;
                default:
                    break;
            }
            multicore_fifo_push_blocking(temp); //acknowledge
        }

    }// while(1)

}

struct repeating_timer lcd_timer;

bool lcd_timer_callback(struct repeating_timer *t) 
{
    lcd_update_request=true;
    return true;
}

void lcd_irq_disable(void)
{
    bool cancelled = cancel_repeating_timer(&lcd_timer);
}

void lcd_irq_enable(int16_t repeat_interval)
{
    // Create a repeating timer that calls repeating_timer_callback.
    // If the delay is negative (see below) then the next call to the callback will be exactly 500ms after the
    // start of the call to the last callback
    // Negative delay so means we will call repeating_timer_callback, and call it again
    // 500ms later regardless of how long the callback took to execute
    add_repeating_timer_ms(repeat_interval, lcd_timer_callback, NULL, &lcd_timer);
}

//gives protected access to spi (core safe)
void spi_busy_wait(bool enable)
{
    static bool busy=false;

    if(!enable)
    {
        busy=false;
        return;
    }

    do{
        uint32_t save = spin_lock_blocking(core.spin_lock);
        if(busy)
        {
            lock_internal_spin_unlock_with_wait(&core, save);
        }
        else
        {
            busy=true;
            lock_internal_spin_unlock_with_notify(&core, save);
            return;
        }

    }while(true);

}