/**
 * @file transparent_uart.c
 * @brief Host-configured USB CDC to hardware UART binary mode.
 *
 * Copyright (c) 2026 DereIBims. MIT License.
 */

#include <stdio.h>
#include "pico/stdlib.h"
#include "hardware/uart.h"
#include "tusb.h"
#include "pirate.h"
#include "pirate/bio.h"
#include "system_config.h"
#include "bytecode.h"
#include "command_struct.h"
#include "modes.h"
#include "ui/ui_help.h"
#include "ui/ui_term.h"
#include "mode/hwuart.h"
#include "binmode/transparent_uart.h"

#define TRANSPARENT_UART_CDC_ITF 1
#define TRANSPARENT_UART_DTR_PIN M_UART_CTS

typedef struct {
    uint32_t bit_rate;
    uint8_t stop_bits;
    uint8_t parity;
    uint8_t data_bits;
} transparent_uart_host_config_t;

const char transparent_uart_name[] = "Transparent UART";

static transparent_uart_host_config_t host_config;
static uint32_t line_coding_sequence;
static uint32_t applied_line_coding_sequence;
static bool mode_active;
static bool uart_active;
static uint8_t cdc_rx_buffer[64];
static uint32_t cdc_rx_count;
static uint32_t cdc_rx_index;

void transparent_uart_line_coding_changed(
    uint8_t itf, uint32_t bit_rate, uint8_t stop_bits, uint8_t parity, uint8_t data_bits) {
    if (itf != TRANSPARENT_UART_CDC_ITF) {
        return;
    }

    __atomic_add_fetch(&line_coding_sequence, 1, __ATOMIC_ACQ_REL);
    __atomic_store_n(&host_config.bit_rate, bit_rate, __ATOMIC_RELAXED);
    __atomic_store_n(&host_config.stop_bits, stop_bits, __ATOMIC_RELAXED);
    __atomic_store_n(&host_config.parity, parity, __ATOMIC_RELAXED);
    __atomic_store_n(&host_config.data_bits, data_bits, __ATOMIC_RELAXED);
    __atomic_add_fetch(&line_coding_sequence, 1, __ATOMIC_RELEASE);
}

void transparent_uart_line_state_changed(uint8_t itf, bool dtr, bool rts) {
    if (itf != TRANSPARENT_UART_CDC_ITF) {
        return;
    }

    /* GPIO writes are safe from either RP2040 core. */
    if (__atomic_load_n(&mode_active, __ATOMIC_ACQUIRE)) {
        bio_put(TRANSPARENT_UART_DTR_PIN, !dtr);
        bio_put(M_UART_RTS, !rts);
    }
}

static uint32_t snapshot_line_coding(transparent_uart_host_config_t* snapshot) {
    uint32_t before;
    uint32_t after;
    do {
        before = __atomic_load_n(&line_coding_sequence, __ATOMIC_ACQUIRE);
        if (before & 1u) {
            continue;
        }
        snapshot->bit_rate = __atomic_load_n(&host_config.bit_rate, __ATOMIC_RELAXED);
        snapshot->stop_bits = __atomic_load_n(&host_config.stop_bits, __ATOMIC_RELAXED);
        snapshot->parity = __atomic_load_n(&host_config.parity, __ATOMIC_RELAXED);
        snapshot->data_bits = __atomic_load_n(&host_config.data_bits, __ATOMIC_RELAXED);
        after = __atomic_load_n(&line_coding_sequence, __ATOMIC_ACQUIRE);
    } while (before != after || (after & 1u));
    return after;
}

static bool line_coding_to_uart(const transparent_uart_host_config_t* config,
                                uint* stop_bits,
                                uart_parity_t* parity) {
    if (config->bit_rate < 1 || config->bit_rate > 7372800 ||
        config->data_bits < 5 || config->data_bits > 8) {
        return false;
    }

    switch (config->stop_bits) {
        case CDC_LINE_CODING_STOP_BITS_1:
            *stop_bits = 1;
            break;
        case CDC_LINE_CODING_STOP_BITS_2:
            *stop_bits = 2;
            break;
        default:
            return false;
    }

    switch (config->parity) {
        case CDC_LINE_CODING_PARITY_NONE:
            *parity = UART_PARITY_NONE;
            break;
        case CDC_LINE_CODING_PARITY_ODD:
            *parity = UART_PARITY_ODD;
            break;
        case CDC_LINE_CODING_PARITY_EVEN:
            *parity = UART_PARITY_EVEN;
            break;
        default:
            return false;
    }
    return true;
}

static bool apply_line_coding(const transparent_uart_host_config_t* config) {
    uint stop_bits;
    uart_parity_t parity;
    if (!line_coding_to_uart(config, &stop_bits, &parity)) {
        if (uart_active) {
            uart_deinit(M_UART_PORT);
            uart_active = false;
        }
        return false;
    }

    if (uart_active) {
        uart_tx_wait_blocking(M_UART_PORT);
    }
    uart_init(M_UART_PORT, config->bit_rate);
    uart_set_format(M_UART_PORT, config->data_bits, stop_bits, parity);
    uart_active = true;
    return true;
}

static void setup_uart_pins(void) {
    static const char pin_labels[][5] = { "TX->", "RX<-", "DTR", "RTS" };

    bio_buf_output(M_UART_TX);
    bio_buf_input(M_UART_RX);
    bio_set_function(M_UART_TX, GPIO_FUNC_UART);
    bio_set_function(M_UART_RX, GPIO_FUNC_UART);

    bio_set_function(TRANSPARENT_UART_DTR_PIN, GPIO_FUNC_SIO);
    bio_put(TRANSPARENT_UART_DTR_PIN, true);
    bio_output(TRANSPARENT_UART_DTR_PIN);
    bio_set_function(M_UART_RTS, GPIO_FUNC_SIO);
    bio_put(M_UART_RTS, true);
    bio_output(M_UART_RTS);

    /* Transparent UART does not use hardware CTS flow control. */
    uart_set_hw_flow(M_UART_PORT, false, false);

    system_bio_update_purpose_and_label(true, M_UART_TX, BP_PIN_MODE, pin_labels[0]);
    system_bio_update_purpose_and_label(true, M_UART_RX, BP_PIN_MODE, pin_labels[1]);
    system_bio_update_purpose_and_label(true, TRANSPARENT_UART_DTR_PIN, BP_PIN_MODE, pin_labels[2]);
    system_bio_update_purpose_and_label(true, M_UART_RTS, BP_PIN_MODE, pin_labels[3]);
}

void transparent_uart_setup(void) {
    __atomic_store_n(&mode_active, false, __ATOMIC_RELEASE);
    uart_active = false;
    cdc_rx_count = 0;
    cdc_rx_index = 0;

    /* Same voltage measurement and standard messages as the UART bridge. */
    if (!ui_help_check_vout_vref()) {
        ui_help_error(T_MODE_NO_VOUT_VREF_ERROR);
        printf("%s%s%s\r\n", ui_term_color_info(), GET_T(T_MODE_NO_VOUT_VREF_HINT), ui_term_color_reset());
        return;
    }

    cdc_line_coding_t coding;
    tud_cdc_n_get_line_coding(TRANSPARENT_UART_CDC_ITF, &coding);
    transparent_uart_line_coding_changed(TRANSPARENT_UART_CDC_ITF,
        coding.bit_rate, coding.stop_bits, coding.parity, coding.data_bits);

    transparent_uart_host_config_t config = { 0 };
    applied_line_coding_sequence = snapshot_line_coding(&config);
    uint stop_bits;
    uart_parity_t parity;
    if (!line_coding_to_uart(&config, &stop_bits, &parity)) {
        ui_help_error(T_MODE_INVALID_OPTION);
        return;
    }

    /* Relinquish pins owned by the current CLI bus mode, then use DIO as the
     * base mode so DTR and RTS remain available as driven BIO pins. */
    modes[system_config.mode].protocol_cleanup();
    system_config.mode = DIO;
    modes[system_config.mode].protocol_setup_exc();
    printf("%s%s:%s DIO\r\n", ui_term_color_info(), GET_T(T_MODE_MODE), ui_term_color_reset());

    setup_uart_pins();
    apply_line_coding(&config);

    system_config.binmode_usb_rx_queue_enable = false;
    system_config.binmode_usb_tx_queue_enable = false;
    __atomic_store_n(&mode_active, true, __ATOMIC_RELEASE);

    /* Apply the current host state once; later changes arrive via callback. */
    uint8_t line_state = tud_cdc_n_get_line_state(TRANSPARENT_UART_CDC_ITF);
    transparent_uart_line_state_changed(
        TRANSPARENT_UART_CDC_ITF, (line_state & 0x01u) != 0, (line_state & 0x02u) != 0);
}

void transparent_uart_service(void) {
    if (!__atomic_load_n(&mode_active, __ATOMIC_ACQUIRE)) {
        return;
    }

    transparent_uart_host_config_t config = { 0 };
    uint32_t coding_sequence = snapshot_line_coding(&config);
    if (coding_sequence != applied_line_coding_sequence) {
        applied_line_coding_sequence = coding_sequence;
        if (!apply_line_coding(&config)) {
            ui_help_error(T_MODE_INVALID_OPTION);
        }
    }

    if (!uart_active) {
        return;
    }

    if (cdc_rx_index >= cdc_rx_count) {
        uint32_t available = tud_cdc_n_available(TRANSPARENT_UART_CDC_ITF);
        cdc_rx_count = tud_cdc_n_read(
            TRANSPARENT_UART_CDC_ITF, cdc_rx_buffer, MIN(available, sizeof(cdc_rx_buffer)));
        cdc_rx_index = 0;
    }
    while (cdc_rx_index < cdc_rx_count && uart_is_writable(M_UART_PORT)) {
        uart_putc_raw(M_UART_PORT, cdc_rx_buffer[cdc_rx_index++]);
    }

    uint8_t buffer[64];
    uint32_t write_available = tud_cdc_n_write_available(TRANSPARENT_UART_CDC_ITF);
    uint32_t count = 0;
    while (count < MIN(write_available, sizeof(buffer)) && uart_is_readable(M_UART_PORT)) {
        buffer[count++] = uart_getc(M_UART_PORT);
    }
    if (count) {
        tud_cdc_n_write(TRANSPARENT_UART_CDC_ITF, buffer, count);
        tud_cdc_n_write_flush(TRANSPARENT_UART_CDC_ITF);
    }
}

void transparent_uart_cleanup(void) {
    bool was_active = __atomic_exchange_n(&mode_active, false, __ATOMIC_ACQ_REL);
    system_config.binmode_usb_rx_queue_enable = true;
    system_config.binmode_usb_tx_queue_enable = true;

    if (was_active || uart_active) {
        hwuart_cleanup();
    }
    uart_active = false;
    cdc_rx_count = 0;
    cdc_rx_index = 0;
}
