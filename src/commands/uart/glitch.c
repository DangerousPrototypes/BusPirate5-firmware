/*************************************************************
 * commands/uart/glitch.c
 *************************************************************
 * This handles interfacing to a separate device to perform
 * glitching attacks on a target.
 *
 * In this case, the glitch is triggered by the sending of a
 * character over UART.  Starting at the stop bit, there will
 * be a short delay (specified by user), then an output will
 * be fired for a short time (again, specified by user).
 *
 * Typical usage:
 * + The target device is in a mode where it is awaiting a
 *   user-entered password over serial UART.  After the user
 *   sends a <RETURN> over UART, the device's firmware checks
 *   the password.
 * + The target does the password check; if the password is
 *   good, the code execution flow continues to other
 *   functionality.  If the password was wrong, the flow
 *   jumps back to the password prompt.
 * + The goal of the glitching in this case to to cause the
 *   program flow to "skip over" the code jump when there's
 *   a bad password.
 * + Two common methods to cause this "glitch":
 *   + Power glitching - the threat actor "shorts out" supply
 *     power to the microcontroller for a very, very short
 *     time just as that branch instruction should execute;
 *     this causes the microcontroller to skip over that branch
 *   + EMF/EMP glitching - the threat actor induces a short
 *     blast of electromagnetic "noise" at the microcontroller
 *     just as that branch instruction should execute.
 *
 * This module is used to time either of those two attacks.
 *
 * A PIO SM handles the actual timing and output control.
 * The SM is set to run at 100MHz, so each instruction is 10
 * nanoseconds.  There is a bit of error in the actual timing
 * in terms of an extra cycle or so.
 ************************************************************/

#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include "pico/stdlib.h"
#include "hardware/uart.h"
#include "pirate.h"
#include "pirate/bio.h"
#include "pirate/storage.h"
#include "system_config.h"
#include "command_struct.h"
#include "ui/ui_term.h"
#include "ui/ui_help.h"
#include "ui/ui_cmdln.h"
#include "ui/ui_prompt.h"
#include "bytecode.h"
#include "pirate/button.h"

#include "hardware/pio.h"
#include "pio_config.h"
#include "glitch.pio.h"

// uncomment below if all of the definitions in src/debug_rtt.c/h have
// been done for this
//#define BP_DEBUG_OVERRIDE_DEFAULT_CATEGORY BP_DEBUG_CAT_GLITCH
//

#define SYSTICK_PREEMPT_PRIORITY 0
#define SYSTICK_SUB_PRIORITY 0

// maximum number of characters to receive in glitch loop
#define RX_CHAR_LIMIT 20

static const char* const usage[] = { "glitch\t[-h(elp)] [-c(onfig)]",
                                     "UART glitch generator.  Note that times are in terms of nanoseconds * 10; therefore, a setting of 3 = 30ns",
                                     "Exit:%s press Bus Pirate button" };
// config struct
typedef struct _uart_glitch_config {
    uint32_t glitch_trg;        // character sent from BP UART to trigger the glitch
    uint32_t glitch_delay;      // how long (ns*10) after trigger stop bit to fire trigger
    uint32_t glitch_wander;     // amount of time (ns*10) to vary glitch timing
    uint32_t glitch_time;       // amount of time (ns*10) to have output on
    uint32_t glitch_recycle;    // minimum time (ms) between one glitch cycle and the next
    uint32_t fail_resp;         // first character response from device on bad password
    uint32_t retry_count;       // number of times to try glitching before quitting
    uint32_t disable_ready;     // when 1, do not check ready input
} _uart_glitch_config;

static struct _uart_glitch_config uart_glitch_config;

// help text
static const struct ui_help_options options[] = {
    { 1, "", T_HELP_UART_GLITCH }, // command help
    { 0, "-h", T_HELP_FLAG }, // help
    { 0, "-c", T_HELP_LOGIC_INFO } // show config
};

// LCD display pin text
static const char pin_labels[][5] = { "TRG", "RDY" };

// timer stuffs
static repeating_timer_t ticker;
static uint32_t tick_count_ms = 0;

// pio struct
static struct _pio_config glitch_pio;

/*************************************************
 *    CONFIGURATION PARAMETERS FOR GLITCHING     *
 *************************************************
 * .glitch_trg - the ASCII character code that
 *    starts the glitch cycle.  Typcially a
 *    carraige return (ASCII code 13, '\n')
 * .glitch_delay - amount of time from end of 
 *    send char to start of glitch output on
 * .glitch_wander - the amount of time of time to
 *    vary the glitch timing in microseconds
 * .glitch_time - amount of time to keep glitch
 *    output high
 * .glitch_cyc - a short delay between turning off
 *    the glitch output and sending the character
 *    again.
 * .fail_char - the first ASCII character in
 *    the "normally bad password" response
 * .retry_count - max number of glitch cycles
 *    before giving up
 * .disable_ready - if == 1, do not check ready
 *    input during loop
 ************************************************/
static const struct prompt_item uart_glitch_trg_menu[] = { { T_UART_GLITCH_TRG_MENU_1 } };
static const struct prompt_item uart_glitch_dly_menu[] = { { T_UART_GLITCH_DLY_MENU_1 } };
static const struct prompt_item uart_glitch_vry_menu[] = { { T_UART_GLITCH_VRY_MENU_1 } };
static const struct prompt_item uart_glitch_lng_menu[] = { { T_UART_GLITCH_LNG_MENU_1 } };
static const struct prompt_item uart_glitch_cyc_menu[] = { { T_UART_GLITCH_CYC_MENU_1 } };
static const struct prompt_item uart_glitch_fail_menu[] = { { T_UART_GLITCH_FAIL_MENU_1 } };
static const struct prompt_item uart_glitch_cnt_menu[] = { { T_UART_GLITCH_CNT_MENU_1 } };
static const struct prompt_item uart_glitch_nordy_menu[] = { { T_UART_GLITCH_NORDY_MENU_1 } };

static const struct ui_prompt uart_menu[] = { [0] = { .description = T_UART_GLITCH_TRG_MENU,
                                                      .menu_items = uart_glitch_trg_menu,
                                                      .menu_items_count = count_of(uart_glitch_trg_menu),
                                                      .prompt_text = T_UART_GLITCH_TRG_PROMPT,
                                                      .minval = 1,
                                                      .maxval = 255,
                                                      .defval = 13,
                                                      .menu_action = 0,
                                                      .config = &prompt_int_cfg },
                                              [1] = { .description = T_UART_GLITCH_DLY_MENU,
                                                      .menu_items = uart_glitch_dly_menu,
                                                      .menu_items_count = count_of(uart_glitch_dly_menu),
                                                      .prompt_text = T_UART_GLITCH_DLY_PROMPT,
                                                      .minval = 1,
                                                      .maxval = 5000000,
                                                      .defval = 1,
                                                      .menu_action = 0,
                                                      .config = &prompt_int_cfg },
                                              [2] = { .description = T_UART_GLITCH_VRY_MENU,
                                                      .menu_items = uart_glitch_vry_menu,
                                                      .menu_items_count = count_of(uart_glitch_vry_menu),
                                                      .prompt_text = T_UART_GLITCH_VRY_PROMPT,
                                                      .minval = 1,
                                                      .maxval = 50,
                                                      .defval = 1,
                                                      .menu_action = 0,
                                                      .config = &prompt_int_cfg },
                                              [3] = { .description = T_UART_GLITCH_LNG_MENU,
                                                      .menu_items = uart_glitch_lng_menu,
                                                      .menu_items_count = count_of(uart_glitch_lng_menu),
                                                      .prompt_text = T_UART_GLITCH_LNG_PROMPT,
                                                      .minval = 0,
                                                      .maxval = 5000000,
                                                      .defval = 1,
                                                      .menu_action = 0,
                                                      .config = &prompt_int_cfg },
                                              [4] = { .description = T_UART_GLITCH_CYC_MENU,
                                                      .menu_items = uart_glitch_cyc_menu,
                                                      .menu_items_count = count_of(uart_glitch_cyc_menu),
                                                      .prompt_text = T_UART_GLITCH_CYC_PROMPT,
                                                      .minval = 10,
                                                      .maxval = 1000,
                                                      .defval = 10,
                                                      .menu_action = 0,
                                                      .config = &prompt_int_cfg },
                                              [5] = { .description = T_UART_GLITCH_FAIL_MENU,
                                                      .menu_items = uart_glitch_fail_menu,
                                                      .menu_items_count = count_of(uart_glitch_fail_menu),
                                                      .prompt_text = T_UART_GLITCH_FAIL_PROMPT,
                                                      .minval = 1,
                                                      .maxval = 255,
                                                      .defval = 35,
                                                      .menu_action = 0,
                                                      .config = &prompt_int_cfg },
                                              [6] = { .description = T_UART_GLITCH_CNT_MENU,
                                                      .menu_items = uart_glitch_cnt_menu,
                                                      .menu_items_count = count_of(uart_glitch_cnt_menu),
                                                      .prompt_text = T_UART_GLITCH_CNT_PROMPT,
                                                      .minval = 1,
                                                      .maxval = 10000,
                                                      .defval = 100,
                                                      .menu_action = 0,
                                                      .config = &prompt_int_cfg },
                                              [7] = { .description = T_UART_GLITCH_NORDY_MENU,
                                                      .menu_items = uart_glitch_nordy_menu,
                                                      .menu_items_count = count_of(uart_glitch_nordy_menu),
                                                      .prompt_text = T_UART_GLITCH_NORDY_PROMPT,
                                                      .minval = 0,
                                                      .maxval = 1,
                                                      .defval = 1,
                                                      .menu_action = 0,
                                                      .config = &prompt_int_cfg }
                                                      };

void glitch_settings(void) {
    ui_prompt_mode_settings_int(GET_T(T_UART_GLITCH_TRG_MENU), uart_glitch_config.glitch_trg, "(ASCII)");
    ui_prompt_mode_settings_int(GET_T(T_UART_GLITCH_DLY_MENU), uart_glitch_config.glitch_delay, "ns*10");
    ui_prompt_mode_settings_int(GET_T(T_UART_GLITCH_VRY_MENU), uart_glitch_config.glitch_wander, "ns*10");
    ui_prompt_mode_settings_int(GET_T(T_UART_GLITCH_LNG_MENU), uart_glitch_config.glitch_time, "ns*10");
    ui_prompt_mode_settings_int(GET_T(T_UART_GLITCH_CYC_MENU), uart_glitch_config.glitch_recycle, "ms");
    ui_prompt_mode_settings_int(GET_T(T_UART_GLITCH_FAIL_MENU), uart_glitch_config.fail_resp, "(ASCII)");
    ui_prompt_mode_settings_int(GET_T(T_UART_GLITCH_CNT_MENU), uart_glitch_config.retry_count, 0x00);
    ui_prompt_mode_settings_string(GET_T(T_UART_GLITCH_NORDY_MENU),
            uart_glitch_config.disable_ready ?
                GET_T(T_UART_GLITCH_NORDY_ENABLED) :
                GET_T(T_UART_GLITCH_NORDY_DISABLED), 0x00);
}

/******************************************************
 * Load config data, if file exists.  Prompt user to
 * enter config stuff, accept, or get out
 *****************************************************/
uint32_t uart_glitch_setup(void) {
    uint32_t temp;
    prompt_result result;

    const char config_file[] = "uglitch.bp";
    const mode_config_t config_t[] = {
        // clang-format off
        { "$.trigger", &uart_glitch_config.glitch_trg, MODE_CONFIG_FORMAT_DECIMAL },
        { "$.delay", &uart_glitch_config.glitch_delay, MODE_CONFIG_FORMAT_DECIMAL },
        { "$.wander", &uart_glitch_config.glitch_wander, MODE_CONFIG_FORMAT_DECIMAL },
        { "$.time", &uart_glitch_config.glitch_time, MODE_CONFIG_FORMAT_DECIMAL },
        { "$.recycle", &uart_glitch_config.glitch_recycle, MODE_CONFIG_FORMAT_DECIMAL },
        { "$.failchar", &uart_glitch_config.fail_resp, MODE_CONFIG_FORMAT_DECIMAL },
        { "$.retries", &uart_glitch_config.retry_count, MODE_CONFIG_FORMAT_DECIMAL },
        { "$.noready", &uart_glitch_config.disable_ready, MODE_CONFIG_FORMAT_DECIMAL }
        // clang-format off
    };

    if (storage_load_mode(config_file, config_t, count_of(config_t))) {
        printf("\r\n\r\n%s%s%s\r\n", ui_term_color_info(),
            GET_T(T_USE_PREVIOUS_SETTINGS),
            ui_term_color_reset());

        glitch_settings(); 

        bool user_value;
        if (!ui_prompt_bool(&result, true, true, true, &user_value)) {
            return (0);
        }
        if (user_value) {
            return (1); // user said yes, use the saved settings
        }
    }

    ui_prompt_uint32(&result, &uart_menu[0], &temp);
    if (result.exit) {
        return 0;
    }
    uart_glitch_config.glitch_trg = temp;

    ui_prompt_uint32(&result, &uart_menu[1], &temp);
    if (result.exit) {
        return 0;
    }
    uart_glitch_config.glitch_delay = temp;

    ui_prompt_uint32(&result, &uart_menu[2], &temp);
    if (result.exit) {
        return 0;
    }
    uart_glitch_config.glitch_wander = temp;

    ui_prompt_uint32(&result, &uart_menu[3], &temp);
    if (result.exit) {
        return 0;
    }
    uart_glitch_config.glitch_time = temp;


    ui_prompt_uint32(&result, &uart_menu[4], &temp);
    if (result.exit) {
        return 0;
    }
    uart_glitch_config.glitch_recycle = temp;

    ui_prompt_uint32(&result, &uart_menu[5], &temp);
    if (result.exit) {
        return 0;
    }
    uart_glitch_config.fail_resp = temp;

    ui_prompt_uint32(&result, &uart_menu[6], &temp);
    if (result.exit) {
        return 0;
    }
    uart_glitch_config.retry_count = temp;

    ui_prompt_uint32(&result, &uart_menu[7], &temp);
    if (result.exit) {
        return 0;
    }
    uart_glitch_config.disable_ready = temp;

    printf("\r\n");
    
    return (storage_save_mode(config_file, config_t, count_of(config_t)));
}

/********************************************
 * The next 4 methods are used to start/stop
 * a one-millisecond timer.  The timer is
 * used for checking for timeouts, etc.
 ********************************************/
bool tick_inc(repeating_timer_t* ticker) {
    tick_count_ms++;
    return (true);
}

void ticker_init() {
    tick_count_ms = 0;
    add_repeating_timer_ms(1, tick_inc, NULL, &ticker);
}

void ticker_kill() {
    cancel_repeating_timer(&ticker);
}

static inline uint32_t get_ticks() {
    return (tick_count_ms);
}

/********************************************************
 * Bus Pirate pins 0 and 1 are used for glitching:
 * - Pin 0 is the triggered glitch output (to be used to
 *   do the actual glitching by some other device)
 * - Pin 1 is an input used for the external glitching
 *   device to indicate its readiness.  For example, an
 *   EMP type device may need some time to recharge before
 *   its ready again.  This check may be disabled by config.
 * 
 * Enable PIO and get ready to go
 *******************************************************/
bool setup_uart_glitch_hardware() {
    PRINT_INFO("glitch::Entering setup_hardware()\r\n");
    bio_put(M_UART_RTS, 0);

    // set up timer
    ticker_init();

    glitch_pio.pio = PIO_MODE_PIO;
    glitch_pio.sm = 0;
    glitch_pio.program = &uart_glitch_program;
    glitch_pio.offset = pio_add_program(glitch_pio.pio, glitch_pio.program);

    uart_glitch_program_init(glitch_pio.pio,
                             glitch_pio.sm,
                             glitch_pio.offset,
                             bio2bufiopin[M_UART_GLITCH_TRG],
                             bio2bufiopin[M_UART_TX]);

    system_bio_update_purpose_and_label(true, M_UART_GLITCH_TRG, BP_PIN_MODE, pin_labels[0]);
    system_bio_update_purpose_and_label(true, M_UART_GLITCH_RDY, BP_PIN_IO, pin_labels[1]);
    bio_output(M_UART_GLITCH_TRG);
    bio_input(M_UART_GLITCH_RDY);

    // turn off output right away
    bio_put(M_UART_GLITCH_TRG, 0);

    return (true);
}

/********************************************************
 * Deallocate the 2 IO pins and remove the PIO program
 *******************************************************/
void teardown_uart_glitch_hardware() {
    PRINT_INFO("glitch::Entering teardown_hardware()\r\n");
    bio_put(M_UART_RTS, 1);

    system_bio_update_purpose_and_label(false, M_UART_GLITCH_TRG, BP_PIN_MODE, 0);
    system_bio_update_purpose_and_label(false, M_UART_GLITCH_RDY, BP_PIN_MODE, 0);

    // kill the timer
    ticker_kill();

    pio_remove_program(glitch_pio.pio, glitch_pio.program, glitch_pio.offset);
}

/******************************************************
 * Main glitch handler.
 ******************************************************/
void uart_glitch_handler(struct command_result* res) {
    PRINT_INFO("glitch::Starting main glitch handler\r\n");

    if (ui_help_show(res->help_flag, usage, count_of(usage), &options[0], count_of(options))) {
        return;
    }

    if (cmdln_args_find_flag('c')) {
        glitch_settings();
        return;
    }

    // Go get/set up config for glitching
    if (!uart_glitch_setup()) {
        printf("%s%s%s\r\n", ui_term_color_error(), GET_T(T_UART_GLITCH_SETUP_ERR), ui_term_color_reset());
        return;
    }

    if (!ui_help_check_vout_vref()) {
        return;
    }

    // set up and enable the two hardware pins, start the PIO program
    if (!setup_uart_glitch_hardware())
    {
        return;
    }

    printf("\r\n%sUART glitching.  Press Bus Pirate button to exit.%s\r\n",
        ui_term_color_info(), ui_term_color_reset());

    // get the number of edges in the trigger character.  This is UART
    // serial, so if two consecutive bits are high, it's just one edge;
    // that's what the goofy last_was_high thing is all about.
    uint32_t edges = 0;
    bool last_was_high = false;
    for (size_t ii = 0; ii < 8; ++ii) {
        if (uart_glitch_config.glitch_trg & (1U << ii)) {
            ++edges;
            if (last_was_high) {
                --edges;
            }
            last_was_high = true;
        }
        else {
            last_was_high = false;
        }
    }

    PRINT_DEBUG("glitch::Trigger UART edge count: %d\r\n", edges);
    PRINT_DEBUG("glitch::Hardware setup done!\r\n");

    bool glitched = false;
    bool tool_timeout = false;
    bool cancelled = false;
    bool done = false;
    bool found = false;
    uint32_t tries = 0;
    char c;
    char trigger_char = (char)uart_glitch_config.glitch_trg;
    char resp_string[RX_CHAR_LIMIT];
    size_t resp_count;
    uint32_t tick_start = 0;
    uint32_t glitch_min_delay = uart_glitch_config.glitch_delay;
    uint32_t glitch_max_delay = uart_glitch_config.glitch_delay + uart_glitch_config.glitch_wander;
    uint32_t this_glitch_delay = glitch_min_delay;

    // keep going until we've either:
    // + successfuly glitched
    // + BP button was pressed
    // + hit max attempt count
    // + the "ready" input was low for too long (tool timeout)
    while (!glitched && !cancelled && !done && !tool_timeout) {

        // check for external device ready; allow BP button to
        // exit.  Tool timeout time is 1 second.
        // This can be disabled by config setting
        tick_start = get_ticks();
        while (!bio_get(M_UART_GLITCH_RDY) && !cancelled &&
                    !tool_timeout && !uart_glitch_config.disable_ready) {
            if (button_get(0)) {
                cancelled = true;
            }

            if (get_ticks() - tick_start > 1000) {
                tool_timeout = true;
            }
        }

        if (tool_timeout || cancelled) {
            break;
        }

        // set up the FIFO for the PIO:
        // first item is the "on" time for the glitch pulse
        // second item is the number of edges for the trigger character
        // third item is the delay before firing the pulse
        // Note that timing is in terms of ns * 10 for items 1 and 3; so if value is
        // 7, then 70ns.
        pio_sm_put_blocking(glitch_pio.pio, glitch_pio.sm, uart_glitch_config.glitch_time);
        pio_sm_put_blocking(glitch_pio.pio, glitch_pio.sm, edges);
        pio_sm_put_blocking(glitch_pio.pio, glitch_pio.sm, this_glitch_delay);

        // serial out the trigger character.  The stop bit transition is the
        // trigger used by PIO to start timing
        uart_putc_raw(M_UART_PORT, trigger_char);

        // wait for a char to be RX'd.  Allow the button
        // to break us out, if necessary
        while (!uart_is_readable(M_UART_PORT) && !cancelled) {
            if (button_get(0)) {
                cancelled = true;
            }
        }

        // clear the response string and count
        memset(resp_string, 0, RX_CHAR_LIMIT);
        resp_count = 0;

        // start parsing the response from the device being glitched.
        // Ignore return & linefeed chars until we get the first
        // "real" character.  If that character is not the "normally
        // expected bad password character", then we consider the
        // glitch successful!
        // There's a timeout on this, in case we don't get RX_CHAR_LIMIT - 1
        // characters
        while (uart_is_readable(M_UART_PORT)) {
            c = uart_getc(M_UART_PORT);

            // Ignore any leading newlings/returns.  Some devices add them
            // before actual response text.  Once we have received any valid
            // character, allow a newline/return to end reception
            if (resp_count > 0 || (c != '\r' && c != '\n')) {
                if (c == '\r' || c == '\n') {
                    break;
                }

                // continue building the string until we hit char limit
                resp_string[resp_count++] = c;
                if (resp_count >= (RX_CHAR_LIMIT - 1)) {
                    break;
                }
            }
            // a bit of delay between characters
            busy_wait_us_32(500);
        }

        printf("Attempt %3d, delay %dns RX: %s\r\n", tries + 1, this_glitch_delay * 10, resp_string);

        // parse through the response.  if our "normal bad password response" 
        // character is present, then we didn't glitch :/
        found = false;
        for (size_t ii = 0; ii < strlen(resp_string); ++ii) {
            if (resp_string[ii] == uart_glitch_config.fail_resp) {
                found = true;
            }
        }

        if (!found) {
            glitched = true;
            break;
        }

        // exit when button pressed.
        if (button_get(0)) {
            cancelled = true;
            break;
        }

        // increment glitch delay time.  If we hit max, then
        // reset to min and increment glitch count
        if ((++this_glitch_delay) > glitch_max_delay) {
            this_glitch_delay = glitch_min_delay;

            if (++tries >= uart_glitch_config.retry_count) {
                done = true;
            }
        }

        // backoff wait time between cycles
        tick_start = get_ticks();
        if ((get_ticks() - tick_start) < uart_glitch_config.glitch_recycle) {
            tight_loop_contents();
        }

        // read and discard any remaining RX chars before next try
        while (uart_is_readable(M_UART_PORT)) {
            c = uart_getc(M_UART_PORT);
        }
    }

    // why did we break out of the glitch loop?
    if (glitched) {
        printf("%s%s%s\r\n", ui_term_color_notice(), GET_T(T_UART_GLITCH_GLITCHED), ui_term_color_reset());
    } else if (tool_timeout) {
        printf("%s%s%s\r\n", ui_term_color_error(), GET_T(T_UART_TOOL_TIMEOUT), ui_term_color_reset());
    } else if (cancelled) {
        printf("%s%s%s\r\n", ui_term_color_notice(), GET_T(T_UART_GLITCH_CANCELLED), ui_term_color_reset());
    } else if (done) {
        printf("%s%s%s\r\n", ui_term_color_notice(), GET_T(T_UART_GLITCH_DONE), ui_term_color_reset());
    } else {
        printf("%s%s%s\r\n", ui_term_color_error(), GET_T(T_UART_GLITCH_UNKNOWN), ui_term_color_reset());
    }
    
    // we're done, release the two hardware pins and PIO program
    teardown_uart_glitch_hardware();
}
