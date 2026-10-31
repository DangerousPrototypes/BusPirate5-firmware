#include <stdio.h>
#include "pico/stdlib.h"
#include <stdint.h>
#include "hardware/uart.h"
#include "pirate.h"
#include "system_config.h"
#include "command_struct.h"
#include "ui/ui_term.h"
#include "ui/ui_help.h"
#include "bytecode.h"
#include "mode/hwuart.h"
#include "pirate/button.h"
#include "usb_rx.h"
#include "usb_tx.h"
#include "hwuart.pio.h"
#include "pirate/bio.h"
#include "pio_config.h"
#include "lib/bp_args/bp_cmd.h"

//static struct _pio_config pio_config;

static const char* const usage[] = {
    "sim\t[-h(elp)]",
};

const bp_command_def_t simcard_def = {
    .name = "sim",
    .description = T_HELP_UART_BRIDGE,
    .usage = usage,
    .usage_count = count_of(usage),
};

void simcard_handler(struct command_result* res) {
    if (bp_cmd_help_check(&simcard_def, res->help_flag)) {
        return;
    }
    if (!ui_help_check_vout_vref()) {
        return;
    }

    // uart_rx_program_init(M_I2C_PIO, M_I2C_PIO_SM, pio_loaded_offset, bio2bufiopin[BIO0], 9600);
    // bool success = pio_claim_free_sm_and_add_program_for_gpio_range(&uart_rx_program, &pio_config.pio,
    // &pio_config.sm, &pio_config.offset, bio2bufiopin[BIO0], 1, true); hard_assert(success); printf("PIO: pio=%d,
    // sm=%d, offset=%d\r\n", PIO_NUM(pio_config.pio), pio_config.sm, pio_config.offset);
    // pio_remove_program_and_unclaim_sm(&uart_rx_program, pio_config.pio, pio_config.sm, pio_config.offset);
}