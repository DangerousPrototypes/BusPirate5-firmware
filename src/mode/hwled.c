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
#include "hardware/pio.h"
#include "ws2812.pio.h"
#include "apa102.pio.h"
#include "pirate/rgb.h"
#include "pirate/storage.h"
#include "ui/ui_term.h"
#include "ui/ui_help.h"
#include "pio_config.h"
#include "binmode/logicanalyzer.h"
#include "lib/bp_args/bp_cmd.h"

struct _pio_config pio_config;

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

led_mode_config_t hwled_mode_config;

static uint8_t device_cleanup;

// Forward declarations for device-specific functions
static uint32_t ws2812_setup_exc(void);
static void ws2812_cleanup(void);
static void ws2812_start(void);
static void ws2812_stop(void);
static void ws2812_write(uint32_t pixel_data);
static uint32_t apa102_setup_exc(void);
static void apa102_cleanup(void);
static void apa102_start(void);
static void apa102_stop(void);
static void apa102_write(uint32_t pixel_data);
static uint32_t onboard_setup_exc(void);
static void onboard_cleanup(void);
static void onboard_start(void);
static void onboard_stop(void);
static void onboard_write(uint32_t pixel_data);

// Device function table - indexed by M_LED_DEVICE_TYPE enum
const led_device_funcs led_devices[] = {
    [M_LED_WS2812] = {
        .setup_exc = ws2812_setup_exc,
        .cleanup = ws2812_cleanup,
        .start = ws2812_start,
        .stop = ws2812_stop,
        .write = ws2812_write,
        .data_message_start = T_HWLED_RESET,
        .data_message_stop = T_HWLED_RESET
    },
    [M_LED_APA102] = {
        .setup_exc = apa102_setup_exc,
        .cleanup = apa102_cleanup,
        .start = apa102_start,
        .stop = apa102_stop,
        .write = apa102_write,
        .data_message_start = T_HWLED_FRAME_START,
        .data_message_stop = T_HWLED_FRAME_STOP
    },
    [M_LED_WS2812_ONBOARD] = {
        .setup_exc = onboard_setup_exc,
        .cleanup = onboard_cleanup,
        .start = onboard_start,
        .stop = onboard_stop,
        .write = onboard_write,
        .data_message_start = T_HWLED_RESET,
        .data_message_stop = T_HWLED_RESET
    }
};

// WS2812 device functions
static uint32_t ws2812_setup_exc(void) {
    pio_config.pio = PIO_MODE_PIO;
    pio_config.sm = 0;
    system_bio_update_purpose_and_label(true, M_LED_SDO, BP_PIN_MODE, pin_labels[0]);
    hwled_mode_config.baudrate = 800000;
    bio_buf_output(M_LED_SDO);
    pio_config.program = &ws2812_program;
    pio_config.offset = pio_add_program(pio_config.pio, pio_config.program);
#ifdef BP_PIO_SHOW_ASSIGNMENT
    printf("PIO: pio=%d, sm=%d, offset=%d\r\n", PIO_NUM(pio_config.pio), pio_config.sm, pio_config.offset);
#endif
    ws2812_program_init(pio_config.pio,
                        pio_config.sm,
                        pio_config.offset,
                        bio2bufiopin[M_LED_SDO],
                        (float)hwled_mode_config.baudrate,
                        false);
    system_config.num_bits = 24;
    return 1;
}

static void ws2812_cleanup(void) {
    pio_remove_program(pio_config.pio, pio_config.program, pio_config.offset);
}

static void ws2812_start(void) {
    hwled_wait_idle();
    busy_wait_us(65); // >50us delay to reset
}

static void ws2812_stop(void) {
    hwled_wait_idle();
    busy_wait_us(65); // >50us delay to reset
}

static void ws2812_write(uint32_t pixel_data) {
    pio_sm_put_blocking(pio_config.pio, pio_config.sm, (pixel_data << 8u));
}

// APA102 device functions
static uint32_t apa102_setup_exc(void) {
    pio_config.pio = PIO_MODE_PIO;
    pio_config.sm = 0;
    system_bio_update_purpose_and_label(true, M_LED_SDO, BP_PIN_MODE, pin_labels[0]);
    system_bio_update_purpose_and_label(true, M_LED_SCL, BP_PIN_MODE, pin_labels[1]);
    hwled_mode_config.baudrate = (5 * 1000 * 1000);
    bio_buf_output(M_LED_SDO);
    bio_buf_output(M_LED_SCL);
    pio_config.program = &apa102_mini_program;
    pio_config.offset = pio_add_program(pio_config.pio, pio_config.program);
#ifdef BP_PIO_SHOW_ASSIGNMENT
    printf("PIO: pio=%d, sm=%d, offset=%d\r\n", PIO_NUM(pio_config.pio), pio_config.sm, pio_config.offset);
#endif
    apa102_mini_program_init(pio_config.pio,
                             pio_config.sm,
                             pio_config.offset,
                             hwled_mode_config.baudrate,
                             bio2bufiopin[M_LED_SCL],
                             bio2bufiopin[M_LED_SDO]);
    system_config.num_bits = 32;
    return 1;
}

static void apa102_cleanup(void) {
    pio_remove_program(pio_config.pio, pio_config.program, pio_config.offset);
}

static void apa102_start(void) {
    pio_sm_put_blocking(pio_config.pio, pio_config.sm, 0x00000000);
}

static void apa102_stop(void) {
    pio_sm_put_blocking(pio_config.pio, pio_config.sm, 0xFFFFFFFF);
}

static void apa102_write(uint32_t pixel_data) {
    pio_sm_put_blocking(pio_config.pio, pio_config.sm, pixel_data);
}

// WS2812 Onboard device functions
static uint32_t onboard_setup_exc(void) {
    rgb_irq_enable(false);
    rgb_set_all(0, 0, 0);
    pio_config.pio = PIO_RGB_LED_PIO;
    pio_config.sm = PIO_RGB_LED_SM;
    hwled_mode_config.baudrate = 800000;
    system_config.num_bits = 24;
#if BP_VER == 5
    logic_analyzer_set_base_pin(RGB_CDO);
#endif
    return 1;
}

static void onboard_cleanup(void) {
#if BP_VER == 5
    logic_analyzer_set_base_pin(LA_BPIO0);
#endif
    rgb_irq_enable(true);
}

static void onboard_start(void) {
    hwled_wait_idle();
    busy_wait_us(65); // >50us delay to reset
}

static void onboard_stop(void) {
    hwled_wait_idle();
    busy_wait_us(65); // >50us delay to reset
}

static void onboard_write(uint32_t pixel_data) {
    rgb_put(pixel_data);
}


// Device type — flag -d / --device
static const bp_val_choice_t led_device_choices[] = {
    { "ws2812",   NULL, T_HWLED_DEVICE_MENU_1, 0 },
    { "apa102",   NULL, T_HWLED_DEVICE_MENU_2, 1 },
    { "onboard",  NULL, T_HWLED_DEVICE_MENU_3, 2 },
};
static const bp_val_constraint_t led_device_choice = {
    .type = BP_VAL_CHOICE,
    .choice = { .choices = led_device_choices, .count = 3, .def = 0 },
    .prompt = T_HWLED_DEVICE_MENU,
};

static const bp_command_opt_t led_setup_opts[] = {
    { "device", 'd', BP_ARG_REQUIRED, "ws2812/apa102/onboard", 0, &led_device_choice },
    { 0 },
};

const bp_command_def_t led_setup_def = {
    .name = "led",
    .description = 0,
    .opts = led_setup_opts,
};

uint32_t hwled_setup(void) {

    const char config_file[] = "bpled.bp";
    const mode_config_t config_t[] = {
        // clang-format off
        { "$.device", &hwled_mode_config.device, MODE_CONFIG_FORMAT_DECIMAL },
        //{ "$.num_leds", &hwled_mode_config.num_leds, MODE_CONFIG_FORMAT_DECIMAL },
        // clang-format on
    };

    // Detect interactive vs CLI mode by checking the primary flag
    uint32_t temp;
    bp_cmd_status_t st = bp_cmd_flag(&led_setup_def, 'd', &temp);
    if (st == BP_CMD_INVALID) return 0;
    bool interactive = (st == BP_CMD_MISSING);

    if (interactive) {
        if (storage_load_mode(config_file, config_t, count_of(config_t))) {
            printf("\r\n\r\n%s%s%s\r\n", ui_term_color_info(), GET_T(T_USE_PREVIOUS_SETTINGS), ui_term_color_reset());
            hwled_settings();
            int r = bp_cmd_yes_no_exit("");
            if (r == BP_YN_EXIT) return 0; // exit
            if (r == BP_YN_YES)  return 1; // use saved settings
        }

        if (bp_cmd_prompt(&led_device_choice, &temp) != BP_CMD_OK) return 0;
        hwled_mode_config.device = temp;
    } else {
        hwled_mode_config.device = temp;
    }

    storage_save_mode(config_file, config_t, count_of(config_t));

    return 1;
}

uint32_t hwled_setup_exc(void) {
    uint32_t result = led_devices[hwled_mode_config.device].setup_exc();
    if (result) {
        device_cleanup = hwled_mode_config.device;
        system_config.subprotocol_name = led_device_type[hwled_mode_config.device];
    }
    return result;
}

bool hwled_preflight_sanity_check(void){
    if (hwled_mode_config.device == M_LED_WS2812_ONBOARD) {
        return true;
    }
    return ui_help_sanity_check(true, 0x00);
}

void hwled_start(struct _bytecode* result, struct _bytecode* next) {
    led_devices[hwled_mode_config.device].start();
    result->data_message = GET_T(led_devices[hwled_mode_config.device].data_message_start);
}

void hwled_stop(struct _bytecode* result, struct _bytecode* next) {
    led_devices[hwled_mode_config.device].stop();
    result->data_message = GET_T(led_devices[hwled_mode_config.device].data_message_stop);
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
    // TODO: add support for RGBW   (RGB + white)?
    // TODO: add support for RGBWW  (RGB + cool white + warm white)?
    // TODO: add support for RGBWWA (RGB + cool white + warm white + amber)?
    // NOTE: Only supporting 24-bit RGB values for now.
    //       As a hack, callers can likely use RGBW by packing
    //       three pixels' data into four 24-bit values:
    //       0x00R1G1B1 0x00W1R2G2 0x00B2W2R3 0x00G3B3W3
    led_devices[hwled_mode_config.device].write(result->out_data);
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
    led_devices[device_cleanup].cleanup();
    system_config.subprotocol_name = 0x00;
    system_config.num_bits = 8;
    system_bio_update_purpose_and_label(false, M_LED_SDO, BP_PIN_MODE, 0);
    system_bio_update_purpose_and_label(false, M_LED_SCL, BP_PIN_MODE, 0);
    bio_init();
}

void hwled_settings(void) {
    ui_help_setting_string(GET_T(T_HWLED_DEVICE_MENU), GET_T(led_device_choices[hwled_mode_config.device].label), 0x00);
    //ui_help_setting_int(GET_T(T_HWLED_NUM_LEDS_MENU), hwled_mode_config.num_leds, 0x00);
}

void hwled_help(void) {
    ui_help_mode_commands(hwled_commands, hwled_commands_count);
}

uint32_t hwled_get_speed(void) {
    return hwled_mode_config.baudrate;
}

// NOTE: Function must have no parameters ... this is a protocol entry point.
void hwled_wait_idle(void) {
    if(!pio_sm_wait_idle(pio_config.pio, pio_config.sm, 0xffffff)){
        printf("Timeout, error!");
    }        
}

//-----------------------------------------
//


bool hwled_bpio_configure(bpio_mode_configuration_t *bpio_mode_config) {
    // Map submode to device type  
    hwled_mode_config.device = bpio_mode_config->submode;
    
    return true;
}
