/*
 * Copyright (c) 2021 Stefan Althöfer
 * SPDX-License-Identifier: BSD-3-Clause (as indicated in the PIO file)
 * https://github.com/stefanalt/RP2040-PIO-1-Wire-Master
 *
 * 1-Wire is a tradmark of Maxim Integrated
 * 2023 Modified by Ian Lesnet for Bus Pirate 5 buffered IO
 *
 * OWxxxx functions might fall under Maxxim copyriht.
 *
 * Demo code for PIO 1-Wire interface
 */

#include <stdio.h>
#include <string.h>
#include "pico/stdlib.h"
#include "pirate.h"
#include "command_struct.h"
#include "hardware/pio.h"
#include "pirate/hw1wire_pio.h"
#include "ui/ui_help.h"
#include "binmode/fala.h"

static const char* const ds18b20_usage[] = {
    "ds18b20\t[-h(elp)]",
    "measure temperature (single sensor bus only):%s ds18b20",
};

static const struct ui_help_options ds18b20_options[] = {
    { 1, "", T_HELP_1WIRE_DS18B20 }, // command help
    { 0, "-h", T_HELP_FLAG },        // help
};

void onewire_test_ds18b20_conversion(struct command_result* res) {
    if (ui_help_show(
            res->help_flag, ds18b20_usage, count_of(ds18b20_usage), &ds18b20_options[0], count_of(ds18b20_options))) {
        return;
    }

    int i;
    unsigned char buf[9];
    int32_t temp;

    //we manually control any FALA capture
    fala_start_hook();

    if (!onewire_reset()) {
        res->error = true;
        goto ds18b20_cleanup;
    }

    onewire_tx_byte(0xcc);   // Skip ROM command
    onewire_tx_byte(0x4e);   // Write Scatchpad
    onewire_tx_byte(0x00);   // TH
    onewire_tx_byte(0x00);   // TL
    onewire_tx_byte(0x7f);   // CONF (12bit)
    onewire_wait_for_idle(); // added: need to wait or the transmission isn't done before the reset

    if (!onewire_reset()) {
        res->error = true;
        goto ds18b20_cleanup;
    }

    onewire_tx_byte(0xcc); // Skip ROM command
    onewire_tx_byte(0x44); // Convert T
    sleep_ms(800);         /* 12bit: 750 ms */

    printf("RX:");

    if (!onewire_reset()) {
        res->error = true;
        goto ds18b20_cleanup;
    }

    onewire_tx_byte(0xcc); // Skip ROM command
    onewire_tx_byte(0xbe); // Read Scatchpad
    for (i = 0; i < 9; i++) {
        buf[i] = onewire_rx_byte();
        printf(" %.2x", buf[i]);
    }

    //we manually control any FALA capture
    fala_stop_hook();
    fala_notify_hook();

    if (calc_crc8_buf(buf, 8) != buf[8]) {
        printf("\r\nCRC Fail\r\n");
        return; // return 0 to avoid device not found message
    }

    temp = buf[0] | (buf[1] << 8);
    if (temp & 0x8000) {
        temp |= 0xffff0000;
    }
    printf("\r\nTemperature: %.3f\r\n", (float)temp / 16);

    return;

ds18b20_cleanup:
    //we manually control any FALA capture
    fala_stop_hook();
    fala_notify_hook();
    return;
}