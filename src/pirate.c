#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include "pico/multicore.h"
#include "hardware/spi.h"
#include "hardware/timer.h"
#include "hardware/uart.h"
#include "pico/mutex.h"
#include "ws2812.pio.h"
#include "pirate.h"
#include "system_config.h"
#include "command_struct.h"
#include "ui/ui_lcd.h"
#include "pirate/rgb.h"
#if (BP_VER == 5 || BP_VER == XL5)
#include "pirate/shift.h"
#endif
#include "pirate/bio.h"
#include "pirate/button.h"
#include "pirate/storage.h"
#include "pirate/lcd.h"
#include "pirate/amux.h"
#include "pirate/mcu.h"
#include "ui/ui_init.h"
#include "ui/ui_info.h"
#include "ui/ui_term.h"
#include "ui/ui_prompt.h"
#include "ui/ui_process.h"
#include "ui/ui_flags.h"
#include "commands/global/button_scr.h"
#include "commands/global/freq.h"
#include "queue.h"
#include "usb_tx.h"
#include "usb_rx.h"
#include "debug.h"
#include "ui/ui_cmdln.h"
#include "bytecode.h"
#include "modes.h"
#include "displays.h"
#include "system_monitor.h"
#include "ui/ui_statusbar.h"
#include "tusb.h"
#include "hardware/sync.h"
#include "pico/lock_core.h"
// #include "helpers.h"
#include "binmode/binmodes.h"
#include "commands/global/p_pullups.h"
#include "pirate/psu.h"
#include "commands/global/w_psu.h"
// #include "display/scope.h"
// #include "mode/logicanalyzer.h"
#include "msc_disk.h"
#include "pirate/intercore_helpers.h"
// #include "display/robot16.h"
#ifdef BP_SPLASH_ENABLED
#include BP_SPLASH_FILE
#endif

static mutex_t spi_mutex;

uint8_t reserve_for_future_mode_specific_allocations[10 * 1024] = { 0 };

void core1_entry(void);
/*
int64_t ui_term_screensaver_enable(alarm_id_t id, void* user_data) {
    system_config.lcd_screensaver_active = true;
    lcd_screensaver_enable();
    return 0;
}
*/
void gpio_setup(uint8_t pin, bool direction, bool level) {
    gpio_set_dir(pin, direction);
    gpio_set_function(pin, GPIO_FUNC_SIO);
    gpio_put(pin, level);
}

static void main_system_initialization(void) {

#if (BP_VER == 5)
    uint8_t bp_rev = mcu_detect_revision();
#endif

    reserve_for_future_mode_specific_allocations[1] = 99;
    reserve_for_future_mode_specific_allocations[2] = reserve_for_future_mode_specific_allocations[1];
    reserve_for_future_mode_specific_allocations[1] = reserve_for_future_mode_specific_allocations[2];

    // init buffered IO pins
    bio_init();

    // setup SPI0 for on board peripherals
    uint baud = spi_init(BP_SPI_PORT, BP_SPI_HIGH_SPEED);
    gpio_set_function(BP_SPI_CDI, GPIO_FUNC_SPI);
    gpio_set_function(BP_SPI_CLK, GPIO_FUNC_SPI);
    gpio_set_function(BP_SPI_CDO, GPIO_FUNC_SPI);

// init shift register pins
#if (BP_VER == 5 || BP_VER == XL5)
    shift_init();
#endif

#ifdef BP_REV
    system_config.hardware_revision = BP_REV;
#else
#error "No platform revision defined. Check pirate.h."
#endif

    // SPI bus is used from here
    // setup the mutex for spi arbitration
    mutex_init(&spi_mutex);

    // init psu pins
    psucmd_init();

    // LCD pin init
    lcd_init();

    // ADC pin init
    amux_init();

    // TF flash card CS pin init
    storage_init();

// initial pin states
#if (BP_VER == 5 || BP_VER == XL5)
    // configure the defaults for shift register attached hardware
    shift_clear_set_wait(CURRENT_EN_OVERRIDE, (AMUX_S3 | AMUX_S1 | DISPLAY_RESET | CURRENT_EN));
#else
    // todo: current detect
    gpio_setup(CURRENT_EN_OVERRIDE, GPIO_OUT, 0);
    gpio_setup(AMUX_S0, GPIO_OUT, 0);
    gpio_setup(AMUX_S1, GPIO_OUT, 1);
    gpio_setup(AMUX_S2, GPIO_OUT, 0);
    gpio_setup(AMUX_S3, GPIO_OUT, 1);
    // gpio_setup(DISPLAY_RESET, GPIO_OUT, 1);
    gpio_setup(CURRENT_EN, GPIO_OUT, 1);
    gpio_setup(LA_BPIO0, GPIO_IN, 0);
    gpio_setup(LA_BPIO1, GPIO_IN, 0);
    gpio_setup(LA_BPIO2, GPIO_IN, 0);
    gpio_setup(LA_BPIO3, GPIO_IN, 0);
    gpio_setup(LA_BPIO4, GPIO_IN, 0);
    gpio_setup(LA_BPIO5, GPIO_IN, 0);
    gpio_setup(LA_BPIO6, GPIO_IN, 0);
    gpio_setup(LA_BPIO7, GPIO_IN, 0);
#endif
    pullups_init(); // uses shift register internally

#if (BP_VER == 5 || BP_VER == XL5)
    shift_output_enable(
        true); // enable shift register outputs, also enabled level translator so don't do RGB LEDs before here!
#endif
    // busy_wait_ms(10);
    // reset the LCD
    lcd_reset();

    // input button init
    button_init();

    // setup the system_config defaults and the initial pin label references
    system_init();

    // setup the UI command buffers
    ui_init();

    // voltage and current monitor for toolbar and display
    // highly optimized to only update changed characters
    monitor_init();

// Now continue after init of all the pins and shift registers
// Mount the TF flash card file system (and put into SPI mode)
// This must be done before any other SPI communications
#if (BP_VER == 5 && BP_REV <= 9)
    spi_set_baudrate(BP_SPI_PORT, BP_SPI_START_SPEED);
    storage_mount();
    if (storage_load_config()) {
        system_config.config_loaded_from_file = true;
        // update LED
        rgb_set_effect(system_config.led_effect);
    } else {
        // party mode/demo mode if no config file found
        rgb_set_effect(LED_EFFECT_PARTY_MODE);
    }
    spi_set_baudrate(BP_SPI_PORT, BP_SPI_HIGH_SPEED);
#endif

    // RGB LEDs pins, pio, set to black
    // this must be done after the 74hct245 is enabled during shift register setup
    // NOTE: this is now handled on core1 entry
    // rgb_init();
    // psucmd_init();
    // uart
    // duplicate the terminal output on a debug uart on IO pins
    if (system_config.terminal_uart_enable) {
        debug_uart_init(system_config.terminal_uart_number, true, true, true);
    }
    // a transmit only uart for developers to debug (on IO pins)
    if (system_config.debug_uart_enable) {
        debug_uart_init(system_config.debug_uart_number, true, true, false);
    }

    multicore_launch_core1(core1_entry);

    // LCD setup
    lcd_configure();
#ifdef BP_SPLASH_ENABLED
    lcd_write_background(splash_data);
    /*monitor(system_config.psu);
    if (displays[system_config.display].display_lcd_update){
        displays[system_config.display].display_lcd_update(UI_UPDATE_ALL);
    }*/
    lcd_backlight_enable(true);
#endif

    // turn everything off
    bio_init();       // make all pins safe
    psucmd_disable(); // disable psu and reset pin label, clear any errors

// mount NAND flash here
#if !(BP_VER == 5 && BP_REV <= 9)
    storage_mount();
    if (storage_load_config()) {
        system_config.config_loaded_from_file = true;
        // update LED
        rgb_set_effect(system_config.led_effect);
    } else {
        // party mode/demo mode if no config file found
        rgb_set_effect(LED_EFFECT_PARTY_MODE);
    }
#endif

    translation_set(system_config.terminal_language);

#if (BP_VER == 5)
    // test for PCB revision
    // must be done after shift register setup
    //  if firmware mismatch, turn all LEDs red
    if (bp_rev != BP_REV) { //
        // printf("Error: PCB revision does not match firmware. Expected %d, found %d.\r\n", BP_REV,
        // mcu_detect_revision());
        rgb_irq_enable(false);
        while (true) {
            rgb_set_all(0xff, 0, 0);
            busy_wait_ms(500);
            rgb_set_all(0, 0, 0);
            busy_wait_ms(500);
        }
    }
#endif

#ifdef BP_SPLASH_ENABLED
    busy_wait_ms(1000);
    // draw background after showing splash screen
    lcd_backlight_enable(false);
#endif
    monitor(system_config.psu);
    if (displays[system_config.display].display_lcd_update) {
        displays[system_config.display].display_lcd_update(UI_UPDATE_ALL);
    }
    lcd_backlight_enable(true);

    // begin main loop on secondary core
    // this will also setup the USB device
    // we need to have read any config files on the TF flash card before now
    icm_core0_send_message_synchronous(BP_ICM_INIT_CORE1);
    binmode_load_save_config(false);
    binmode_setup();
}

static void core0_infinite_loop(void) {

    enum bp_statemachine {
        BP_SM_DISPLAY_MODE,
        BP_SM_GET_INPUT,
        BP_SM_PROCESS_COMMAND,
        BP_SM_COMMAND_PROMPT
    };

    uint8_t bp_state = 0;
    uint32_t value;
    struct prompt_result result;
    //alarm_id_t screensaver;
    bool has_been_connected = false;

    lcd_screensaver_alarm_reset(); //setup the screensaver timer (if configured)

    while (1) {

        // co-op multitask **when not actively doing anything**
        // core 2 handles USB and other sensitive stuff, so it's not critical to co-op multitask
        // but the terminal will not be responsive if the service is blocking
        binmode_service();

        if (tud_cdc_n_connected(0)) {
            if (!has_been_connected) {
                has_been_connected = true;
                prepare_usbmsdrive_readonly();
                // sync with the host
                storage_mount();
                insert_usbmsdrive();
            }
        } else {
            if (has_been_connected) {
                has_been_connected = false;
            }
            make_usbmsdrive_writable();
        }

        switch (bp_state) {
            case BP_SM_DISPLAY_MODE:
                // config file option loaded, wait for any key
                // for ASCII mode terminal_ansi_color is always false
                // this has the side effect of always prompting if the saved mode is ASCII
                // this is a feature, not a bug -
                // it lets new users escape from ASCII mode without learning of the config menus
                if (system_config.terminal_ansi_color) {
                    char c;
                    result.error = false;
                    result.success = false;

                    if (rx_fifo_try_get(&c)) {
                        value = 's';
                        result.success = true;
                    }
                } else {
                    ui_prompt_vt100_mode(&result, &value);
                }

                if (result.success) {
                    lcd_screensaver_alarm_reset();
                    switch (value) {
                        case 'n': // user requested ASCII mode
                            system_config.terminal_ansi_color = UI_TERM_NO_COLOR;
                            system_config.terminal_ansi_statusbar = false;
                            printf("\r\n"); // make pretty
                            break;
                        case 'y': // user requested VT100 mode
                            // no configuration exists, default to status bar enabled
                            system_config.terminal_ansi_color = UI_TERM_FULL_COLOR;
                            system_config.terminal_ansi_statusbar = true;
                        case 's':                    // case were configuration already exists
                            if (!ui_term_detect()) { // Do we detect a VT100 ANSI terminal? what is the size?
                                break;
                            }
                            // if something goes wrong with detection, the next function will skip internally
                            ui_term_init(); // Initialize VT100 if ANSI terminal (or not if detect failed)
                            // this sets the scroll region for the status bar (if enabled)
                            // and does the initial painting of the full statusbar
                            if (system_config.terminal_ansi_statusbar) {
                                ui_statusbar_init();
                                ui_statusbar_update(UI_UPDATE_ALL);
                            }
                            break;
                        default:
                            break;
                    }
                    // show welcome
                    // ui_info_print_info(&args, &res);

                    bp_state = BP_SM_COMMAND_PROMPT;
                } else if (result.error) { // user hit enter but not a valid option
                    printf("\r\n\r\nVT100 compatible color mode? (Y/n)> ");
                }
                // printf("\r\n\r\nVT100 compatible color mode? (Y/n)> ");
                // button_irq_enable(0, &button_irq_callback); //enable button interrupt
                break;
            case BP_SM_GET_INPUT:
                // it seems like we need an array where we can add our function for periodic service?
                displays[system_config.display].display_periodic();
                modes[system_config.mode].protocol_periodic();

                if (system_config.binmode_lock_terminal) {
                    break;
                }
                
                uint8_t key_pressed = (uint8_t)ui_term_get_user_input();
                
                //all keys deal with screensaver
                if(key_pressed) { //0x01 is a key press, 0xff is enter
                    lcd_screensaver_alarm_reset();
                }

                if (key_pressed==0xff) { //enter
                    printf("\r\n");
                    bp_state = BP_SM_PROCESS_COMMAND;
                    button_irq_disable(0); 
                }

                enum button_codes press_code = button_check_press(0);
                if (press_code != BP_BUTT_NO_PRESS) {
                    button_irq_disable(0);
                    button_exec(press_code);         // execute script based on the button press type
                    bp_state = BP_SM_COMMAND_PROMPT; // return to command prompt
                }
                break;
            case BP_SM_PROCESS_COMMAND:
                system_config.error = ui_process_commands();
                bp_state = BP_SM_COMMAND_PROMPT;
                break;
            case BP_SM_COMMAND_PROMPT:
                if (system_config.subprotocol_name) {
                    printf("%s%s-(%s)>%s ",
                           ui_term_color_prompt(),
                           modes[system_config.mode].protocol_name,
                           system_config.subprotocol_name,
                           ui_term_color_reset());
                } else {
                    printf("%s%s>%s ",
                           ui_term_color_prompt(),
                           modes[system_config.mode].protocol_name,
                           ui_term_color_reset());
                }
                cmdln_next_buf_pos();
                bp_state = BP_SM_GET_INPUT;
                // button_irq_enable(0, &button_irq_callback);
                break;

            default:
                bp_state = BP_SM_COMMAND_PROMPT;
                break;
        }

        // shared multitasking stuff
        // system error, over current error, etc
        if (system_config.error) {
            printf("\x07");        // bell!
            psucmd_over_current(); // check for PSU error, reset and show warning
            system_config.error = 0;
            bp_state = BP_SM_COMMAND_PROMPT;
        }
    }
    assert(false); // funtion should never exit / this point should be unreachable
}

void main(void) {
    main_system_initialization();
    core0_infinite_loop(); // this never should exit, but....
    assert(false); // infinite loop above should never exit
}


// refresh interrupt flag, serviced in the loop outside interrupt
bool lcd_update_request = false;
bool lcd_update_force = false;

// begin of code execution for the second core (core1)
static void core1_initialization(void) {
    tx_fifo_init();
    rx_fifo_init();

    rgb_init();

    // wait for main core to signal start
    bp_icm_raw_message_t raw_init_message;
    do {
        raw_init_message = icm_core1_get_raw_message();
    } while (get_embedded_message(raw_init_message) != BP_ICM_INIT_CORE1);

    // USB init
    if (system_config.terminal_usb_enable) {
        tusb_init();
    }

    lcd_irq_enable(BP_LCD_REFRESH_RATE_MS);

    // terminal debug uart enable
    if (system_config.terminal_uart_enable) {
        rx_uart_init_irq();
    }

    icm_core1_notify_completion(raw_init_message);
}
static void core1_infinite_loop(void) {

    while (1) {
        // service (thread safe) tinyusb tasks
        if (system_config.terminal_usb_enable) {
            tud_task(); // tinyusb device task
        }

        // service the terminal TX queue
        tx_fifo_service();
        // optionally service the binmode TX queue if requested
        if (system_config.binmode_usb_tx_queue_enable) {
            bin_tx_fifo_service();
        }

        if (system_config.psu == 1 &&
            system_config.psu_irq_en == true &&
            !psu_fuse_ok()
            ) {
            system_config.psu_irq_en = false;
            psucmd_irq_callback();
        }

        if (lcd_update_request) {
            monitor(system_config.psu); // TODO: fix monitor to return bool up_volts and up_current
            uint32_t update_flags = 0;
            if (lcd_update_force) {
                lcd_update_force = false;
                update_flags |= UI_UPDATE_FORCE | UI_UPDATE_ALL;
            }
            if (system_config.pin_changed) {
                update_flags |= UI_UPDATE_LABELS; // pin labels
            }
            if (monitor_voltage_changed()) {
                update_flags |= UI_UPDATE_VOLTAGES; // pin voltages
            }
            if (system_config.psu && monitor_current_changed()) {
                update_flags |= UI_UPDATE_CURRENT; // psu current sense
            }
            if (system_config.info_bar_changed) {
                update_flags |= UI_UPDATE_INFOBAR; // info bar
            }

            if (!system_config.lcd_screensaver_active) {
                assert(system_config.display < MAXDISPLAY);
                //assert(system_config.mode <= MAXMODE);

                // BUGBUG -- comments describing intent here would be helpful
                if (displays[system_config.display].display_lcd_update) {
                    displays[system_config.display].display_lcd_update(update_flags);
                }
            }

            if (system_config.terminal_ansi_color && system_config.terminal_ansi_statusbar &&
                system_config.terminal_ansi_statusbar_update && !system_config.terminal_ansi_statusbar_pause) {
                ui_statusbar_update(update_flags);
            }

// remains for legacy REV8 support of TF flash
#if BP_REV < 10
            if (storage_detect()) {
            }
#endif

            freq_measure_period_irq(); // update frequency periodically
            monitor_reset();
            lcd_update_request = false;
        }

        // service any requests with priority
        while (multicore_fifo_rvalid()) {
            bp_icm_raw_message_t raw_message = icm_core1_get_raw_message();
            switch (get_embedded_message(raw_message)) {
                case BP_ICM_DISABLE_LCD_UPDATES:
                    lcd_irq_disable();
                    lcd_update_request = false;
                    break;
                case BP_ICM_ENABLE_LCD_UPDATES:
                    lcd_irq_enable(BP_LCD_REFRESH_RATE_MS);
                    lcd_update_request = true;
                    break;
                case BP_ICM_FORCE_LCD_UPDATE:
                    lcd_irq_enable(BP_LCD_REFRESH_RATE_MS);
                    lcd_update_force = true;
                    lcd_update_request = true;
                    break;
                case BP_ICM_ENABLE_RGB_UPDATES:
                    rgb_irq_enable(false);
                    break;
                case BP_ICM_DISABLE_RGB_UPDATES:
                    rgb_irq_enable(true);
                    break;
                default:
                    break;
            }
            icm_core1_notify_completion(raw_message);
        }

    } // while(1)
}
void core1_entry(void) {
    core1_initialization();
    core1_infinite_loop();
    assert(false); // infinite loop above should never exit
}

struct repeating_timer lcd_timer;

bool lcd_timer_callback(struct repeating_timer* t) {
    lcd_update_request = true;
    return true;
}

void lcd_irq_disable(void) {
    bool cancelled = cancel_repeating_timer(&lcd_timer);
}

void lcd_irq_enable(int16_t repeat_interval) {
    // Create a repeating timer that calls repeating_timer_callback.
    // If the delay is negative (see below) then the next call to the callback will be exactly 500ms after the
    // start of the call to the last callback
    // Negative delay so means we will call repeating_timer_callback, and call it again
    // 500ms later regardless of how long the callback took to execute
    add_repeating_timer_ms(repeat_interval, lcd_timer_callback, NULL, &lcd_timer);
}

// gives protected access to spi (core safe)
void spi_busy_wait(bool enable) {
    if (!enable) {
        // the check is to protect against the first csel_deselect call not matched by a csel_select
        if (lock_is_owner_id_valid(spi_mutex.owner)) {
            mutex_exit(&spi_mutex);
        }
    } else {
        mutex_enter_blocking(&spi_mutex);
    }
}
