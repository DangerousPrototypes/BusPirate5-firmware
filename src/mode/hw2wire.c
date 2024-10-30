#include <stdio.h>
#include <string.h>
#include <math.h>
#include "pico/stdlib.h"
#include <stdint.h>
#include "pirate.h"
#include "system_config.h"
#include "opt_args.h"
#include "bytecode.h"
#include "mode/hw2wire.h"
#include "pirate/bio.h"
#include "ui/ui_prompt.h"
#include "ui/ui_cmdln.h"
#include "hw2wire.pio.h"
#include "pirate/hw2wire_pio.h"
#include "pirate/storage.h"
#include "ui/ui_term.h"
#include "ui/ui_command.h"
#include "ui/ui_format.h"
#include "ui/ui_help.h"
#include "commands/2wire/sle4442.h"

static const char pin_labels[][5] = { "SDA", "SCL", "RST" };
struct _hw2wire_mode_config hw2wire_mode_config;
static uint8_t checkshort(void);

// command configuration
const struct _command_struct hw2wire_commands[] = {
    // note: for now the allow_hiz flag controls if the mode provides it's own help
    { "sle4442", false, &sle4442, T_HELP_SLE4442 }, // the help is shown in the -h *and* the list of mode apps
};
const uint32_t hw2wire_commands_count = count_of(hw2wire_commands);

uint32_t hw2wire_setup(void) {
    uint32_t temp;

    // menu items options
    static const struct prompt_item i2c_data_bits_menu[] = { { T_HWI2C_DATA_BITS_MENU_1 },
                                                             { T_HWI2C_DATA_BITS_MENU_2 } };
    static const struct prompt_item i2c_speed_menu[] = { { T_HWI2C_SPEED_MENU_1 } };

    static const struct ui_prompt i2c_menu[] = { { .description = T_HW2WIRE_SPEED_MENU,
                                                   .menu_items = i2c_speed_menu,
                                                   .menu_items_count = count_of(i2c_speed_menu),
                                                   .prompt_text = T_HWI2C_SPEED_PROMPT,
                                                   .minval = 1,
                                                   .maxval = 1000,
                                                   .defval = 400,
                                                   .menu_action = 0,
                                                   .config = &prompt_int_cfg },
                                                 { .description = T_HWI2C_DATA_BITS_MENU,
                                                   .menu_items = i2c_data_bits_menu,
                                                   .menu_items_count = count_of(i2c_data_bits_menu),
                                                   .prompt_text = T_HWI2C_DATA_BITS_PROMPT,
                                                   .minval = 0,
                                                   .maxval = 0,
                                                   .defval = 1,
                                                   .menu_action = 0,
                                                   .config = &prompt_list_cfg } };

    const char config_file[] = "bp2wire.bp";

    const mode_config_t config_t[] = {
        // clang-format off
        { "$.baudrate", &hw2wire_mode_config.baudrate, MODE_CONFIG_FORMAT_DECIMAL },
        { "$.data_bits", &hw2wire_mode_config.data_bits, MODE_CONFIG_FORMAT_DECIMAL },
        // clang-format on
    };
    prompt_result result;

    if (storage_load_mode(config_file, config_t, count_of(config_t))) {
        printf("\r\n%s%s%s\r\n", ui_term_color_info(), GET_T(T_USE_PREVIOUS_SETTINGS), ui_term_color_reset());
        // printf(" %s: %s\r\n", GET_T(T_HWI2C_DATA_BITS_MENU),
        // GET_T(i2c_data_bits_menu[hw2wire_mode_config.data_bits].description));
        hw2wire_settings();
        bool user_value;
        if (!ui_prompt_bool(&result, true, true, true, &user_value)) {
            return 0;
        }
        if (user_value) {
            return 1; // user said yes, use the saved settings
        }
    }
    ui_prompt_uint32(&result, &i2c_menu[0], &hw2wire_mode_config.baudrate);
    if (result.exit) {
        return 0;
    }
    // printf("Result: %d\r\n", hw2wire_mode_config.baudrate);
    // ui_prompt_uint32(&result, &i2c_menu[1], &temp);
    // if(result.exit) return 0;
    // hw2wire_mode_config.data_bits=(uint8_t)temp-1;

    storage_save_mode(config_file, config_t, count_of(config_t));

    return 1;
}

uint32_t hw2wire_setup_exc(void) {
    pio_hw2wire_init(bio2bufiopin[M_2WIRE_SDA],
                     bio2bufiopin[M_2WIRE_SCL],
                     bio2bufdirpin[M_2WIRE_SDA],
                     bio2bufdirpin[M_2WIRE_SCL],
                     hw2wire_mode_config.baudrate);
    system_bio_claim(true, M_2WIRE_SDA, BP_PIN_MODE, pin_labels[0]);
    system_bio_claim(true, M_2WIRE_SCL, BP_PIN_MODE, pin_labels[1]);
    system_bio_claim(true, M_2WIRE_RST, BP_PIN_MODE, pin_labels[2]);
    pio_hw2wire_reset();
    bio_put(M_2WIRE_RST, 0); // preload the RST pin to be 0 when output
}

void hw2wire_start(struct _bytecode* result, struct _bytecode* next) {
    result->data_message = GET_T(T_HWI2C_START);
    if (checkshort()) {
        result->error_message = GET_T(T_HWI2C_NO_PULLUP_DETECTED);
        result->error = SRES_WARN;
    }
    pio_hw2wire_start();
}

void hw2wire_start_alt(struct _bytecode* result, struct _bytecode* next) {
    result->data_message = GET_T(T_HW2WIRE_RST_HIGH);
    bio_input(M_2WIRE_RST);
}

void hw2wire_stop(struct _bytecode* result, struct _bytecode* next) {
    result->data_message = GET_T(T_HWI2C_STOP);
    pio_hw2wire_stop();
}

void hw2wire_stop_alt(struct _bytecode* result, struct _bytecode* next) {
    result->data_message = GET_T(T_HW2WIRE_RST_LOW);
    bio_output(M_2WIRE_RST);
}

void hw2wire_write(struct _bytecode* result, struct _bytecode* next) {
    uint32_t temp = result->out_data;
    ui_format_bitorder_manual(&temp, result->bits, system_config.bit_order);
    pio_hw2wire_put16(temp);
}

void hw2wire_read(struct _bytecode* result, struct _bytecode* next) {
    uint8_t temp8;
    pio_hw2wire_get16(&temp8);
    uint32_t temp = temp8;
    ui_format_bitorder_manual(&temp, result->bits, system_config.bit_order);
    result->in_data = temp;
}

void hw2wire_tick_clock(struct _bytecode* result, struct _bytecode* next) {
    pio_hw2wire_clock_tick();
}

void hw2wire_set_clk_high(struct _bytecode* result, struct _bytecode* next) {
    pio_hw2wire_set_mask(1 << M_2WIRE_SCL, 1 << M_2WIRE_SCL);
}

void hw2wire_set_clk_low(struct _bytecode* result, struct _bytecode* next) {
    pio_hw2wire_set_mask(1 << M_2WIRE_SCL, 0);
}

void hw2wire_set_dat_high(struct _bytecode* result, struct _bytecode* next) {
    pio_hw2wire_set_mask(1 << M_2WIRE_SDA, 1 << M_2WIRE_SDA);
}

void hw2wire_set_dat_low(struct _bytecode* result, struct _bytecode* next) {
    pio_hw2wire_set_mask(1 << M_2WIRE_SDA, 0);
}

void hw2wire_read_bit(struct _bytecode* result, struct _bytecode* next) {
    result->in_data = bio_get(M_2WIRE_SDA);
}

void hw2wire_macro(uint32_t macro) {
    uint32_t result = 0;
    switch (macro) {
        case 0:
            printf(" 0. Macro menu\r\n");
            break;
        default:
            printf("%s\r\n", GET_T(T_MODE_ERROR_MACRO_NOT_DEFINED));
            system_config.error = 1;
    }

    if (result) {
        printf("Device not found\r\n");
    }
}

void hw2wire_cleanup(void) {
    pio_hw2wire_cleanup();
    bio_init();
    system_bio_claim(false, M_2WIRE_SDA, BP_PIN_MODE, 0);
    system_bio_claim(false, M_2WIRE_SCL, BP_PIN_MODE, 0);
    system_bio_claim(false, M_2WIRE_RST, BP_PIN_MODE, 0);
}

/*void hw2wire_pins(void){
    printf("-\t-\tSCL\tSDA");
}*/

void hw2wire_settings(void) {
    ui_prompt_mode_settings_int(GET_T(T_HW2WIRE_SPEED_MENU), hw2wire_mode_config.baudrate, GET_T(T_KHZ));
}

void hw2wire_printI2Cflags(void) {
    uint32_t temp;
}

void hw2wire_help(void) {
    printf("%s2 wire open drain bus with CLOCK and bidirectional DATA\r\n", ui_term_color_info());
    printf("Open drain bus, requires pull-up resistors (use 'P' to enable pull-ups)\r\n");
    printf("[ & ] create an I2C START & STOP sequence\r\n");
    printf("{ & } Toggle the RST pin HIGH and LOW.\r\n");
    ui_help_mode_commands(hw2wire_commands, hw2wire_commands_count);
/*printf("\r\nAvailable mode commands:\r\n");
for(uint32_t i=0;i<hw2wire_commands_count;i++)
{
    printf("%s%s%s\t%s%s\r\n", ui_term_color_prompt(), hw2wire_commands[i].command,
        ui_term_color_info(), hw2wire_commands[i].help_text?t[hw2wire_commands[i].help_text]:"Unavailable",
ui_term_color_reset());
}*/
#if 0
	printf("\r\n");
	printf("More info: https://en.wikipedia.org/wiki/I2C\r\n");
	printf("\r\n");
	printf("Electrical:\r\n");
	printf("\r\n");
	printf("BPCMD\t   { |            ADDRES(7bits+R/!W bit)             |\r\n");
	printf("CMD\tSTART| A6  | A5  | A4  | A3  | A2  | A1  | A0  | R/!W| ACK* \r\n");
	printf("\t-----|-----|-----|-----|-----|-----|-----|-----|-----|-----\r\n");
	printf("SDA\t\"\"___|_###_|_###_|_###_|_###_|_###_|_###_|_###_|_###_|_###_ ..\r\n");
	printf("SCL\t\"\"\"\"\"|__\"__|__\"__|__\"__|__\"__|__\"__|__\"__|__\"__|__\"__|__\"__ ..\r\n");
	printf("\r\n");
	printf("BPCMD\t   |                      DATA (8bit)              |     |  ]  |\r\n");
	printf("CMD\t.. | D7  | D6  | D5  | D4  | D3  | D2  | D1  | D0  | ACK*| STOP|  \r\n");
	printf("\t  -|-----|-----|-----|-----|-----|-----|-----|-----|-----|-----|\r\n");
	printf("SDA\t.. |_###_|_###_|_###_|_###_|_###_|_###_|_###_|_###_|_###_|___\"\"|\r\n");
	printf("SCL\t.. |__\"__|__\"__|__\"__|__\"__|__\"__|__\"__|__\"__|__\"__|__\"__|\"\"\"\"\"|\r\n");
	printf("\r\n");
	printf("* Receiver needs to pull SDA down when address/byte is received correctly\r\n");
	printf("\r\n");
	printf("Connection:\r\n");
	printf("\t\t  +--[2k]---+--- +3V3 or +5V0\r\n");
	printf("\t\t  | +-[2k]--|\r\n");
	printf("\t\t  | |\r\n");
	printf("\tSDA \t--+-|------------- SDA\r\n");
	printf("{BP}\tSCL\t----+------------- SCL  {DUT}\r\n");
	printf("\tGND\t------------------ GND\r\n");
#endif
}

static uint8_t checkshort(void) {
    uint8_t temp;
    temp = (bio_get(M_2WIRE_SDA) == 0 ? 1 : 0);
    temp |= (bio_get(M_2WIRE_SCL) == 0 ? 2 : 0);
    return (temp == 3); // there is only a short when both are 0 otherwise repeated start wont work
}

uint32_t hw2wire_get_speed(void) {
    return hw2wire_mode_config.baudrate * 1000;
}
