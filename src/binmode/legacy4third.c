/*
 * This file is part of the Bus Pirate project (http://code.google.com/p/the-bus-pirate/).
 *
 * Written and maintained by the Bus Pirate project.
 *
 * To the extent possible under law, the project has
 * waived all copyright and related or neighboring rights to Bus Pirate. This
 * work is published from United States.
 *
 * For details see: http://creativecommons.org/publicdomain/zero/1.0/.
 *
 *    This program is distributed in the hope that it will be useful,
 *    but WITHOUT ANY WARRANTY; without even the implied warranty of
 *    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 */

// Authors: Dreg, mbrugman, Ian 

/* Legacy Binary Mode for third parties */

// #include <stdio.h>
#include <string.h>
#include <math.h>
#include "pico/stdlib.h"
#include "pirate.h"

#include "usb_rx.h"
#include "usb_tx.h"
#include "pirate.h"
#include "command_struct.h" //needed for same reason as bytecode and needs same fix
#include "bytecode.h"
#include "modes.h"
#include "binio_helpers.h"
#include "tusb.h"
#include "system_config.h"
#include "ui/ui_term.h"
#include "lib/bp_args/bp_cmd.h" // bp_cmd_yes_no_exit
#include "pirate/hwspi.h"
#include "pirate/mem.h"
#include "pirate/bio.h"
#include "pirate/button.h"
#include "commands/global/w_psu.h"
#include "commands/global/p_pullups.h"
#include "binmode/bpio.h"
#include "commands/global/cmd_mcu.h"
#include "hardware/spi.h"
#include "hardware/sync.h"
#include "pirate/psu.h"

const char legacy4third_mode_name[] = "Legacy Binary Mode for Flashrom and AVRdude";

#define TMPBUFF_SIZE 0x4000
#define CDCBUFF_SIZE 0x4000

#define CDC_SEND_STR(cdc_n, str) cdc_write_all(cdc_n, (const uint8_t*)(str), sizeof(str) - 1)

// Indexed by the low nibble of the 0x60 opcode. These are the rates flashrom
// documents for its spispeed= parameter, and we honour them literally: asking
// for 1M really clocks the bus at 1MHz. The original Bus Pirate v3 mapped the
// same indices onto unrelated PIC24 dividers (its "8M" was 2.6MHz and its "2M"
// was 50kHz), which is a quirk worth not reproducing. avrdude uses the same
// eight rates, so indices above 7 are rejected.
static const uint32_t legacy_spi_speeds[] = {
    30000,
    125000,
    250000,
    1000000,
    2000000,
    2600000,
    4000000,
    8000000,
};

#define LEGACY_SPI_SPEED_COUNT (sizeof(legacy_spi_speeds) / sizeof(legacy_spi_speeds[0]))
#define LEGACY_SPI_DEFAULT_SPEED 1000000
#define LEGACY_SPI_ENTRY_SPEED 125000
#define LEGACY_IDLE_RESYNC_US 100000
#define LEGACY_PAYLOAD_TIMEOUT_MS 3000

static float psu_voltage = 0.0f; // PSU voltage in volts
static float psu_current_limit = 0.0f; // PSU current limit in amps
static uint8_t* tmpbuf;
static uint8_t* cdc_buff;
static bool set_aux_pins = true;
static bool hold_value = true;
static bool wp_value = true;
static bool spi_configured = false;
static bool spi_mode_active = false;
static bool legacy_abort = false;
static const char* legacy_exit_reason = "not set";
static bool psu_started = false;
static uint32_t psu_last_error = 0;

// For Atmel parts which have a flash size > 64Kbytes, an additional
// command is needed - the "Extended High Byte" address write.  This
// bool will be set when avrdude first queries the part, and will be
// used later if required.
static bool req_EHB_write = false; // This part has > 64K flash

// Each Atmel part has a 3-byte signature.  The first byte is 0x1e,
// the second byte is used to encode the flash size of the part.
// This array contains the second signature byte for every part
// that avrdude supports that has flash > 64Kbytes.
// When avrdude initially connects to the part, it will query for
// the part's signature before we do any other operations.  We
// can use this array to compare and set req_EHB_write to 'true' if needed.
static uint8_t big_flash_parts[] = {0x97, 0x98, 0xa7, 0xa8, 0xc0}; // array of signature bytes for parts with > 64K flash

static uint8_t binmode_debug = 0; // Debug mode flag

void set_planks_auxpins(bool set)
{
    const uint8_t hold_pin = 2;
    const uint8_t wp_pin = 3;

    if (set)
    {
        bio_output(hold_pin);
        bio_put(hold_pin, hold_value ? true : false);
        system_bio_update_purpose_and_label(true, hold_pin, BP_PIN_IO, hold_value ? "HIGH" : "LOW");
        system_set_active(true, hold_pin, &system_config.aux_active);

        bio_output(wp_pin);
        bio_put(wp_pin, wp_value ? true : false);
        system_bio_update_purpose_and_label(true, wp_pin, BP_PIN_IO, wp_value ? "HIGH" : "LOW");
        system_set_active(true, wp_pin, &system_config.aux_active);
    }
    else
    {
        bio_input(hold_pin);
        bio_input(wp_pin);
        system_set_active(true, hold_pin, &system_config.aux_active);
        system_set_active(true, wp_pin, &system_config.aux_active);
    }
}


void disable_psu_legacy(void) {
    psucmd_disable();
}

void setup_spi_legacy(uint32_t spi_speed, uint8_t data_bits, uint8_t cpol, uint8_t cpah, uint8_t cs) {
    bpio_mode_configuration_t mode_config={
        .speed = spi_speed,
        .data_bits = data_bits,
        .clock_polarity = cpol,
        .clock_phase = cpah,
        .chip_select_idle = cs,
    };

    spi_configured = (mode_change_new((uint8_t*)"SPI", &mode_config) == false);
    if (set_aux_pins) {
        set_planks_auxpins(true);
    }
    system_config.binmode_usb_rx_queue_enable = true;
    system_config.binmode_usb_tx_queue_enable = true;
}

void enable_debug_legacy(void) {
    binmode_debug = 1;
}

void legacy_print(const char* text) {
    while (*text) {
        tx_fifo_try_put((char*)text);
        text++;
    }
}

bool legacy_exit_requested(void) {
    char c;

    if (legacy_abort) {
        return true;
    }
    if (button_get(0)) {
        legacy_exit_reason = "button pressed";
        legacy_abort = true;
        return true;
    }
    if (!rx_fifo_try_get(&c)) {
        return false;
    }
    if (c != 'q' && c != 'Q') {
        return false;
    }

    legacy_print("\r\nExit legacy binary mode and reset the Bus Pirate? (y/n) ");
    absolute_time_t answer_deadline = make_timeout_time_ms(15000);
    while (!time_reached(answer_deadline)) {
        if (button_get(0)) {
            legacy_print("\r\n");
            legacy_exit_reason = "button pressed at the confirmation prompt";
            legacy_abort = true;
            return true;
        }
        if (rx_fifo_try_get(&c)) {
            if (c == 'y' || c == 'Y') {
                legacy_print("y\r\n");
                legacy_exit_reason = "'q' then 'y' on the terminal";
                legacy_abort = true;
                return true;
            }
            if (c == 'n' || c == 'N') {
                legacy_print("n\r\n");
                return false;
            }
        }
    }

    legacy_print("\r\nNo answer, staying in legacy binary mode.\r\n");
    return false;
}

void cdc_write_all(uint32_t cdc_id, const uint8_t* buf, uint32_t len) {
    (void)cdc_id;

    for (uint32_t i = 0; i < len; i++) {
        while (!bin_tx_fifo_try_put((char)buf[i])) {
            if (legacy_exit_requested()) {
                return;
            }
        }
    }
}

bool read_buff(uint8_t* buf, uint32_t len) {
    uint32_t total = 0;
    char c;

    while (total < len) {
        if (bin_rx_fifo_try_get(&c)) {
            buf[total] = (uint8_t)c;
            total++;
            continue;
        }
        if (legacy_exit_requested()) {
            return false;
        }
    }

    return true;
}

bool read_payload(uint8_t* buf, uint32_t len) {
    uint32_t total = 0;
    char c;
    absolute_time_t deadline = make_timeout_time_ms(LEGACY_PAYLOAD_TIMEOUT_MS);

    while (total < len) {
        if (bin_rx_fifo_try_get(&c)) {
            buf[total] = (uint8_t)c;
            total++;
            deadline = make_timeout_time_ms(LEGACY_PAYLOAD_TIMEOUT_MS);
            continue;
        }
        if (legacy_exit_requested()) {
            return false;
        }
        if (time_reached(deadline)) {
            return false;
        }
    }

    return true;
}

bool discard_buff(uint32_t len) {
    uint8_t scratch[64];

    while (len) {
        uint32_t count = len > sizeof(scratch) ? sizeof(scratch) : len;
        if (!read_payload(scratch, count)) {
            return false;
        }
        len -= count;
    }

    return true;
}

void send_nak_padded(uint32_t pad_len) {
    uint8_t chunk[64];

    CDC_SEND_STR(1, "\x00");
    memset(chunk, 0xFF, sizeof(chunk));
    while (pad_len && !legacy_abort) {
        uint32_t count = pad_len > sizeof(chunk) ? sizeof(chunk) : pad_len;
        cdc_write_all(1, chunk, count);
        pad_len -= count;
    }
}

void cdc_full_flush(uint32_t cdc_id) {
    (void)cdc_id;
    char c;
    absolute_time_t hard_stop = make_timeout_time_ms(250);
    absolute_time_t quiet_until = make_timeout_time_ms(20);

    while (!time_reached(quiet_until) && !time_reached(hard_stop)) {
        if (button_get(0)) {
            legacy_exit_reason = "button pressed during the entry flush";
            legacy_abort = true;
            return;
        }
        if (bin_rx_fifo_try_get(&c)) {
            quiet_until = make_timeout_time_ms(20);
        }
    }
}

void spi_bus_sync(void) {
    while (spi_is_readable(M_SPI_PORT)) {
        (void)spi_get_hw(M_SPI_PORT)->dr;
    }
    spi_get_hw(M_SPI_PORT)->icr = SPI_SSPICR_RORIC_BITS;
}

void set_pins_ui(void) {
    static const char pin_labels[][5] = { "CLK", "MOSI", "MISO", "CS" };

    system_bio_update_purpose_and_label(true, M_SPI_CLK, BP_PIN_MODE, pin_labels[0]);
    system_bio_update_purpose_and_label(true, M_SPI_CDO, BP_PIN_MODE, pin_labels[1]);
    system_bio_update_purpose_and_label(true, M_SPI_CDI, BP_PIN_MODE, pin_labels[2]);
    system_bio_update_purpose_and_label(true, M_SPI_CS, BP_PIN_MODE, pin_labels[3]);
}

void reset_legacy(void) {
    spi_configured = false;
    spi_mode_active = false;
    psu_started = false;
    disable_psu_legacy();
    pullups_disable();
    bpio_mode_configuration_t mode_config = {0};
    mode_change_new((uint8_t*)"HIZ", &mode_config);
    set_planks_auxpins(false);
    set_pins_ui();
}

void legacy_protocol(void) {
    uint8_t op_byte;
    uint8_t extended_info;
    uint8_t count_zero = 0;
    uint32_t spi_speed = 0;
    uint8_t cs_init = 0x01;
    absolute_time_t last_op_time;

    legacy_abort = false;
    legacy_exit_reason = "read_buff returned without an abort flag";
    psu_started = false;
    spi_mode_active = false;
    /* =========================================================================
     * DO NOT REMOVE THE ASSIGNMENT BELOW. IT IS NOT COSMETIC.
     *
     * monitor() on core1 calls amux_sweep(), which walks the analog mux across
     * every ADC channel to read pin voltages for the display. Four of those
     * channels are BPIO4..BPIO7, which in SPI mode are MISO, CS, CLK and MOSI,
     * and each one is wired to the ADC for 60us at a time. On boards where the
     * mux is driven through the 595 shift register, every channel change is a
     * shift plus latch plus wait, so one sweep loads the live bus for
     * milliseconds and corrupts whatever transfer is in flight.
     *
     * Measured on a Bus Pirate 5 rev8, reading the same flash address over and
     * over, before this flag existed:
     *
     *      3 byte reads at 125kHz    0.19 ms     0.00% bad
     *     64 byte reads at 125kHz    4.1  ms     0.75% bad
     *    256 byte reads at 125kHz   16.4  ms     5.03% bad
     *   2048 byte reads at   1MHz   16.4  ms     5.08% bad
     *   2048 byte reads at 125kHz  131    ms    33.90% bad
     *   2048 byte reads at   8MHz    2    ms     0.00% bad
     *
     * Look at the two 16.4 ms rows: different size, different clock, identical
     * error rate. The failure tracks how long a transfer lasts, not how fast it
     * clocks, and going faster makes it better. That is the opposite of a signal
     * integrity problem and is what rules out the wiring. With this flag set,
     * every row above measures 0.00%.
     *
     * The Bus Pirate 6 and newer drive the mux from dedicated MCU pins, so a
     * channel change is four gpio_put calls and the sweep is short enough that
     * the same test measures 0.00% without this flag. The flag is set on every
     * board anyway so behaviour does not depend on which one you plugged in.
     *
     * The same glitch has been reported on the I2C lines, so it is not specific
     * to this mode. Only the pin voltage sweep is suppressed here. The PSU over
     * current check keeps running, because it selects the current sense channel
     * and never touches an IO pin.
     * https://forum.buspirate.com/t/periodic-glitch-on-both-sda-and-scl-lines-for-i2c-mode/94
     * ========================================================================= */
    system_config.binmode_suppress_monitor = true;
    cdc_full_flush(1);
    last_op_time = get_absolute_time();

    while (1) {
        if (legacy_exit_requested()) {
            return;
        }
        op_byte = 0;
        extended_info = 0;
        if (!read_buff(&op_byte, 1)) {
            return;
        }

        absolute_time_t now = get_absolute_time();
        if (absolute_time_diff_us(last_op_time, now) > LEGACY_IDLE_RESYNC_US) {
            count_zero = 0;
        }
        last_op_time = now;

        if (psu_started && !psu_status.enabled) {
            psu_started = false;
            spi_configured = false;
            if (!psu_last_error) {
                psu_last_error = PSU_ERROR_FUSE_TRIPPED;
            }
            legacy_print("\r\nPSU FAULT: target power was cut, aborting SPI operations.\r\n");
        }

        if (binmode_debug) {
            printf("\r\n-\r\nop_byte=0x%02X", op_byte);
            printf(", extended_info=0x%02X", extended_info);
        }

        if (op_byte) {
            count_zero = 0; // ugly, but simple
            if (op_byte >= 0x10 && op_byte <= 0x1F) {
                extended_info = op_byte;
                op_byte = 0x10;
            } else if (op_byte >= 0x60 && op_byte <= 0x6F) // this must be the first
            {
                extended_info = op_byte;
                op_byte = 0x60;
            } else if (op_byte >= 0x80 && op_byte <= 0x8F) {
                extended_info = op_byte;
                op_byte = 0x80;
            } else if (op_byte >= 0x40 && op_byte <= 0x4F) {
                extended_info = op_byte;
                op_byte = 0x40;
            }
        }

        if (binmode_debug) {
            printf("\r\nop_byte=0x%02X", op_byte);
            printf(", extended_info=0x%02X", extended_info);
        }
        switch (op_byte) {
            case 0x00: {
                if (!count_zero) {
                    if (binmode_debug) {
                        printf("\r\nBBIO1->");
                    }
                    CDC_SEND_STR(1, "BBIO1");
                    spi_speed = 0;
                    reset_legacy();
                }
                if (count_zero < 0xFF) {
                    count_zero++;
                }
            } break;

            case 0x0F: {
                if (binmode_debug) {
                    printf("\r\nBus Pirate CLI prompt");
                }
                // ugly hack for fixed baudarate (look flashrom src!):
                CDC_SEND_STR(1, "Bus Pirate v2.5\r\nCommunity Firmware v7.1\r\nHiZ>");
            } break;

            case 0x01: {
                if (binmode_debug) {
                    printf("\r\nSPI1->");
                }
                spi_speed = LEGACY_SPI_ENTRY_SPEED;
                setup_spi_legacy(spi_speed, 8, 0, 0, cs_init);
                spi_mode_active = true;
                CDC_SEND_STR(1, "SPI1");
            } break;

            case 0x40: {
                if (binmode_debug) {
                    printf("\r\npsu...");
                }

                // PSU
                if (set_aux_pins) {
                    set_planks_auxpins(true);
                }

                if ((extended_info & 0b00001000) == 0) {
                    psu_started = false;
                    disable_psu_legacy();
                } else {
                    uint32_t result = psucmd_enable(psu_voltage, psu_current_limit, false, 100);
                    if (result) {
                        psu_started = false;
                        psu_last_error = result;
                        legacy_print("\r\nPSU ERROR: target is NOT powered by the Bus Pirate.\r\n");
                    } else {
                        psu_started = true;
                        if (binmode_debug) {
                            printf("\r\nPSU Enabled");
                        }
                    }
                }

                // Pull-ups
                if ((extended_info & 0b00000100) == 0) {
                    if (binmode_debug) {
                        printf("\r\npullup_disable");
                    }
                    pullups_disable();
                } else {
                    if (binmode_debug) {
                        printf("\r\npullup_enable");
                    }
                    pullups_enable();
                }

                // AUX
                if ((extended_info & 0b00000010) == 0) {
                    if (binmode_debug) {
                        printf("\r\naux_disable");
                    }
                } else {
                    if (binmode_debug) {
                        printf("\r\naux_enable");
                    }
                }

                // CS
                if ((extended_info & 0b00000001) == 0) {
                    if (binmode_debug) {
                        printf("\r\ncs 0");
                    }
                    cs_init = 0x00;
                } else {
                    if (binmode_debug) {
                        printf("\r\ncs 1");
                    }
                    cs_init = 0x01;
                }

                CDC_SEND_STR(1, "\x01");
            } break;

            case 0x60: {
                if (binmode_debug) {
                    printf("\r\nspi_speed");
                }
                uint8_t speed_index = extended_info & 0x0F;
                if (speed_index >= LEGACY_SPI_SPEED_COUNT) {
                    if (binmode_debug) {
                        printf("\r\ninvalid speed index %d", speed_index);
                    }
                    CDC_SEND_STR(1, "\x00");
                    break;
                }
                spi_speed = legacy_spi_speeds[speed_index];
                if (binmode_debug) {
                    printf("\r\nspi_speed: %d", spi_speed);
                }
                CDC_SEND_STR(1, "\x01");
            } break;

            case 0x80: {
                if (binmode_debug) {
                    printf("\r\nhwspi_init");
                }

                // SMP sample time (middle=0)
                if ((extended_info & 0x1) == 0) {
                    if (binmode_debug) {
                        printf("\r\nSMP 0");
                    }
                } else {
                    if (binmode_debug) {
                        printf("\r\nSMP 1");
                    }
                }

                // CKE clock edge (active to idle=1)
                if ((extended_info & 0x2) == 0) {
                    if (binmode_debug) {
                        printf("\r\nCKE 0");
                    }
                } else {
                    if (binmode_debug) {
                        printf("\r\nCKE 1");
                    }
                }

                // CKP clock idle phase (low=0)
                if ((extended_info & 0x4) == 0) {
                    if (binmode_debug) {
                        printf("\r\nCKP 0");
                    }
                } else {
                    if (binmode_debug) {
                        printf("\r\nCKP 1");
                    }
                }

                // Output HiZ(0)/3.3v(1)
                if ((extended_info & 0x8) == 0) {
                    if (binmode_debug) {
                        printf("\r\nHiZ");
                    }
                } else {
                    if (binmode_debug) {
                        printf("\r\n3.3v");
                    }
                }

                if (spi_speed == 0) {
                    spi_speed = LEGACY_SPI_DEFAULT_SPEED;
                }
                uint8_t cpol = (extended_info & 0x4) ? 1 : 0;
                uint8_t cpha = (extended_info & 0x2) ? 0 : 1;
                setup_spi_legacy(spi_speed, 8, cpol, cpha, cs_init);
                hwspi_select();
                CDC_SEND_STR(1, "\x01");
            } break;

            case 0x03: {
                if (binmode_debug) {
                    printf("\r\nhwspi_deselect");
                }
                hwspi_deselect();
                CDC_SEND_STR(1, "\x01");
            } break;

            case 0x02: {
                if (binmode_debug) {
                    printf("\r\nhwspi_select");
                }
                hwspi_select();
                CDC_SEND_STR(1, "\x01");
            } break;

            case 0x10: {
                if (binmode_debug) {
                    printf("\r\nBulk SPI transfer");
                }
                uint32_t bytes2read = (extended_info & 0x0F) + 1;
                if (binmode_debug) {
                    printf("\r\nbytes_to_read: %d", bytes2read);
                }
                if (!spi_mode_active || !spi_configured) {
                    CDC_SEND_STR(1, "\x00");
                    break;
                }
                CDC_SEND_STR(1, "\x01");
                if (!read_payload(tmpbuf, bytes2read)) {
                    if (legacy_abort) {
                        return;
                    }
                    break;
                }
                spi_bus_sync();
                if (binmode_debug) {
                    printf("\r\n>> ");
                }
                bool is_read_sig_cmd = false;  // true when reading any one of the signature bytes
                uint8_t read_sig_byte_inx = 0; // which signature byte to read
                for (int i = 0; i < bytes2read; i++) {
                    if (button_get(0)) {
                        return;
                    }
                    if (binmode_debug) {
                        printf("\r\n0x%02X | ", tmpbuf[i]);
                    }
                    // This hack required to handle parts with more than
                    // 64K bytes of flash.  Signature byte 2 indicates flash
                    // size - we'll use this byte to set a global flag if
                    // this part requires writing the Extended High Byte address
                    // First - check if we are receiving a command 0x30 (query signature);
                    // this command is 4 bytes of SPI:
                    // 0 -> avrdude/BP sends 0x30
                    // 1 -> avrdude/BP clocks out 0, and part returns 0x30 to confirm command
                    // 2 -> avrdude/BP sends which of the 3 signature bytes to read
                    // 3 -> avrdude/BP clocks out 0, part returns requested value.
                    if (i == 0 && tmpbuf[i] == 0x30) {
                        // this is the start of a read signature command - set state active
                        is_read_sig_cmd = true;
                    }
                    if (is_read_sig_cmd && i == 2) {
                        // this is 3rd byte of the SPI sequence from BP to part - which signature byte to read
                        read_sig_byte_inx = tmpbuf[i];
                    }
                    tmpbuf[i] = hwspi_write_read(tmpbuf[i]);
                    if (binmode_debug) {
                        printf("0x%02X", tmpbuf[i]);
                        if (is_read_sig_cmd) {
                            if (i == 0) {
                                printf("  - read signature command");
                            } else if (i == 2) {
                                printf("  - read signature byte %d", read_sig_byte_inx);
                            }
                        }
                    }
                    // IF
                    // we're in the process of requesting part signature bytes AND
                    // this is a request to read signature byte 1 AND
                    // this is the part's response to the 4th byte in the SPI sequence
                    // THEN
                    // tmpbuf[i] holds the signature byte 2 - this is what we need to
                    // determine flash size.
                    if (is_read_sig_cmd && read_sig_byte_inx == 1 && i == 3) {
                        if (binmode_debug) {
                            printf("  - signature part ID 0x%02x", tmpbuf[i]);
                        }
                        req_EHB_write = false; // assume this part's flash is <= 64Kb flash
                        for (uint8_t ii = 0; ii < sizeof(big_flash_parts); ++ii) {
                            if (big_flash_parts[ii] == tmpbuf[3]) {
                                // we have a match!  This part has more than 64K bytes of flash
                                req_EHB_write = true;
                                break;
                            }
                        }
                        if (binmode_debug) {
                            if (req_EHB_write) {
                                printf(", requires EHB write");
                            } else {
                                printf(", does not require EHB write");
                            }
                        }
                    }
                }
                if (binmode_debug) {
                    printf("\r\n");
                }
                cdc_write_all(1, tmpbuf, bytes2read);
            } break;

            // SPI b00000100 (0x04) - Write then read & b00000101 (0x05) - Write then read, no CS
            case 0x04:
            case 0x05: {
                uint16_t bytes_to_read = 0;
                uint16_t bytes_to_write = 0;

                if (!spi_mode_active) {
                    CDC_SEND_STR(1, "\x00");
                    break;
                }

                if (!read_payload(tmpbuf, 4)) {
                    if (legacy_abort) {
                        return;
                    }
                    break;
                }
                if (binmode_debug) {
                    printf("\r\nbytes_to_write H: 0x%02X", tmpbuf[0]);
                    printf("\r\nbytes_to_write L: 0x%02x", tmpbuf[1]);
                    printf("\r\nbytes_to_read H: 0x%02X", tmpbuf[2]);
                    printf("\r\nbytes_to_read L: 0x%02x", tmpbuf[3]);
                }
                bytes_to_write = (tmpbuf[0] << 8) | tmpbuf[1];
                bytes_to_read = (tmpbuf[2] << 8) | tmpbuf[3];
                if (binmode_debug) {
                    printf("\r\nbytes_to_write: %d", bytes_to_write);
                    printf("\r\nbytes_to_read: %d", bytes_to_read);
                }

                if (0x00 == bytes_to_read && 0x00 == bytes_to_write) {
                    // for AVRDUDE
                    CDC_SEND_STR(1, "\x01");
                    break;
                }

                if (((uint32_t)bytes_to_write + (uint32_t)bytes_to_read) > TMPBUFF_SIZE) {
                    if (binmode_debug) {
                        printf("\r\nlength out of range");
                    }
                    if (!discard_buff(bytes_to_write)) {
                        if (legacy_abort) {
                            return;
                        }
                        break;
                    }
                    send_nak_padded(bytes_to_read);
                    break;
                }

                if (bytes_to_write) {
                    if (!read_payload(tmpbuf, bytes_to_write)) {
                        if (legacy_abort) {
                            return;
                        }
                        break;
                    }
                }

                if (!spi_configured) {
                    send_nak_padded(bytes_to_read);
                    break;
                }

                spi_bus_sync();

                if (0x04 == op_byte) {
                    hwspi_select();
                }
                if (binmode_debug) {
                    printf("\r\n>> ");
                }
                uint32_t total_bytes_spi = (uint32_t)bytes_to_write + (uint32_t)bytes_to_read;
                for (uint32_t j = 0; j < total_bytes_spi; j++) {
                    if (button_get(0)) {
                        hwspi_deselect();
                        return;
                    }
                    if (binmode_debug) {
                        printf("\r\n[%d] 0x%02X -> | ", j, tmpbuf[j]);
                    }
                    tmpbuf[j] = hwspi_write_read(j >= bytes_to_write ? 0xFF : tmpbuf[j]);
                    if (binmode_debug) {
                        printf("<- 0x%02X", tmpbuf[j]);
                    }
                }
                if (0x04 == op_byte) {
                    hwspi_deselect();
                }

                if (bytes_to_write) {
                    uint32_t delta = bytes_to_write - 1;
                    tmpbuf[delta] = 0x01;
                    cdc_write_all(1, tmpbuf + delta, (uint32_t)bytes_to_read + 1);
                } else {
                    CDC_SEND_STR(1, "\x01");
                    if (bytes_to_read) {
                        cdc_write_all(1, tmpbuf, bytes_to_read);
                    }
                }
                if (binmode_debug) {
                    printf("\r\nsent %d bytes", bytes_to_read + 1);
                }
            } break;

            case 0x06: // AVR EXTENDED COMMAND
            {
                CDC_SEND_STR(1, "\x01");
                if (!read_payload(&op_byte, 1)) {
                    if (legacy_abort) {
                        return;
                    }
                    break;
                }
                if (binmode_debug) {
                    printf("\r\n-\r\nAVR op_byte=0x%02X", op_byte);
                }
                switch (op_byte) {
                    case 0x00:
                        if (binmode_debug) {
                            printf("\r\nAVR NOOP");
                        }
                        CDC_SEND_STR(1, "\x01");
                        break;

                    case 0x01:
                        if (binmode_debug) {
                            printf("\r\nAVR VERSION");
                        }
                        CDC_SEND_STR(1, "\x01\x00\x01");
                        break;

                    case 0x02:
                        if (binmode_debug) {
                            printf("\r\nAVR BULK READ");
                        }

                        if (!read_payload(tmpbuf, 8)) {
                            if (legacy_abort) {
                                return;
                            }
                            break;
                        }

                        uint32_t addr = (tmpbuf[0] << 24) | (tmpbuf[1] << 16) | (tmpbuf[2] << 8) | tmpbuf[3];
                        uint32_t len = (tmpbuf[4] << 24) | (tmpbuf[5] << 16) | (tmpbuf[6] << 8) | tmpbuf[7];

                        if (binmode_debug) {
                            printf("\r\naddr: 0x%08X, len: 0x%08X", addr, len);
                            if (req_EHB_write) {
                                printf(" -- EHB is 0x%02x", (addr >> 16) & 0x03);
                            }
                        }

                        if (!spi_mode_active || !spi_configured) {
                            CDC_SEND_STR(1, "\x00");
                            break;
                        }

                        // addr counts words, len counts bytes. Without the extended
                        // high byte command we can only reach 64K words; with it the
                        // two extra address bits reach 256K words.
                        uint32_t addr_limit = req_EHB_write ? 0x40000 : 0x10000;
                        uint32_t words = (len + 1) / 2;
                        if (addr >= addr_limit || words > addr_limit || (addr + words) > addr_limit) {
                            if (binmode_debug) {
                                printf("\r\nAVR bulk read out of range");
                            }
                            CDC_SEND_STR(1, "\x00");
                            break;
                        }

                        CDC_SEND_STR(1, "\x01");

                        if (binmode_debug) {
                            printf("\r\n>> ");
                        }
                        spi_bus_sync();
                        while (len > 0) {
                            if (button_get(0)) {
                                return;
                            }
                            if (req_EHB_write) {
                                hwspi_write_read(0x4d); // AVR_LOAD_ADDRESS_EXTENDED_HIGH_BYTE_COMMAND
                                hwspi_write_read(0x00);
                                hwspi_write_read((addr >> 16) & 0x03);  // just the two lowest bits
                                hwspi_write_read(0x00);
                            }
                            hwspi_write_read(0x20); // AVR_FETCH_LOW_BYTE_COMMAND
                            hwspi_write_read((addr >> 8) & 0xFF);
                            hwspi_write_read(addr & 0xFF);
                            uint8_t byte_flash = hwspi_write_read(0x00);
                            if (binmode_debug) {
                                printf("\r\n0x%02X", byte_flash);
                            }
                            cdc_write_all(1, &byte_flash, 1); // Send the readed byte
                            len--;
                            if (len > 0) {
                                hwspi_write_read(0x28); // AVR_FETCH_HIGH_BYTE_COMMAND
                                hwspi_write_read((addr >> 8) & 0xFF);
                                hwspi_write_read(addr & 0xFF);
                                uint8_t byte_flash = hwspi_write_read(0x00);
                                if (binmode_debug) {
                                    printf("\r\n0x%02X", byte_flash);
                                }
                                cdc_write_all(1, &byte_flash, 1); // Send the readed byte
                                len--;
                            }
                            addr++;
                        }
                        if (binmode_debug) {
                            printf(" - end addr 0x%08x\r\n", addr);
                        }
                        break;

                    default:
                        // error
                        CDC_SEND_STR(1, "\x00");
                        break;
                }
            } break;

            default: {
                if (binmode_debug) {
                    printf("\r\nunsupported op 0x%02X", op_byte);
                }
                CDC_SEND_STR(1, "\x00");
            } break;
        }
    }
}

// handler needs to be cooperative multitasking until mode is enabled
void legacy4third_mode(void) {
    static uint32_t mode_active = 0;
    if (mode_active == 0) {
        mode_active++;
        // enable_debug_legacy();
        system_config.binmode_usb_rx_queue_enable = true;
        system_config.binmode_usb_tx_queue_enable = true;
        set_pins_ui();
    } else if (mode_active == 1) {
        set_aux_pins = true;
        mode_active++;

        bp_yn_result_t r;

        r = bp_cmd_yes_no_exit("\r\nSet OUTPUT HOLD(IO2) & WP(IO3) pins? (no=INPUT)");
        if (r == BP_YN_EXIT) goto finish_legacy;
        set_aux_pins = (r == BP_YN_YES);

        if (set_aux_pins) {
            r = bp_cmd_yes_no_exit("\r\nSet HOLD HIGH? (no=LOW)");
            if (r == BP_YN_EXIT) goto finish_legacy;
            hold_value = (r == BP_YN_YES);

            r = bp_cmd_yes_no_exit("\r\nSet WP HIGH? (no=LOW)");
            if (r == BP_YN_EXIT) goto finish_legacy;
            wp_value = (r == BP_YN_YES);
        }
        if (set_aux_pins) {
            set_planks_auxpins(true);
        }

        static const bp_val_constraint_t psu_voltage_constraint = {
            .type = BP_VAL_FLOAT,
            .f = { .min = 0.8f, .max = 5.0f, .def = 3.3f },
        };
        static const bp_val_constraint_t psu_current_constraint = {
            .type = BP_VAL_FLOAT,
            .f = { .min = 0.0f, .max = 500.0f, .def = 200.0f },
        };

        printf("\r\n%sPower supply\r\nVolts (0.80V-5.00V)%s", ui_term_color_info(), ui_term_color_reset());
        if (bp_cmd_prompt(&psu_voltage_constraint, &psu_voltage) == BP_CMD_EXIT)
            goto finish_legacy;

        if (binmode_debug) {
            printf("\r\nVolts: %2.2f\n", psu_voltage);
        }

        printf("\r\n%sMaximum current (0mA-500mA)%s", ui_term_color_info(), ui_term_color_reset());
        if (bp_cmd_prompt(&psu_current_constraint, &psu_current_limit) == BP_CMD_EXIT)
            goto finish_legacy;

        if (binmode_debug) {
            printf("\r\nCurrent: %2.2f\n",psu_current_limit);
        }

        printf("\r\n%sPower supply set to %2.2fV, %3.0fmA%s\r\n",
               ui_term_color_info(), psu_voltage, psu_current_limit, ui_term_color_reset());

        cdc_buff = (uint8_t*)mem_alloc(CDCBUFF_SIZE + TMPBUFF_SIZE, 0);
        if (binmode_debug) {
            printf("\r\ncdc_buff: %p\r\n", cdc_buff);
        }
        if (cdc_buff == NULL) {
            printf("\r\nError: Not enough memory for cdc_buff!\r\n");
            goto finish_legacy;
        }
        printf("\r\nSPI speed follows what the host asks for: 30k, 125k, 250k, 1M, 2M,\r\n"
               "2.6M, 4M, 8M. Long wires are the usual cause of bad reads. Use\r\n"
               "cables of 10cm or shorter, and spispeed=125k is the recommended\r\n"
               "setting.\r\n");
        printf("\r\nPin voltage readings are paused while this mode runs and resume on\r\n"
               "exit. The analog mux that measures them touches the SPI pins and\r\n"
               "corrupts transfers on older Bus Pirate 5 hardware. Newer boards are\r\n"
               "not affected, but readings pause on all of them so behaviour matches.\r\n");
        printf("\r\nDone! Just execute flashrom or avrdude using the binary com port\r\n"
               "To exit: press the button, or type 'q' here and confirm with 'y'.\r\n"
               "Either way the Bus Pirate resets on exit.\r\n");

        tmpbuf = cdc_buff + CDCBUFF_SIZE;
        memset(cdc_buff, 0, CDCBUFF_SIZE);
        memset(tmpbuf, 0, TMPBUFF_SIZE);
        psu_last_error = 0;
        system_config.binmode_usb_rx_queue_enable = true;
        system_config.binmode_usb_tx_queue_enable = true;
        legacy_protocol();
        printf("\r\nExit reason: %s\r\n", legacy_exit_reason);
        finish_legacy:
        system_config.binmode_suppress_monitor = false;
        if (psu_last_error) {
            printf("\r\nWARNING: the power supply reported error %d during this session.\r\n"
                   "The target was not powered by the Bus Pirate.\r\n", psu_last_error);
        }
        printf("\r\nExiting Legacy Binary Mode...\r\n");
        printf("Resetting Bus Pirate...\r\n");
        printf("After reconnect press enter to use Bus Pirate in other modes.\r\n");
        sleep_ms(1000); 
        system_config.binmode_usb_rx_queue_enable = true;
        system_config.binmode_usb_tx_queue_enable = true;
        if (NULL != cdc_buff)
        {
            mem_free(cdc_buff);
        }
        cdc_buff = NULL;
        reset_legacy();
        system_bio_update_purpose_and_label(false, M_SPI_CLK, BP_PIN_MODE, 0);
        system_bio_update_purpose_and_label(false, M_SPI_CDO, BP_PIN_MODE, 0);
        system_bio_update_purpose_and_label(false, M_SPI_CDI, BP_PIN_MODE, 0);
        system_bio_update_purpose_and_label(false, M_SPI_CS, BP_PIN_MODE, 0);
        set_planks_auxpins(false);
        cmd_mcu_reset();
    }
}

/*
 Hercules testing:
 HEX1: 01 49 60 8A
 HEX2: 02
 HEX3: 05 00 01 00 03 9f 03
*/
