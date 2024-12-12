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
 * So, this module is used to time either of those two attacks.
 ************************************************************/

#include <stdio.h>
#include "pico/stdlib.h"
#include "pico/time.h"
#include <stdint.h>
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
#include <string.h>

// uncomment below if all of the definitions in src/debug_rtt.c/h have
// been done for this
//#define BP_DEBUG_OVERRIDE_DEFAULT_CATEGORY BP_DEBUG_CAT_GLITCH
//

#define SYSTICK_PREEMPT_PRIORITY 0
#define SYSTICK_SUB_PRIORITY 0

static const char* const usage[] = { "glitch\t[-h(elp)] [-c(onfig)]",
                                     "UART glitch generator",
                                     "Exit: press Bus Pirate button" };

typedef struct _uart_glitch_config {
    uint32_t glitch_trg;        // character sent from BP UART to trigger the glitch
    uint32_t glitch_delay;      // how long (us) after trigger stop bit to fire trigger
    uint32_t glitch_wander;     // amount of time (us) to vary glitch timing
    uint32_t glitch_recycle;    // minimum time (ms) between one glitch cycle and the next
    uint32_t fail_resp;         // first character response from device on bad password
    uint32_t retry_count;       // number of times to try glitching before quitting
} _uart_glitch_config;

static const struct ui_help_options options[] = {
    { 1, "", T_HELP_UART_GLITCH }, // command help
    { 0, "-t", T_HELP_UART_BRIDGE_TOOLBAR },
    { 0, "-h", T_HELP_FLAG }, // help
};

static const char pin_labels[][5] = { "TRG", "RDY" };

static struct _uart_glitch_config uart_glitch_config;


static repeating_timer_t ticker;
static uint32_t tick_count_ms = 0;

/*************************************************
 *    CONFIGURATION PARAMETERS FOR GLITCHING     *
 *************************************************
 * .glitch_trg - the ASCII character code that
 *    starts the glitch cycle.  Typcially a
 *    carraige return (ASCII code 13, '\n')
 * .glitch_wander - the amount of time of time to
 *    vary the glitch timing in microseconds
 * .glitch_cyc - a short delay between turning off
 *    the glitch output and sending the character
 *    again.
 * .fail_char - the first ASCII character in
 *    the "normally bad password" response
 * .retry_count - max number of glitch cycles
 *    before giving up
 ************************************************/
static const struct prompt_item uart_glitch_trg_menu[] = { { T_UART_GLITCH_TRG_MENU_1 } };
static const struct prompt_item uart_glitch_dly_menu[] = { { T_UART_GLITCH_DLY_MENU_1 } };
static const struct prompt_item uart_glitch_vry_menu[] = { { T_UART_GLITCH_VRY_MENU_1 } };
static const struct prompt_item uart_glitch_cyc_menu[] = { { T_UART_GLITCH_CYC_MENU_1 } };
static const struct prompt_item uart_glitch_fail_menu[] = { { T_UART_GLITCH_FAIL_MENU_1 } };
static const struct prompt_item uart_glitch_cnt_menu[] = { { T_UART_GLITCH_CNT_MENU_1 } };

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
                                                      .maxval = 255,
                                                      .defval = 1,
                                                      .menu_action = 0,
                                                      .config = &prompt_int_cfg },
                                              [2] = { .description = T_UART_GLITCH_VRY_MENU,
                                                      .menu_items = uart_glitch_vry_menu,
                                                      .menu_items_count = count_of(uart_glitch_vry_menu),
                                                      .prompt_text = T_UART_GLITCH_VRY_PROMPT,
                                                      .minval = 1,
                                                      .maxval = 10,
                                                      .defval = 1,
                                                      .menu_action = 0,
                                                      .config = &prompt_int_cfg },
                                              [3] = { .description = T_UART_GLITCH_CYC_MENU,
                                                      .menu_items = uart_glitch_cyc_menu,
                                                      .menu_items_count = count_of(uart_glitch_cyc_menu),
                                                      .prompt_text = T_UART_GLITCH_CYC_PROMPT,
                                                      .minval = 10,
                                                      .maxval = 1000,
                                                      .defval = 10,
                                                      .menu_action = 0,
                                                      .config = &prompt_int_cfg },
                                              [4] = { .description = T_UART_GLITCH_FAIL_MENU,
                                                      .menu_items = uart_glitch_fail_menu,
                                                      .menu_items_count = count_of(uart_glitch_fail_menu),
                                                      .prompt_text = T_UART_GLITCH_FAIL_PROMPT,
                                                      .minval = 1,
                                                      .maxval = 255,
                                                      .defval = 35,
                                                      .menu_action = 0,
                                                      .config = &prompt_int_cfg },
                                              [5] = { .description = T_UART_GLITCH_CNT_MENU,
                                                      .menu_items = uart_glitch_cnt_menu,
                                                      .menu_items_count = count_of(uart_glitch_cnt_menu),
                                                      .prompt_text = T_UART_GLITCH_CNT_PROMPT,
                                                      .minval = 1,
                                                      .maxval = 10000,
                                                      .defval = 100,
                                                      .menu_action = 0,
                                                      .config = &prompt_int_cfg },
                                                      };

void glitch_settings(void) {
    ui_prompt_mode_settings_int(GET_T(T_UART_GLITCH_TRG_MENU), uart_glitch_config.glitch_trg, "(ASCII)");
    ui_prompt_mode_settings_int(GET_T(T_UART_GLITCH_DLY_MENU), uart_glitch_config.glitch_delay, "us");
    ui_prompt_mode_settings_int(GET_T(T_UART_GLITCH_VRY_MENU), uart_glitch_config.glitch_wander, "us");
    ui_prompt_mode_settings_int(GET_T(T_UART_GLITCH_CYC_MENU), uart_glitch_config.glitch_recycle, "ms");
    ui_prompt_mode_settings_int(GET_T(T_UART_GLITCH_FAIL_MENU), uart_glitch_config.fail_resp, "(ASCII)");
    ui_prompt_mode_settings_int(GET_T(T_UART_GLITCH_CNT_MENU), uart_glitch_config.retry_count, 0x00);
}

uint32_t uart_glitch_setup(void) {
    uint32_t temp;
    prompt_result result;

    const char config_file[] = "uglitch.bp";
    const mode_config_t config_t[] = {
        // clang-format off
        { "$.trigger", &uart_glitch_config.glitch_trg, MODE_CONFIG_FORMAT_DECIMAL },
        { "$.delay", &uart_glitch_config.glitch_delay, MODE_CONFIG_FORMAT_DECIMAL },
        { "$.wander", &uart_glitch_config.glitch_wander, MODE_CONFIG_FORMAT_DECIMAL },
        { "$.recycle", &uart_glitch_config.glitch_recycle, MODE_CONFIG_FORMAT_DECIMAL },
        { "$.failchar", &uart_glitch_config.fail_resp, MODE_CONFIG_FORMAT_DECIMAL },
        { "$.retries", &uart_glitch_config.retry_count, MODE_CONFIG_FORMAT_DECIMAL },
        // clang-format off
    };

    if (storage_load_mode(config_file, config_t, count_of(config_t))) {
        printf("\r\n\r\n%s%s%s\r\n", ui_term_color_info(), GET_T(T_USE_PREVIOUS_SETTINGS), ui_term_color_reset());
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
    uart_glitch_config.glitch_recycle = temp;

    ui_prompt_uint32(&result, &uart_menu[4], &temp);
    if (result.exit) {
        return 0;
    }
    uart_glitch_config.fail_resp = temp;

    ui_prompt_uint32(&result, &uart_menu[5], &temp);
    if (result.exit) {
        return 0;
    }
    uart_glitch_config.retry_count = temp;

    printf("\r\n");
    
    storage_save_mode(config_file, config_t, count_of(config_t));
}

bool tick_inc(repeating_timer_t* ticker) {
    tick_count_ms++;
    return (true);
}

void ticker_init() {
    tick_count_ms = 0;
    add_repeating_timer_ms(-1, tick_inc, NULL, &ticker);
}

void ticker_kill() {
    cancel_repeating_timer(&ticker);
}

static inline uint32_t get_ticks() {
    return (tick_count_ms);
}

/********************************************************
 * Bus Pirate pins 0 and 1 are used for glitchhing:
 * - Pin 0 is the triggered glitch output (to be used to
 *   do the actual glitching by some other device)
 * - Pin 1 is an input used for the external glitching
 *   device to indicate its readiness.  For example, an
 *   EMP type device may need some time to recharge before
 *   its ready again.
 *******************************************************/
bool setup_hardware() {
    PRINT_INFO("glitch::Entering setup_hardware()\r\n");
    bio_put(M_UART_RTS, 0);

    bio_set_function(M_UART_GLITCH_TRG, GPIO_FUNC_SIO);
    bio_set_function(M_UART_GLITCH_RDY, GPIO_FUNC_SIO);
    bio_output(M_UART_GLITCH_TRG);
    bio_input(M_UART_GLITCH_RDY);
    system_bio_claim(true, M_UART_GLITCH_TRG, BP_PIN_MODE, pin_labels[0]);
    system_bio_claim(true, M_UART_GLITCH_RDY, BP_PIN_MODE, pin_labels[1]);

    // set the trigger low right away
    bio_put(M_UART_GLITCH_TRG, 0);

    // set up timer
    ticker_init();

    return (true);
}

void teardown_hardware() {
    PRINT_INFO("glitch::Entering teardown_hardware()\r\n");
    bio_put(M_UART_RTS, 1);

    system_bio_claim(false, M_UART_GLITCH_TRG, BP_PIN_MODE, 0);
    system_bio_claim(false, M_UART_GLITCH_RDY, BP_PIN_MODE, 0);

    // kill the timer
    ticker_kill();
}

void uart_glitch_handler(struct command_result* res) {
    if (ui_help_show(res->help_flag, usage, count_of(usage), &options[0], count_of(options))) {
        return;
    }

    if (!uart_glitch_setup()) {
        printf("%s%s%s\r\n", ui_term_color_error(), GET_T(T_UART_GLITCH_SETUP_ERR), ui_term_color_reset());
        return;
    }

    if (!ui_help_check_vout_vref()) {
        return;
    }

    bool toolbar_state = system_config.terminal_ansi_statusbar_pause;
    bool pause_toolbar = !cmdln_args_find_flag('t' | 0x20);
    if (pause_toolbar) {
        system_config.terminal_ansi_statusbar_pause = true;
    }

    // set up and enable the two hardware pins, start the PIO program
    if (!setup_hardware())
    {
        return;
    }

    printf("\r\n%sHardware setup!%s\r\n", ui_term_color_info(), ui_term_color_reset());

    printf("\r\n%s%s%s\r\n", ui_term_color_notice(), GET_T(T_HELP_UART_GLITCH_EXIT), ui_term_color_reset());

    bool glitched = false;
    bool tool_timeout = false;
    bool cancelled = false;
    bool done = false;
    bool found = false;
    uint32_t tries = 0;
    char c;
    char trigger_char = (char)uart_glitch_config.glitch_trg;
    char fail_resp_char = (char)uart_glitch_config.fail_resp;
    char resp_string[40];
    size_t resp_count;
    uint32_t tick_start = 0;
    uint32_t glitch_min_time = uart_glitch_config.glitch_delay - uart_glitch_config.glitch_wander;
    uint32_t glitch_max_time = uart_glitch_config.glitch_delay + uart_glitch_config.glitch_wander;
    uint32_t this_glitch_time = glitch_min_time;

    // The main glitch loop starts here
    // keep going until we either:
    // + succeed in glitching the device
    // + user presses the BP button
    // + we exceeded the max number of tries
    // + glitch hardware is not ready for more than 1 second (input B01)
    // Basic logic flow:
    // + wait until the device ready input is high
    // + serial out the glitch trigger character
    // + wait for some response
    // + parse that response - if it starts with the "normal bad
    //   password character" then assume glitch failed
    // + increment/test the max retries count
    // + wait for the backoff period before retrying
    while (!glitched && !cancelled && !done && !tool_timeout) {
        // check for external device ready; allow BP button to
        // exit
        tick_start = get_ticks();
        while (!bio_get(M_UART_GLITCH_RDY) && !cancelled && !tool_timeout) {
            if (button_get(0)) {
                cancelled = true;
                break;
            }

            if (get_ticks() - tick_start > 1000) {
                tool_timeout = true;
            }
        }

        if (tool_timeout) {
            break;
        }

        // serial out the trigger character
        PRINT_DEBUG("glitch::UART-ing char\r\n");
        uart_putc_raw(M_UART_PORT, trigger_char);

        // delay before turning on the output.  Remember! The
        // timer is starting right after we stuff the character
        // into the buffer, not after it's actually sent
        busy_wait_us(this_glitch_time);
        bio_put(M_UART_GLITCH_TRG, 1);

        // wait for a char to be RX'd.  Allow the button
        // to break us out, if necessary
        while (!uart_is_readable(M_UART_PORT) && !cancelled) {
            if (button_get(0)) {
                cancelled = true;
            }
        }

        memset(resp_string, 0, 20);
        resp_count = 0;

        // start parsing the response from the device being glitched.
        // Ignore return & linefeed chars until we get the first
        // "real" character.  If that character is not the "normally
        // expected bad password character", then we consider the
        // glitch successful!
        tick_start = get_ticks();
        while (uart_is_readable(M_UART_PORT) && !cancelled && ((get_ticks() - tick_start) < 50)) {
            c = uart_getc(M_UART_PORT);

            if (c != '\r' && c != '\n') {
                resp_string[resp_count++] = c;
                if (resp_count >= 19) {
                    break;
                }
            }
            // short delay to wait for next character
            busy_wait_us_32(100);
        }
        bio_put(M_UART_GLITCH_TRG, 0);

        printf("Attempt %d at %dus RX: %s\r\n", tries + 1, this_glitch_time, resp_string);

        // parse through the response.  if our "normal bad password response" 
        // character is present, then we didn't glitch :/
        found = false;
        for (uint8_t ii = 0; ii < strlen(resp_string); ++ii) {
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
        if (++this_glitch_time > glitch_max_time) {
            this_glitch_time = glitch_min_time;

            if (++tries >= uart_glitch_config.retry_count) {
                done = true;
            }
        }

        // backoff wait time between cycles
        busy_wait_ms(uart_glitch_config.glitch_recycle);

        // clear the rx buffer for next try
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
    teardown_hardware();

    if (pause_toolbar) {
        system_config.terminal_ansi_statusbar_pause = toolbar_state;
    }
}