#include <stdbool.h>
#include <stdio.h>
#include <string.h>
#include "pico/stdlib.h"
#include "hardware/spi.h"
#include "pirate.h"
#include "command_struct.h"
#include "fatfs/ff.h"
#include "pirate/storage.h"
#include "bytecode.h"
#include "mode/hwspi.h"
#include "lib/sfud/inc/sfud.h"
#include "lib/sfud/inc/sfud_def.h"
#include "spiflash.h"
#include "lib/bp_args/bp_cmd.h"
// #include "ui/ui_prompt.h"
// #include "ui/ui_const.h"
#include "ui/ui_help.h"
#include "system_config.h"
#include "pirate/amux.h"
#include "hardware/pio.h"
#include "spisnif.pio.h"
#include "usb_rx.h"
#include "pio_config.h"

static struct _pio_config pio_config;
static struct _pio_config pio_config_d1;

static const char* const usage[] = {
    "This command has no help text. (yet)"
    /*    "flash [init|probe|erase|write|read|verify|test]\r\n\t[-f <file>] [-e(rase)] [-v(verify)] [-h(elp)]",
        "Initialize and probe: flash probe",
        "Erase and program, with verify: flash write -f example.bin -e -v",
        "Read to file: flash read -f example.bin",
        "Verify with file: flash verify -f example.bin",
        "Test chip (full erase/write/verify): flash test",
        "Force dump: flash read -o -b <bytes> -f <file>"*/
};

const bp_command_def_t sniff_def = {
    .name         = "sniff",
    .description  = T_HELP_SPI_SNIFF,
    .actions      = NULL,
    .action_count = 0,
    .opts         = NULL,
    .usage        = usage,
    .usage_count  = count_of(usage),
};

bool pio_read(uint32_t* raw) {
    if (pio_sm_is_rx_fifo_empty(pio_config.pio, pio_config.sm)) {
        return false;
    }
    // 8-bit read from the uppermost byte of the FIFO, as data is left-justified
    (*raw) = pio_config.pio->rxf[pio_config.sm];
    // TODO: change this based on UART settings
    // Detect parity error?
    return true;
}

bool pio_read_d1(uint32_t* raw) {
    if (pio_sm_is_rx_fifo_empty(pio_config_d1.pio, pio_config_d1.sm)) {
        return false;
    }
    // 8-bit read from the uppermost byte of the FIFO, as data is left-justified
    (*raw) = pio_config_d1.pio->rxf[pio_config_d1.sm];
    // TODO: change this based on UART settings
    // Detect parity error?
    return true;
}

void sniff_handler(struct command_result* res) {
    uint32_t value;
    if (bp_cmd_help_check(&sniff_def, res->help_flag)) {
        return;
    }

    bool success = pio_claim_free_sm_and_add_program_for_gpio_range(
        &spisnif_program, &pio_config.pio, &pio_config.sm, &pio_config.offset, bio2bufiopin[BIO0], 3, true);
    hard_assert(success);
    printf("PIO: pio=%d, sm=%d, offset=%d\r\n", PIO_NUM(pio_config.pio), pio_config.sm, pio_config.offset);
    spisnif_program_init(
        pio_config.pio, pio_config.sm, pio_config.offset, bio2bufiopin[BIO3], bio2bufiopin[BIO0], bio2bufiopin[BIO2]);

    success = pio_claim_free_sm_and_add_program_for_gpio_range(
        &spisnif_2_program, &pio_config_d1.pio, &pio_config_d1.sm, &pio_config_d1.offset, bio2bufiopin[BIO0], 3u, true);
    hard_assert(success);
    printf("PIO: pio=%d, sm=%d, offset=%d\r\n", PIO_NUM(pio_config_d1.pio), pio_config_d1.sm, pio_config_d1.offset);
    spisnif_2_program_init(pio_config_d1.pio,
                           pio_config_d1.sm,
                           pio_config_d1.offset,
                           bio2bufiopin[BIO3],
                           bio2bufiopin[BIO1],
                           bio2bufiopin[BIO2]);

    static const char pin_labels[][5] = { "DAT0", "DAT1", "SCLK", "SCS" };

    system_bio_update_purpose_and_label(true, BIO0, BP_PIN_MODE, pin_labels[0]);
    system_bio_update_purpose_and_label(true, BIO1, BP_PIN_MODE, pin_labels[1]);
    system_bio_update_purpose_and_label(true, BIO2, BP_PIN_MODE, pin_labels[2]);
    system_bio_update_purpose_and_label(true, BIO3, BP_PIN_MODE, pin_labels[3]);
    printf("Any key to exit\r\n");
    while (true) {
        if (pio_read(&value)) {
            printf("%d ", value);
        }
        if (pio_read_d1(&value)) {
            printf("(%d) ", value);
        }
        char c;
        if (rx_fifo_try_get(&c)) {
            break;
        }
    }
    pio_remove_program_and_unclaim_sm(&spisnif_program, pio_config.pio, pio_config.sm, pio_config.offset);

    pio_remove_program_and_unclaim_sm(&spisnif_2_program, pio_config_d1.pio, pio_config_d1.sm, pio_config_d1.offset);

    system_bio_update_purpose_and_label(false, BIO0, BP_PIN_MODE, 0);
    system_bio_update_purpose_and_label(false, BIO1, BP_PIN_MODE, 0);
    system_bio_update_purpose_and_label(false, BIO2, BP_PIN_MODE, 0);
    system_bio_update_purpose_and_label(false, BIO3, BP_PIN_MODE, 0);
}
