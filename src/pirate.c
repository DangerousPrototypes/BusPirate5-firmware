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
#include "debug_uart.h"
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
static bool should_disable_unique_usb_serial_number(void) {

    bool result = false;
#ifdef BP_MANUFACTURING_TEST_MODE
    BP_DEBUG_PRINT(BP_DEBUG_LEVEL_VERBOSE, BP_DEBUG_CAT_EARLY_BOOT,
        "Init: manufacturing mode ... disabling unique USB serial number\n"
        );
    result = true;
#endif
    if (system_config.disable_unique_usb_serial_number) {
        BP_DEBUG_PRINT(BP_DEBUG_LEVEL_VERBOSE, BP_DEBUG_CAT_EARLY_BOOT,
            "Init: system_config.disable_unique_usb_serial_number is TRUE\n"
            );
        result = true;
    } else
    if (system_config.storage_fat_type == 0) {
        BP_DEBUG_PRINT(BP_DEBUG_LEVEL_VERBOSE, BP_DEBUG_CAT_EARLY_BOOT,
            "Init: Storage unformatted ... disabling unique USB serial number\n"
            );
        result = true;
    } else
    // if (!system_config.config_loaded_from_file) {
    //     BP_DEBUG_PRINT(BP_DEBUG_LEVEL_VERBOSE, BP_DEBUG_CAT_EARLY_BOOT,
    //         "Init: no configuration ... disabling unique USB serial number\n"
    //         );
    //     result = true;
    // } else
    if (storage_file_exists("FACTORY.USB")) {
        BP_DEBUG_PRINT(BP_DEBUG_LEVEL_VERBOSE, BP_DEBUG_CAT_EARLY_BOOT,
            "Init: `\\FACTORY.USB` file exists ... disabling unique USB serial number\n"
            );
        result = true;
    }
    return result;
}

static void main_system_initialization(void) {

    BP_DEBUG_PRINT(BP_DEBUG_LEVEL_VERBOSE, BP_DEBUG_CAT_EARLY_BOOT,
        "Init: tx/rx_fifo_init()\n"
        );
    tx_fifo_init();
    rx_fifo_init();

#if (BP_VER == 5)
    uint8_t bp_rev = mcu_detect_revision();
    BP_DEBUG_PRINT(BP_DEBUG_LEVEL_VERBOSE, BP_DEBUG_CAT_EARLY_BOOT,
        "Init: mcu_detect_revision: %d\n", bp_rev
        );
#endif

    reserve_for_future_mode_specific_allocations[1] = 99;
    reserve_for_future_mode_specific_allocations[2] = reserve_for_future_mode_specific_allocations[1];
    reserve_for_future_mode_specific_allocations[1] = reserve_for_future_mode_specific_allocations[2];

    // init buffered IO pins
    BP_DEBUG_PRINT(BP_DEBUG_LEVEL_VERBOSE, BP_DEBUG_CAT_EARLY_BOOT,
        "Init: buffered IO\n"
        );
    bio_init();

    // setup SPI0 for on board peripherals
    BP_DEBUG_PRINT(BP_DEBUG_LEVEL_VERBOSE, BP_DEBUG_CAT_EARLY_BOOT,
        "Init: setup SPI0\n"
        );
    uint baud = spi_init(BP_SPI_PORT, BP_SPI_HIGH_SPEED);
    gpio_set_function(BP_SPI_CDI, GPIO_FUNC_SPI);
    gpio_set_function(BP_SPI_CLK, GPIO_FUNC_SPI);
    gpio_set_function(BP_SPI_CDO, GPIO_FUNC_SPI);

// init shift register pins
#if (BP_VER == 5 || BP_VER == XL5)
    BP_DEBUG_PRINT(BP_DEBUG_LEVEL_VERBOSE, BP_DEBUG_CAT_EARLY_BOOT,
        "Init: shift register pins\n"
        );
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
    BP_DEBUG_PRINT(BP_DEBUG_LEVEL_VERBOSE, BP_DEBUG_CAT_EARLY_BOOT,
        "Init: PSU\n"
        );
    psucmd_init();

    // LCD pin init
    BP_DEBUG_PRINT(BP_DEBUG_LEVEL_VERBOSE, BP_DEBUG_CAT_EARLY_BOOT,
        "Init: LCD\n"
        );
    lcd_init();

    // ADC pin init
    BP_DEBUG_PRINT(BP_DEBUG_LEVEL_VERBOSE, BP_DEBUG_CAT_EARLY_BOOT,
        "Init: ADC\n"
        );
    amux_init();

    // TF flash card CS pin init
    BP_DEBUG_PRINT(BP_DEBUG_LEVEL_VERBOSE, BP_DEBUG_CAT_EARLY_BOOT,
        "Init: Storage\n"
        );
    storage_init(); // only sets up the GPIO... storage actually mounted later

    // initial pin states
    BP_DEBUG_PRINT(BP_DEBUG_LEVEL_VERBOSE, BP_DEBUG_CAT_EARLY_BOOT,
        "Init: Pin states\n"
        );
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

    BP_DEBUG_PRINT(BP_DEBUG_LEVEL_VERBOSE, BP_DEBUG_CAT_EARLY_BOOT,
        "Init: Pullups\n"
        );
    pullups_init(); // uses shift register internally

    // Shift register setup (BP5, BP5XL)
#if (BP_VER == 5 || BP_VER == XL5)
    BP_DEBUG_PRINT(BP_DEBUG_LEVEL_VERBOSE, BP_DEBUG_CAT_EARLY_BOOT,
        "Init: BP5 / BP5XL - shift output enable\n"
        );
    // enable shift register outputs
    // also enabled level translator so..
    // ***don't do RGB LEDs before here***
    shift_output_enable(true);
#endif

    // RGB init must be post-shift-register setup on BP5, BP5XL
    BP_DEBUG_PRINT(BP_DEBUG_LEVEL_VERBOSE, BP_DEBUG_CAT_EARLY_BOOT,
        "Init: rgb_init()\n"
        );
    rgb_init();

    // Verify PCB revision (BP5 only)
#if (BP_VER == 5)
    // test for PCB revision
    // must be done after shift register setup
    //  if firmware mismatch, turn all LEDs red
    if (bp_rev != BP_REV) { //
        BP_DEBUG_PRINT(BP_DEBUG_LEVEL_FATAL, BP_DEBUG_CAT_EARLY_BOOT,
            "Init: FATAL: PCB revision does not match firmware. Expected %d, found %d.\n",
            BP_REV, bp_rev
            );
        rgb_irq_enable(false);
        while (true) {
            rgb_set_all(0x7F, 0, 0); // half brightness red is still VERY bright
            busy_wait_ms(500);
            rgb_set_all(0, 0, 0);
            busy_wait_ms(500);
        }
    }
#endif

    // busy_wait_ms(10);
    // reset the LCD
    BP_DEBUG_PRINT(BP_DEBUG_LEVEL_VERBOSE, BP_DEBUG_CAT_EARLY_BOOT,
        "Init: LCD reset\n"
        );
    lcd_reset();

    // input button init
    BP_DEBUG_PRINT(BP_DEBUG_LEVEL_VERBOSE, BP_DEBUG_CAT_EARLY_BOOT,
        "Init: button init\n"
        );
    button_init();

    // setup the system_config defaults and the initial pin label references
    BP_DEBUG_PRINT(BP_DEBUG_LEVEL_VERBOSE, BP_DEBUG_CAT_EARLY_BOOT,
        "Init: system init()\n"
        );
    system_init();

    // setup the UI command buffers
    BP_DEBUG_PRINT(BP_DEBUG_LEVEL_VERBOSE, BP_DEBUG_CAT_EARLY_BOOT,
        "Init: ui_init()\n"
        );
    ui_init();

    // voltage and current monitor for toolbar and display
    // highly optimized to only update changed characters
    BP_DEBUG_PRINT(BP_DEBUG_LEVEL_VERBOSE, BP_DEBUG_CAT_EARLY_BOOT,
        "Init: monitor_init()\n"
        );
    monitor_init();

    // //////////////////////////////////////////////////////////////////////
    // Prior to this point, configuration settings from storage have
    // not been available!
#if (BP_VER != 5 || BP_REV >= 10)
    BP_DEBUG_PRINT(BP_DEBUG_LEVEL_VERBOSE, BP_DEBUG_CAT_EARLY_BOOT,
        "Init: Mounting SPI Flash\n"
        );
    storage_mount();

    BP_DEBUG_PRINT(BP_DEBUG_LEVEL_VERBOSE, BP_DEBUG_CAT_EARLY_BOOT,
        "Init: loading config file\n"
        );
    if (storage_load_config()) {
        BP_DEBUG_PRINT(BP_DEBUG_LEVEL_VERBOSE, BP_DEBUG_CAT_EARLY_BOOT,
            "Init: Config found, setting pixel effect to %d\n",
            system_config.led_effect
            );
        system_config.config_loaded_from_file = true;
        // update LED
        rgb_set_effect(system_config.led_effect);
    } else {
        BP_DEBUG_PRINT(BP_DEBUG_LEVEL_VERBOSE, BP_DEBUG_CAT_EARLY_BOOT,
            "Init: No config found, setting pixel effect to PARTY MODE\n"
            );
        // party mode/demo mode if no config file found
        rgb_set_effect(LED_EFFECT_PARTY_MODE);
    }
#else
    // Now continue after init of all the pins and shift registers
    // Mount the TF flash card file system (and put into SPI mode)
    // This must be done before any other SPI communications
    BP_DEBUG_PRINT(BP_DEBUG_LEVEL_VERBOSE, BP_DEBUG_CAT_EARLY_BOOT,
        "Init: BP5 w/TF Flash: storage mount\n"
        );
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
    // Stored configuration settings (if available) now loaded.
    // //////////////////////////////////////////////////////////////////////

    // May rely on storage and/or system_config settings...
    if (should_disable_unique_usb_serial_number()) {
        BP_DEBUG_PRINT(BP_DEBUG_LEVEL_VERBOSE, BP_DEBUG_CAT_EARLY_BOOT,
            "Init: Disabling unique USB serial number\n"
            );
        system_config.disable_unique_usb_serial_number = true;
    }

    translation_set(system_config.terminal_language);
    BP_DEBUG_PRINT(BP_DEBUG_LEVEL_VERBOSE, BP_DEBUG_CAT_EARLY_BOOT,
        "Init: Translation %d --> %d (%s)\n",
        system_config.terminal_language,
        get_current_language_idx(),
        get_current_language_name()
        );

    // duplicate the terminal output on a debug uart on IO pins
    if (system_config.terminal_uart_enable) {
        BP_DEBUG_PRINT(BP_DEBUG_LEVEL_VERBOSE, BP_DEBUG_CAT_EARLY_BOOT,
            "Init: Enabling UART %d for Console\n",
            system_config.terminal_uart_number
            );
        debug_uart_init(system_config.terminal_uart_number, true, true, true);
    }
    // a transmit only uart for developers to debug (on IO pins)
    if (system_config.debug_uart_enable) {
        BP_DEBUG_PRINT(BP_DEBUG_LEVEL_VERBOSE, BP_DEBUG_CAT_EARLY_BOOT,
            "Init: Enabling UART %d for DEBUG (output only)\n",
            system_config.debug_uart_number
            );
        debug_uart_init(system_config.debug_uart_number, true, true, false);
    }

    BP_DEBUG_PRINT(BP_DEBUG_LEVEL_VERBOSE, BP_DEBUG_CAT_EARLY_BOOT,
        "Init: Launching Core1\n"
        );
    multicore_launch_core1(core1_entry);

    // LCD setup
    BP_DEBUG_PRINT(BP_DEBUG_LEVEL_VERBOSE, BP_DEBUG_CAT_EARLY_BOOT,
        "Init: configuring LCD\n"
        );
    lcd_configure();

#ifdef BP_SPLASH_ENABLED
    BP_DEBUG_PRINT(BP_DEBUG_LEVEL_VERBOSE, BP_DEBUG_CAT_EARLY_BOOT,
        "Init: showing splash\n"
        );
    lcd_write_background(splash_data);
    /*monitor(system_config.psu);
    if (displays[system_config.display].display_lcd_update){
        displays[system_config.display].display_lcd_update(UI_UPDATE_ALL);
    }*/
    lcd_backlight_enable(true);
#endif

    // turn everything off
    BP_DEBUG_PRINT(BP_DEBUG_LEVEL_VERBOSE, BP_DEBUG_CAT_EARLY_BOOT,
        "Init: make pins safe and PSU disable\n"
        );
    bio_init();       // make all pins safe
    psucmd_disable(); // disable psu and reset pin label, clear any errors

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

    // //////////////////////////////////////////////////////////////////////
    // Notify the second core to complete initialization now that
    // system_config settings are loaded.
    BP_DEBUG_PRINT(BP_DEBUG_LEVEL_VERBOSE, BP_DEBUG_CAT_EARLY_BOOT,
        "Init: ICM - Sync init w/ core1\n"
        );
    icm_core0_send_message_synchronous(BP_ICM_INIT_CORE1);
    BP_DEBUG_PRINT(BP_DEBUG_LEVEL_VERBOSE, BP_DEBUG_CAT_EARLY_BOOT,
        "Init: ICM - Core1 sync completed\n"
        );
    // Only ***AFTER*** Core1 is ready can printf (UART / Terminals) be used.
    // //////////////////////////////////////////////////////////////////////

    BP_DEBUG_PRINT(BP_DEBUG_LEVEL_VERBOSE, BP_DEBUG_CAT_EARLY_BOOT,
        "Init: loading binmode configuration\n"
        );
    binmode_load_save_config(false);

    BP_DEBUG_PRINT(BP_DEBUG_LEVEL_VERBOSE, BP_DEBUG_CAT_EARLY_BOOT,
        "Init: binmode_setup()\n"
        );
    binmode_setup();

    BP_DEBUG_PRINT(BP_DEBUG_LEVEL_VERBOSE, BP_DEBUG_CAT_EARLY_BOOT,
        "Init: main_system_initialization() complete()\n"
        );

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
                PRINT_INFO("New terminal connection detected ... making USB storage read-only.\n");
                has_been_connected = true;
                prepare_usbmsdrive_readonly();
                // sync with the host
                storage_mount();
                insert_usbmsdrive();
            }
        } else {
            if (has_been_connected) {
                PRINT_INFO("Terminal disconnected ... ensuring USB storage is writable.\n");
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
                    PRINT_VERBOSE("Prompting to allow VT100 mode.\n");
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
                    PRINT_VERBOSE("Repeating prompt to allow VT100 mode.\n");
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
    // N.B. -- printf() can ***NOT*** be used until after
    //         both cores are in their infinite loops.
    //         Use RTT for early boot debugging / status messages.

    // N.B. -- Keep it SIMPLE.  Do as much initialization
    //         from core 0 as possible, before going multi-core.
    //         This includes loading system_config from storage (if avialable).
    //         Consider renaming functions to reflect
    //         pre-config vs. post-config (conditional) setup?
    SEGGER_RTT_Init();

    BP_DEBUG_PRINT(BP_DEBUG_LEVEL_WARNING, BP_DEBUG_CAT_EARLY_BOOT,"\n");
    BP_DEBUG_PRINT(BP_DEBUG_LEVEL_WARNING, BP_DEBUG_CAT_EARLY_BOOT,
        "Init: =========== %s ===========\n",
        BP_HARDWARE_VERSION
        );
    BP_DEBUG_PRINT(BP_DEBUG_LEVEL_WARNING, BP_DEBUG_CAT_EARLY_BOOT,
        "Init: Firmware %s @ %s (%s)\n",
        BP_FIRMWARE_VERSION,
        BP_FIRMWARE_HASH,
        BP_FIRMWARE_TIMESTAMP
        );
    BP_DEBUG_PRINT(BP_DEBUG_LEVEL_WARNING, BP_DEBUG_CAT_EARLY_BOOT,
        "Init: %s with %s RAM, %s FLASH, S/N %08X%08X\n",
           BP_HARDWARE_MCU,
           BP_HARDWARE_RAM,
           BP_HARDWARE_FLASH,
           (uint32_t)(mcu_get_unique_id() >> 32),
           (uint32_t)(mcu_get_unique_id() & 0xFFFFFFFFu)
           );
    main_system_initialization();

    BP_DEBUG_PRINT(BP_DEBUG_LEVEL_VERBOSE, BP_DEBUG_CAT_EARLY_BOOT,
        "Init: entering core0_infinite_loop()\n"
        );

    bp_mark_system_initialized();
    core0_infinite_loop(); // this never should exit, but....
    assert(false); // infinite loop above should never exit
}

// refresh interrupt flag, serviced in the loop outside interrupt
bool lcd_update_request = false;
bool lcd_update_force = false;

// begin of code execution for the second core (core1)
static void core1_initialization(void) {

    // //////////////////////////////////////////////////////////////////////
    // N.B. - Do ***NOT*** expect system configuration to be
    //        available until _AFTER_ the ICM message is received.
    BP_DEBUG_PRINT(BP_DEBUG_LEVEL_VERBOSE, BP_DEBUG_CAT_EARLY_BOOT,
        "Init: Core1: waiting for signal from core0...\n"
        );
    // wait for main core to signal start
    bp_icm_raw_message_t raw_init_message;
    do {
        raw_init_message = icm_core1_get_raw_message();
        if (get_embedded_message(raw_init_message) != BP_ICM_INIT_CORE1) {
            // Should never occur.  If it does, likely to never get expected message.
            // In which case, critical to have at least logged the error.
            BP_DEBUG_PRINT(BP_DEBUG_LEVEL_ERROR, BP_DEBUG_CAT_EARLY_BOOT,
                "Init: Core1: Unexpected ICM value! "
                "Got %02x (raw: %08x), expected %02x\n",
                get_embedded_message(raw_init_message), raw_init_message.raw, BP_ICM_INIT_CORE1
                );
        }
    } while (get_embedded_message(raw_init_message) != BP_ICM_INIT_CORE1);
    // OK, system_config is valid (loaded from storage, if available)
    // //////////////////////////////////////////////////////////////////////

    if (system_config.terminal_usb_enable) {
        BP_DEBUG_PRINT(BP_DEBUG_LEVEL_VERBOSE, BP_DEBUG_CAT_EARLY_BOOT,
            "Init: Core1: tusb_init()\n"
            );
        tusb_init();
    } else {
        BP_DEBUG_PRINT(BP_DEBUG_LEVEL_WARNING, BP_DEBUG_CAT_EARLY_BOOT,
            "Init: Core1: USB ***DISABLED*** by config\n"
            );
    }

    BP_DEBUG_PRINT(BP_DEBUG_LEVEL_VERBOSE, BP_DEBUG_CAT_EARLY_BOOT,
        "Init: Core1: lcd_irq_enable()\n"
        );
    lcd_irq_enable(BP_LCD_REFRESH_RATE_MS);

    // terminal debug uart enable
    if (system_config.terminal_uart_enable) {
        BP_DEBUG_PRINT(BP_DEBUG_LEVEL_WARNING, BP_DEBUG_CAT_EARLY_BOOT,
            "Init: Core1: UART terminal ***ENABLED*** by config\n"
            );
        rx_uart_init_irq();
    }

    BP_DEBUG_PRINT(BP_DEBUG_LEVEL_VERBOSE, BP_DEBUG_CAT_EARLY_BOOT,
        "Init: Core1: icm_core1_notify_completion(0x%08x)\n",
        raw_init_message.raw
        );
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
        // also receive input from RTT, if available
        rx_from_rtt_terminal();

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

            if (system_config.terminal_ansi_color &&
                system_config.terminal_ansi_statusbar &&
                system_config.terminal_ansi_statusbar_update &&
                !system_config.terminal_ansi_statusbar_pause) {
                ui_statusbar_update(update_flags);
            }

#if (BP_VER == 5 && BP_REV <= 9)
           // remains for legacy REV8 support of TF flash
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
    BP_DEBUG_PRINT(BP_DEBUG_LEVEL_VERBOSE, BP_DEBUG_CAT_EARLY_BOOT,
        "Init: core1_entry()\n"
        );
    core1_initialization();
    BP_DEBUG_PRINT(BP_DEBUG_LEVEL_VERBOSE, BP_DEBUG_CAT_EARLY_BOOT,
        "Init: starting core1_infinite_loop()\n"
        );
    bp_mark_system_initialized();
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
