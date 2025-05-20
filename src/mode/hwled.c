// TODO: add timeout to all I2C stuff that can hang!
#include <stdbool.h>
#include <stdio.h>
#include "pico/stdlib.h"
#include <stdint.h>
#include "pirate.h"
#include "system_config.h"
#include "command_struct.h"
#include "bytecode.h"
#include "mode/hwled.h"
#include "pirate/bio.h"
#include "ui/ui_prompt.h"
#include "hardware/pio.h"
#include "ws2812.pio.h"
#include "apa102.pio.h"
#include "pirate/rgb.h"
#include "pirate/storage.h"
#include "ui/ui_term.h"
#include "ui/ui_help.h"
#include "pio_config.h"
#include "binmode/logicanalyzer.h"

static struct _pio_config pio_config;

// command configuration
const struct _mode_command_struct hwled_commands[] = { 0 };
const uint32_t hwled_commands_count = count_of(hwled_commands);

static const char pin_labels[][5] = {
    "SDO",
    "SCL",
};

static const char led_device_type[][7] = {
    "WS2812",
    "APA102",
};

enum M_LED_DEVICE_TYPE {
    M_LED_WS2812,
    M_LED_APA102,
    M_LED_WS2812_ONBOARD
};

static struct _led_mode_config mode_config;

static uint8_t device_cleanup;


static const struct prompt_item leds_type_menu[] = { { T_HWLED_DEVICE_MENU_1 },
                                                     { T_HWLED_DEVICE_MENU_2 },
                                                     { T_HWLED_DEVICE_MENU_3 } };
//static const struct prompt_item leds_num_menu[] = { { T_HWLED_NUM_LEDS_MENU_1 } };

static const struct ui_prompt leds_menu[] = {
    {
        .description = T_HWLED_DEVICE_MENU,
        .menu_items = leds_type_menu,
        .menu_items_count = count_of(leds_type_menu),
        .prompt_text = T_HWLED_DEVICE_PROMPT,
        .minval = 0,
        .maxval = 0,
        .defval = 1,
        .menu_action = 0,
        .config = &prompt_list_cfg
    },
    /*{
        .description = T_HWLED_NUM_LEDS_MENU,
        .menu_items = leds_num_menu,
        .menu_items_count = count_of(leds_num_menu),
        .prompt_text = T_HWLED_NUM_LEDS_PROMPT,
        .minval = 1,
        .maxval = 10000,
        .defval = 1,
        .menu_action = 0,
        .config = &prompt_int_cfg
    }*/
};

uint32_t hwled_setup(void) {

    prompt_result result;

    const char config_file[] = "bpled.bp";
    const mode_config_t config_t[] = {
        // clang-format off
        { "$.device", &mode_config.device, MODE_CONFIG_FORMAT_DECIMAL },
        //{ "$.num_leds", &mode_config.num_leds, MODE_CONFIG_FORMAT_DECIMAL },
        // clang-format on
    };

    if (storage_load_mode(config_file, config_t, count_of(config_t))) {
        printf("\r\n\r\n%s%s%s\r\n", ui_term_color_info(), GET_T(T_USE_PREVIOUS_SETTINGS), ui_term_color_reset());
        hwled_settings();
        bool user_value;
        if (!ui_prompt_bool(&result, true, true, true, &user_value)) {
            return 0;
        }
        if (user_value) {
            return 1; // user said yes, use the saved settings
        }
    }

    ui_prompt_uint32(&result, &leds_menu[0], &mode_config.device);
    if (result.exit) {
        return 0;
    }
    mode_config.device--;
    /*if (mode_config.device == 2) {
        mode_config.num_leds = RGB_LEN;
    } else {
        ui_prompt_uint32(&result, &leds_menu[1], &mode_config.num_leds);
        if (result.exit) {
            return 0;
        }
    }*/

    storage_save_mode(config_file, config_t, count_of(config_t));

    //}
    return 1;
}

uint32_t hwled_setup_exc(void) {
    pio_config.pio = PIO_MODE_PIO;
    pio_config.sm = 0;
    switch (mode_config.device) {
        case M_LED_WS2812:
            system_bio_update_purpose_and_label(true, M_LED_SDO, BP_PIN_MODE, pin_labels[0]);
            mode_config.baudrate = 800000;
            bio_buf_output(M_LED_SDO);
            // success = pio_claim_free_sm_and_add_program_for_gpio_range(&ws2812_program, &pio_config.pio,
            // &pio_config.sm, &pio_config.offset, bio2bufiopin[M_LED_SDO], 1, true); hard_assert(success);
            pio_config.program = &ws2812_program;
            pio_config.offset = pio_add_program(pio_config.pio, pio_config.program);
#ifdef BP_PIO_SHOW_ASSIGNMENT
            printf("PIO: pio=%d, sm=%d, offset=%d\r\n", PIO_NUM(pio_config.pio), pio_config.sm, pio_config.offset);
#endif
            ws2812_program_init(pio_config.pio,
                                pio_config.sm,
                                pio_config.offset,
                                bio2bufiopin[M_LED_SDO],
                                (float)mode_config.baudrate,
                                false);  
            system_config.num_bits=24;          
            break;
        case M_LED_APA102:
            system_bio_update_purpose_and_label(true, M_LED_SDO, BP_PIN_MODE, pin_labels[0]);
            system_bio_update_purpose_and_label(true, M_LED_SCL, BP_PIN_MODE, pin_labels[1]);
            mode_config.baudrate = (5 * 1000 * 1000);
            bio_buf_output(M_LED_SDO);
            bio_buf_output(M_LED_SCL);
            // success = pio_claim_free_sm_and_add_program_for_gpio_range(&apa102_mini_program, &pio_config.pio,
            // &pio_config.sm, &pio_config.offset, bio2bufiopin[M_LED_SDO], 1, true); hard_assert(success);
            pio_config.program = &apa102_mini_program;
            pio_config.offset = pio_add_program(pio_config.pio, pio_config.program);
#ifdef BP_PIO_SHOW_ASSIGNMENT
            printf("PIO: pio=%d, sm=%d, offset=%d\r\n", PIO_NUM(pio_config.pio), pio_config.sm, pio_config.offset);
#endif
            apa102_mini_program_init(pio_config.pio,
                                     pio_config.sm,
                                     pio_config.offset,
                                     mode_config.baudrate,
                                     bio2bufiopin[M_LED_SCL],
                                     bio2bufiopin[M_LED_SDO]);
            system_config.num_bits=32;            
            break;
        case M_LED_WS2812_ONBOARD: // internal LEDs, stop any in-progress stuff
            rgb_irq_enable(false);
            rgb_set_all(0, 0, 0);
            pio_config.pio = PIO_RGB_LED_PIO;
            pio_config.sm = PIO_RGB_LED_SM;
            mode_config.baudrate = 800000;
            system_config.num_bits=24;
            #if BP_VER == 5
                logic_analyzer_set_base_pin(RGB_CDO);
            #endif
            break;
        default:
            printf("\r\nError: Invalid device type");
            return 0;
    }
    device_cleanup = mode_config.device;
    system_config.subprotocol_name = led_device_type[mode_config.device];

    return 1;
}

bool hwled_preflight_sanity_check(void){
    if (mode_config.device == M_LED_WS2812_ONBOARD) {
        return true;
    }
    return ui_help_sanity_check(true, 0x00);
}

void hwled_start(struct _bytecode* result, struct _bytecode* next) {
    switch (mode_config.device) {
        case M_LED_WS2812:
        case M_LED_WS2812_ONBOARD:
            hwled_wait_idle();  //wait until the FIFO is empty and the state machine is idle 
            busy_wait_us(65); // >50us delay to reset
            result->data_message = GET_T(T_HWLED_RESET);
            break;
        case M_LED_APA102:
            pio_sm_put_blocking(pio_config.pio, pio_config.sm, 0x00000000);
            result->data_message = GET_T(T_HWLED_FRAME_START);
            break;
        default:
            printf("Error: Invalid device type");
    }
}

void hwled_stop(struct _bytecode* result, struct _bytecode* next) {
    switch (mode_config.device) {
        case M_LED_WS2812:
        case M_LED_WS2812_ONBOARD:
            hwled_wait_idle();  //wait until the FIFO is empty and the state machine is idle    
            busy_wait_us(65); // >50us delay to reset
            result->data_message = GET_T(T_HWLED_RESET);
            break;
        case M_LED_APA102:
            pio_sm_put_blocking(pio_config.pio, pio_config.sm, 0xFFFFFFFF);
            result->data_message = GET_T(T_HWLED_FRAME_STOP);
            break;
        default:
            printf("Error: Invalid device type");
    }
}

void hwled_write(struct _bytecode* result, struct _bytecode* next) {
    // Protocol-specific:
    // * parameter `next` is unused
    // * parameter `result->out_data` contains a 24-bit RGB value to send
    //   NOTE: for WS2812, the top 8 bits are cleared to zero.
    //   NOTE: for APA102, the top 8 bits are forced to 0xFF (full brightness).
    // UNDOCUMENTED: 
    //   Order in which the bytes are sent is NOT documented here.
    //   Caller must test to determin the proper RGB value order for their hardware.
    switch (mode_config.device) {
        // TODO: add support for RGBW   (RGB + white)?
        // TODO: add support for RGBWW  (RGB + cool white + warm white)?
        // TODO: add support for RGBWWA (RGB + cool white + warm white + amber)?
        case M_LED_WS2812:
            // NOTE: Only supporting 24-bit RGB values for now.
            //       As a hack, callers can likely use RGBW by packing
            //       three pixels' data into four 24-bit values:
            //       0x00R1G1B1 0x00W1R2G2 0x00B2W2R3 0x00G3B3W3
            pio_sm_put_blocking(pio_config.pio, pio_config.sm, (result->out_data << 8u));
            break;
        case M_LED_APA102:
            //       only set to full brightness if the top bits from caller are zero
            //       otherwise, allow the caller to set the global brightness bits for
            //       more advanced brightness setting.
            /*if ((result->out_data & 0xE0000000) == 0) {
                result->out_data|=(0xff<<24);
                //pio_sm_put_blocking(pio_config.pio, pio_config.sm, ((0xff<<24)|result->out_data));
            }//else{*/
                pio_sm_put_blocking(pio_config.pio, pio_config.sm, result->out_data);
            //}
            break;
        case M_LED_WS2812_ONBOARD:
            rgb_put(result->out_data);
            break;
        default:
            printf("Error: Invalid device type");
            // return 0;
    }
}

void hwled_macro(uint32_t macro) {
    switch (macro) {
        case 0:
            printf("%s\r\n", GET_T(T_MODE_ERROR_NO_MACROS_AVAILABLE));
            break;
        default:
            printf("%s\r\n", GET_T(T_MODE_ERROR_MACRO_NOT_DEFINED));
            system_config.error = 1;
    }
}

void hwled_cleanup(void) {
    switch (device_cleanup) {
        case M_LED_WS2812:
        case M_LED_APA102:
            // pio_remove_program_and_unclaim_sm(pio_config.program, pio_config.pio, pio_config.sm, pio_config.offset);
            pio_remove_program(pio_config.pio, pio_config.program, pio_config.offset);
            break;
        case M_LED_WS2812_ONBOARD:
            #if BP_VER == 5
                logic_analyzer_set_base_pin(LA_BPIO0);
            #endif        
            rgb_irq_enable(true);
            break;
    }
    system_config.subprotocol_name = 0x00;
    system_config.num_bits=8;
    system_bio_update_purpose_and_label(false, M_LED_SDO, BP_PIN_MODE, 0);
    system_bio_update_purpose_and_label(false, M_LED_SCL, BP_PIN_MODE, 0);
    bio_init();
}

void hwled_settings(void) {
    ui_prompt_mode_settings_string(GET_T(T_HWLED_DEVICE_MENU), GET_T(leds_type_menu[mode_config.device].description), 0x00);
    //ui_prompt_mode_settings_int(GET_T(T_HWLED_NUM_LEDS_MENU), mode_config.num_leds, 0x00);
}

void hwled_help(void) {
    ui_help_mode_commands(hwled_commands, hwled_commands_count);
}

uint32_t hwled_get_speed(void) {
    return mode_config.baudrate;
}

// NOTE: Function must have no parameters ... this is a protocol entry point.
void hwled_wait_idle(void) {
    if(!pio_sm_wait_idle(pio_config.pio, pio_config.sm, 0xffffff)){
        printf("Timeout, error!");
    }        
}
