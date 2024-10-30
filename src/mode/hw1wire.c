#include <stdio.h>
#include "pico/stdlib.h"
#include <stdint.h>
#include <string.h>
#include "pirate.h"
#include "system_config.h"
#include "opt_args.h"
#include "bytecode.h"
#include "mode/hw1wire.h"
#include "pirate/bio.h"
#include "ui/ui_prompt.h"
#include "pirate/storage.h"
#include "ui/ui_term.h"
#include "hardware/pio.h"
#include "pico/binary_info.h"
#include "hardware/clocks.h"
#include "ui/ui_help.h"
#include "commands/1wire/scan.h"
#include "commands/1wire/demos.h"

#define BP_OLD_HW1WIRE
#ifdef BP_OLD_HW1WIRE
#include "pirate/hw1wire_pio.h"
#else
#include "pirate/onewire_library.h"
#endif

// command configuration
const struct _command_struct hw1wire_commands[] = {
    // Function Help
    // note: for now the allow_hiz flag controls if the mode provides it's own help
    { "scan",
      false,
      &onewire_test_romsearch,
      T_HELP_1WIRE_SCAN }, // the help is shown in the -h *and* the list of mode apps
    { "ds18b20", false, &onewire_test_ds18b20_conversion, T_HELP_1WIRE_DS18B20 },
};
const uint32_t hw1wire_commands_count = count_of(hw1wire_commands);

static const char pin_labels[][5] = { "OWD" };

uint32_t hw1wire_setup(void) {
    uint32_t temp;
    return 1;
}

uint32_t hw1wire_setup_exc(void) {
    system_bio_claim(true, M_OW_OWD, BP_PIN_MODE, pin_labels[0]);
#ifdef BP_OLD_HW1WIRE
    onewire_init(bio2bufiopin[M_OW_OWD], bio2bufdirpin[M_OW_OWD]);
#else
    ow_init(8, bio2bufdirpin[M_OW_OWD], bio2bufiopin[M_OW_OWD]);
#endif
    return 1;
}

void hw1wire_start(struct _bytecode* result, struct _bytecode* next) {
    result->data_message = GET_T(T_HW1WIRE_RESET);

    if (bio_get(M_OW_OWD) == 0) {
        result->error_message = GET_T(T_HWI2C_NO_PULLUP_DETECTED);
        result->error = SRES_WARN;
    }
#ifdef BP_OLD_HW1WIRE
    uint8_t device_detect = onewire_reset();
#else
    uint8_t device_detect = ow_reset();
#endif

    if (device_detect) {
        // result->error_message = GET_T(T_HW1WIRE_PRESENCE_DETECT);
    } else {
        result->error_message = GET_T(T_HW1WIRE_NO_DEVICE);
        result->error = SRES_ERROR;
    }
}

void hw1wire_write(struct _bytecode* result, struct _bytecode* next) {
#ifdef BP_OLD_HW1WIRE
    onewire_tx_byte(result->out_data);
    onewire_wait_for_idle();
#else
    ow_send(result->out_data);
#endif
}

void hw1wire_read(struct _bytecode* result, struct _bytecode* next) {
#ifdef BP_OLD_HW1WIRE
    result->in_data = onewire_rx_byte();
    onewire_wait_for_idle(); //temp test
#else
    result->in_data = ow_read();
#endif
}

void hw1wire_cleanup(void) {
#ifdef BP_OLD_HW1WIRE
    onewire_cleanup();
#else
    ow_cleanup();
#endif
    bio_init();
    system_bio_claim(false, M_OW_OWD, BP_PIN_MODE, 0);
}

// MACROS
void hw1wire_macro(uint32_t macro) {
    switch (macro) {
        case 0:
            printf(" 0. Macro list\r\n");
            break;
        default:
            printf("%s\r\n", GET_T(T_MODE_ERROR_MACRO_NOT_DEFINED));
            system_config.error = 1;
    }
}

void hw1wire_help(void) {
    ui_help_mode_commands(hw1wire_commands, hw1wire_commands_count);
}

uint32_t hw1wire_get_speed(void) {
    return 14286;
}
