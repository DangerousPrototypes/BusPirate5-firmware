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

/**
 * @file bp3unbrick.c
 * @brief Binmode that writes bootloader v4.5 to a Bus Pirate v3 over its ICSP header.
 * @details A Bus Pirate v3 with a blank or broken flash cannot load firmware over USB. This
 *          binmode writes the ds30 bootloader v4.5 to its PIC24FJ64GA002 over ICSP
 *          (pirate/pic24_icsp.c), the same words that the combined firmware + bootloader
 *          .hex files carry, and verifies the whole flash. It can then hold the Bus
 *          Pirate v3 in its bootloader, as the PGC-PGD jumper does, so pirate-loader can load
 *          the firmware over the Bus Pirate v3's own USB port.
 *
 *          Everything runs once, when the binmode command selects it. The default binmode
 *          comes back at the end, so the binary port is never left to this binmode.
 *
 *          Bus Pirate v3 ICSP header JP2: pin 1 PGC to IO7, pin 2 PGD to IO6, pin 3 GND to
 *          GND, pin 5 MCLR to IO5. Pin 4 (3V3) may go to VOUT as the IO reference; without
 *          it the power supply gives 3.3 V to VOUT. The Bus Pirate v3 runs from its own USB.
 */

#include <stdio.h>
#include "pico/stdlib.h"
#include "pirate.h"
#include "system_config.h"
#include "command_struct.h"
#include "lib/bp_args/bp_cmd.h"
#include "commands/global/w_psu.h"
#include "commands/global/p_pullups.h"
#include "display/scope.h"
#include "pirate/amux.h"
#include "pirate/bio.h"
#include "pirate/button.h"
#include "pirate/intercore_helpers.h"
#include "pirate/pic24_icsp.h"
#include "pirate/psu.h"
#include "ui/ui_term.h"
#include "usb_rx.h"
#include "binmode/binmodes.h"
#include "binmode/bp3unbrick.h"
#include "binmode/bp3unbrick_bootloader.h"

const char bp3unbrick_name[] = "Bus Pirate v3 unbrick: bootloader v4.5 (ICSP)";

#define BP3_PGC BIO7  // JP2 pin 1
#define BP3_PGD BIO6  // JP2 pin 2
#define BP3_MCLR BIO5 // JP2 pin 5

#define BP3_DEVID 0x0447u                        // PIC24FJ64GA002
#define BP3_ROWS 344u                            // PC 0x000000-0x00ABFE, 22016 instructions
#define BP3_ROW_SPAN (2u * PIC24_ICSP_ROW_WORDS) // PC addresses in a row
#define BP3_CW2 0x00ABFCu
#define BP3_CW1 0x00ABFEu
#define BP3_VOLTS 3.3f
#define BP3_PSU_MA 50.0f           // only the IO buffers: the Bus Pirate v3 has its own supply
#define BP3_ERASE_TIMEOUT_MS 2000u // DS39768 P11 is 400 ms, the chip takes about 200 ms
#define BP3_ROW_TIMEOUT_MS 100u    // P13 is 2 ms

// PGC phase of each attempt: the retries run at half and a quarter of the speed
static const uint32_t phase_ns[] = { 500u, 1000u, 2000u };

static const char label_pgc[] = "PGC";
static const char label_pgd[] = "PGD";
static const char label_mclr[] = "MCLR";

static bool pins_claimed;           // the three pins and their labels are ours
static bool psu_ours;               // this binmode turned the power supply on
static bool psu_feeds_vout;         // VOUT comes from the power supply, not from outside
static absolute_time_t write_start; // when the user confirmed the erase

typedef enum {
    BP3_OK,
    BP3_CANCELLED,
    BP3_NO_TARGET, // no retry fixes these three
    BP3_LINE_FAULT,
    BP3_WRONG_DEVICE,
    BP3_NO_ANSWER, // these are retried with a slower clock
    BP3_ERASE_FAILED,
    BP3_NOT_BLANK,
    BP3_WRITE_FAILED,
    BP3_VERIFY_FAILED,
} bp3_result_t;

typedef struct {
    uint32_t differ;  // words that differ
    uint32_t address; // the first of them
    uint32_t read;
    uint32_t expected;
} bp3_diff_t;

static void print_volts(uint32_t mv) {
    printf("%s%u.%02u%sV",
           ui_term_color_num_float(),
           (unsigned)(mv / 1000u),
           (unsigned)((mv % 1000u) / 10u),
           ui_term_color_reset());
}

static void print_error(const char* text) {
    printf("%sError:%s %s", ui_term_color_error(), ui_term_color_reset(), text);
}

// mV on VOUT and on an IO pin, from the last amux_sweep()
static uint32_t vout_mv(void) {
    return *hw_pin_voltage_ordered[0];
}

static uint32_t pin_mv(uint8_t bio) {
    return *hw_pin_voltage_ordered[bio + 1];
}

static void print_wiring(void) {
    printf("Writes bootloader v4.5 to a Bus Pirate v3, through its ICSP header JP2:\r\n\r\n");
    printf("  %sJP2 pin%s     %sthis Bus Pirate%s\r\n",
           ui_term_color_info(),
           ui_term_color_reset(),
           ui_term_color_info(),
           ui_term_color_reset());
    printf("  1 PGC       IO7\r\n");
    printf("  2 PGD       IO6\r\n");
    printf("  3 GND       GND\r\n");
    printf("  4 3V3       VOUT, or not connected\r\n");
    printf("  5 MCLR      IO5\r\n\r\n");
    printf("The Bus Pirate v3 runs from its own USB cable.\r\n\r\n");
}

static const uint8_t bp3_pins[] = { BP3_PGC, BP3_PGD, BP3_MCLR };

// The pins must be free, or aux pins (a/A/@), which this binmode takes over
static bool pins_free(void) {
    for (uint8_t i = 0; i < count_of(bp3_pins); i++) {
        uint8_t pin = bp3_pins[i];
        if (system_config.pin_func[pin + 1] == BP_PIN_DEBUG) {
            printf("%sError:%s IO%d is the debug UART\r\n", ui_term_color_error(), ui_term_color_reset(), pin);
            return false;
        }
        if (system_config.pin_labels[pin + 1] != 0 && !(system_config.aux_active & (0x01 << pin))) {
            printf("%sError:%s IO%d is in use by %s\r\n",
                   ui_term_color_error(),
                   ui_term_color_reset(),
                   pin,
                   system_config.pin_labels[pin + 1]);
            return false;
        }
    }
    return true;
}

// The LCD redraws, every 500 ms, the labels whose bit is set in system_config.pin_changed, then
// clears them all, and psucmd_enable() overwrites them: a full redraw shows every label for sure.
// Disable first, as display/default.c does: a forced update alone adds a second refresh timer.
static void lcd_redraw(void) {
    icm_core0_send_message_synchronous(BP_ICM_DISABLE_LCD_UPDATES);
    icm_core0_send_message_synchronous(BP_ICM_FORCE_LCD_UPDATE);
}

// The three pins become inputs before VOUT may come up, so an aux output left on them never drives
static void claim_pins(void) {
    static const char* const labels[] = { label_pgc, label_pgd, label_mclr };
    for (uint8_t i = 0; i < count_of(bp3_pins); i++) {
        bio_input(bp3_pins[i]);
        system_set_active(false, bp3_pins[i], &system_config.aux_active);
        system_bio_update_purpose_and_label(true, bp3_pins[i], BP_PIN_MODE, labels[i]);
    }
    pins_claimed = true;
}

static void print_no_target(uint32_t mclr) {
    printf("%sError:%s no target, MCLR (IO5) is at ", ui_term_color_error(), ui_term_color_reset());
    print_volts(mclr);
    printf(".\r\nPower the Bus Pirate v3 from its USB port and check the MCLR wire, JP2 pin 5.\r\n");
}

// The IO buffers run from VOUT: the Bus Pirate v3's 3.3 V on VOUT, or the power supply at 3.3 V.
// no_target tells a Bus Pirate v3 without power from the other failures.
static bool io_reference(bool* no_target) {
    if (scope_running) {
        print_error("the scope is using the analog subsystem, stop it with 'ss' first\r\n");
        return false;
    }
    bool pullups_were_on = system_config.pullup_enabled;
    pullups_disable(); // they would pull MCLR up even with the Bus Pirate v3 unpowered
    if (pullups_were_on) {
        printf("Pull-up resistors: off\r\n");
    }
    amux_sweep();
    uint32_t vout = vout_mv();
    if (psu_status.enabled) {
        if (psu_status.voltage_actual_float < 3.2f || psu_status.voltage_actual_float > 3.4f || vout < 3000u) {
            printf("%sError:%s the power supply is on at %s%1.2f%sV, VOUT ",
                   ui_term_color_error(),
                   ui_term_color_reset(),
                   ui_term_color_num_float(),
                   psu_status.voltage_actual_float,
                   ui_term_color_reset());
            print_volts(vout);
            printf(", and the Bus Pirate v3 runs at 3.3V.\r\n"
                   "Turn it off with 'w' and select this binmode again.\r\n");
            return false;
        }
        psu_feeds_vout = true;
        printf("Power supply: already on at 3.3V\r\n");
        return true;
    }
    if (vout > 3600u) {
        printf("%sError:%s VOUT has ", ui_term_color_error(), ui_term_color_reset());
        print_volts(vout);
        printf(" from outside, and the Bus Pirate v3 runs at 3.3V\r\n");
        return false;
    }
    if (vout >= 3000u) {
        printf("VOUT: ");
        print_volts(vout);
        printf(" from outside, the reference of the IO pins\r\n");
        return true;
    }
    // The analog mux reads MCLR even with the IO buffers unpowered. Without the Bus Pirate v3's
    // pull-up, the power supply would feed its 3.3 V rail if VOUT is wired to JP2 pin 4.
    uint32_t mclr = pin_mv(BP3_MCLR);
    if (mclr < 1600u) {
        print_no_target(mclr);
        *no_target = true;
        return false;
    }
    uint32_t result = psucmd_enable(BP3_VOLTS, BP3_PSU_MA, false, 10);
    switch (result) {
        case PSU_OK:
            psu_ours = true;
            psu_feeds_vout = true;
            printf("Power supply: %s%1.2f%sV on VOUT for the IO pins\r\n",
                   ui_term_color_num_float(),
                   psu_status.voltage_actual_float,
                   ui_term_color_reset());
            return true;
        case PSU_ERROR_FUSE_TRIPPED:
            print_error("the power supply went over its 50mA limit. VOUT only feeds the IO\r\n"
                        "pins: is it shorted, or wired to a Bus Pirate v3 that has no USB power?\r\n");
            return false;
        case PSU_ERROR_VOUT_LOW:
            print_error("VOUT stays low with the power supply on: is VOUT shorted?\r\n");
            return false;
        default:
            print_error("VOUT has a voltage from outside, the power supply did not start\r\n");
            return false;
    }
}

// VOUT feeds the IO buffers, and the Bus Pirate v3 pulls MCLR up to its 3.3 V through 2 kOhm:
// no pull-up, no power. Checked before every attempt, so a lost supply stops the retries.
static bool target_powered(bool report) {
    bio_input(BP3_PGD);
    bio_input(BP3_PGC);
    bio_input(BP3_MCLR);
    busy_wait_ms(2);
    amux_sweep();
    uint32_t vout = vout_mv();
    if (vout < 3000u) {
        printf("%sError:%s VOUT is at ", ui_term_color_error(), ui_term_color_reset());
        print_volts(vout);
        printf("%s\r\n",
               psu_feeds_vout ? ": the power supply turned off, over its current limit?"
                              : ": its outside reference is gone");
        return false;
    }
    uint32_t mclr = pin_mv(BP3_MCLR);
    if (!bio_get(BP3_MCLR) || mclr < vout * 4u / 5u) {
        print_no_target(mclr);
        return false;
    }
    if (report) {
        printf("MCLR: ");
        print_volts(mclr);
        printf(", pulled up: the Bus Pirate v3 is powered\r\n");
    }
    return true;
}

// Keys typed before a question must not answer it
static void drop_typed_input(void) {
    char c;
    while (rx_fifo_try_get(&c)) {
    }
}

// A Bus Pirate v3 without power can be plugged in while the question waits
static bool io_reference_or_retry(void) {
    while (true) {
        bool no_target = false;
        if (io_reference(&no_target)) {
            return true;
        }
        drop_typed_input();
        if (!no_target || bp_cmd_yes_no_exit("\r\nTry again?") != BP_YN_YES) {
            return false;
        }
    }
}

// Drive PGC and PGD (MCLR is driven low) and check that the three lines follow
static bool lines_at(bool pgc, bool pgd) {
    static const struct {
        uint8_t pin;
        const char* name;
    } lines[] = { { BP3_MCLR, "MCLR (IO5)" }, { BP3_PGC, "PGC (IO7)" }, { BP3_PGD, "PGD (IO6)" } };
    bool want[] = { false, pgc, pgd };
    bio_put(BP3_PGC, pgc);
    bio_put(BP3_PGD, pgd);
    busy_wait_us(100);
    amux_sweep();
    uint32_t vout = vout_mv();
    for (uint8_t i = 0; i < count_of(lines); i++) {
        uint32_t mv = pin_mv(lines[i].pin);
        if (want[i] ? (mv >= vout * 4u / 5u) : (mv <= vout / 5u)) {
            continue;
        }
        printf("%sError:%s %s is at ", ui_term_color_error(), ui_term_color_reset(), lines[i].name);
        print_volts(mv);
        printf(" while driven %s: a short, or another programmer on the line?\r\n", want[i] ? "high" : "low");
        return false;
    }
    return true;
}

// With MCLR low the PIC is in reset and its pins are inputs, so each line must follow this Bus Pirate
static bool lines_follow(void) {
    bio_put(BP3_MCLR, false);
    bio_output(BP3_MCLR);
    bio_put(BP3_PGC, false);
    bio_output(BP3_PGC);
    bio_put(BP3_PGD, false);
    bio_output(BP3_PGD);
    if (!lines_at(false, false) || !lines_at(false, true) || !lines_at(true, false)) {
        return false;
    }
    bio_put(BP3_PGC, false);
    printf("Lines: MCLR, PGC and PGD follow this Bus Pirate\r\n");
    return true;
}

static const char* devrev_name(uint16_t devrev) {
    switch (devrev) {
        case 0x3003:
            return "A3/A4";
        case 0x3042:
            return "B4";
        case 0x3043:
            return "B5";
        case 0x3046:
            return "B8";
        default:
            return "unknown";
    }
}

// The bootloader row at this PC address, or NULL for a row that stays erased
static const uint32_t* bootloader_row(uint32_t address) {
    for (uint8_t r = 0; r < BP3UNBRICK_BOOTLOADER_ROWS; r++) {
        if (bp3unbrick_bootloader[r].address == address) {
            return bp3unbrick_bootloader[r].words;
        }
    }
    return NULL;
}

// Compare one row of the PIC with what it must hold (NULL: erased). The unimplemented upper byte
// of the configuration words does not read back as written, and CW1 bit 15 always reads 0.
static void compare_row(uint32_t address, const uint32_t* expected, bp3_diff_t* diff) {
    uint32_t words[PIC24_ICSP_ROW_WORDS];
    pic24_icsp_read(address, words, PIC24_ICSP_ROW_WORDS);
    for (uint32_t i = 0; i < PIC24_ICSP_ROW_WORDS; i++) {
        uint32_t at = address + 2u * i;
        uint32_t want = expected ? expected[i] : PIC24_ICSP_ERASED;
        uint32_t mask = (at == BP3_CW2) ? 0x00FFFFu : (at == BP3_CW1) ? 0x007FFFu : 0xFFFFFFu;
        if ((words[i] ^ want) & mask) {
            if (diff->differ++ == 0) {
                diff->address = at;
                diff->read = words[i];
                diff->expected = want;
            }
        }
    }
}

static void print_diff(const char* what, const bp3_diff_t* diff) {
    printf("%sError:%s %s: %u %s\r\nThe first at 0x%06X: read 0x%06X, expected 0x%06X\r\n",
           ui_term_color_error(),
           ui_term_color_reset(),
           what,
           (unsigned)diff->differ,
           diff->differ == 1 ? "word differs" : "words differ",
           (unsigned)diff->address,
           (unsigned)diff->read,
           (unsigned)diff->expected);
}

static void print_status(pic24_icsp_status_t status) {
    printf("%s\r\n", status == PIC24_ICSP_ERR_WRERR ? "failed (WRERR)" : "failed (WR did not clear)");
}

// One attempt at one PGC speed, from ICSP entry to the end of the verify
static bp3_result_t flash_once(bool* confirmed, bool* erased) {
    pic24_icsp_enter();
    if (pic24_icsp_visi_echo(0xA55Au) != 0xA55Au || pic24_icsp_visi_echo(0x5AA5u) != 0x5AA5u) {
        print_error("the PIC does not answer over ICSP\r\n");
        return BP3_NO_ANSWER;
    }
    uint32_t id[2];
    pic24_icsp_read(PIC24_ICSP_DEVID_ADDR, id, 2);
    uint16_t devid = (uint16_t)id[0];
    uint16_t devrev = (uint16_t)id[1];
    if (devid != BP3_DEVID) {
        printf("%sError:%s DEVID 0x%04X: this is not the PIC24FJ64GA002 of a Bus Pirate v3 (0x%04X)\r\n",
               ui_term_color_error(),
               ui_term_color_reset(),
               devid,
               BP3_DEVID);
        return BP3_WRONG_DEVICE;
    }
    printf("PIC: PIC24FJ64GA002 rev %s (DEVID 0x%04X, DEVREV 0x%04X)\r\n", devrev_name(devrev), devid, devrev);

    if (!*confirmed) {
        printf("\r\nThis erases the Bus Pirate v3 flash and writes bootloader v4.5 alone: it has\r\n"
               "no firmware afterwards, until pirate-loader loads one.\r\n");
        drop_typed_input();
        if (!bp_cmd_confirm(NULL, "Continue?")) {
            return BP3_CANCELLED;
        }
        *confirmed = true;
        write_start = get_absolute_time();
    }

    *erased = true; // from here on a failure leaves a flash that is not complete
    printf("Erasing... ");
    pic24_icsp_status_t status = pic24_icsp_erase_user(BP3_ERASE_TIMEOUT_MS);
    if (status != PIC24_ICSP_OK) {
        print_status(status);
        return BP3_ERASE_FAILED;
    }
    printf("OK\r\n");

    bp3_diff_t diff = { 0 };
    for (uint8_t r = 0; r < BP3UNBRICK_BOOTLOADER_ROWS; r++) {
        compare_row(bp3unbrick_bootloader[r].address, NULL, &diff);
    }
    if (diff.differ) {
        print_diff("not blank after the erase", &diff);
        return BP3_NOT_BLANK;
    }

    printf("Writing %d rows... ", BP3UNBRICK_BOOTLOADER_ROWS);
    for (uint8_t r = 0; r < BP3UNBRICK_BOOTLOADER_ROWS; r++) {
        status =
            pic24_icsp_write_row(bp3unbrick_bootloader[r].address, bp3unbrick_bootloader[r].words, BP3_ROW_TIMEOUT_MS);
        if (status != PIC24_ICSP_OK) {
            print_status(status);
            return BP3_WRITE_FAILED;
        }
    }
    printf("OK\r\n");

    printf("Verifying %u words\r\n", (unsigned)(BP3_ROWS * PIC24_ICSP_ROW_WORDS));
    ui_term_progress_bar_t progress;
    ui_term_progress_bar_draw(&progress);
    for (uint32_t r = 0; r < BP3_ROWS; r++) {
        uint32_t address = r * BP3_ROW_SPAN;
        compare_row(address, bootloader_row(address), &diff);
        ui_term_progress_bar_update(r + 1, BP3_ROWS, &progress);
    }
    ui_term_progress_bar_cleanup(&progress);
    if (diff.differ) {
        print_diff("verify failed", &diff);
        return BP3_VERIFY_FAILED;
    }
    return BP3_OK;
}

// Checks, then attempts at each speed until one works or a retry cannot help
static bp3_result_t flash_bootloader(bool* confirmed, bool* erased) {
    if (!target_powered(true)) {
        return BP3_NO_TARGET;
    }
    if (!lines_follow()) {
        return BP3_LINE_FAULT;
    }
    bp3_result_t result = BP3_NO_ANSWER;
    for (uint8_t s = 0; s < count_of(phase_ns); s++) {
        if (s) {
            if (!target_powered(false)) { // never drive the lines of a Bus Pirate v3 that lost its power
                return BP3_NO_TARGET;
            }
            printf("Trying again with a slower clock, %u ns per PGC phase\r\n", (unsigned)phase_ns[s]);
        }
        pic24_icsp_init(BP3_PGC, BP3_PGD, BP3_MCLR, phase_ns[s]);
        result = flash_once(confirmed, erased);
        pic24_icsp_exit(); // MCLR low: the PIC stays in reset
        if (result == BP3_OK || result == BP3_CANCELLED || result == BP3_WRONG_DEVICE) {
            break;
        }
    }
    if (result == BP3_NO_ANSWER) {
        printf("Check PGC (JP2 pin 1 to IO7) and PGD (JP2 pin 2 to IO6):\r\n"
               "are they swapped, loose or too long?\r\n");
    }
    return result;
}

// MCLR low for 1 ms, then released to the Bus Pirate v3's pull-up
static void reset_target(void) {
    bio_put(BP3_MCLR, false);
    bio_output(BP3_MCLR);
    busy_wait_ms(1);
    bio_input(BP3_MCLR);
}

static void wait_key_or_button(void) {
    char c;
    drop_typed_input();
    while (!rx_fifo_try_get(&c) && !button_get(0)) {
        busy_wait_ms(10);
    }
    while (button_get(0)) {
        busy_wait_ms(10);
    }
}

// The bootloader stays in bootloader mode while PGC reads low at reset, as with the PGC-PGD jumper
static void hold_for_pirate_loader(void) {
    printf("The Bus Pirate v3 has no firmware yet: load one with pirate-loader, over its\r\n"
           "own USB port.\r\n");
    drop_typed_input();
    bp_yn_result_t answer = bp_cmd_yes_no_exit("\r\nHold the Bus Pirate v3 in its bootloader for pirate-loader now?\r\n"
                                               "This Bus Pirate keeps PGC low, as the PGC-PGD jumper does.");
    if (answer != BP_YN_YES) {
        printf("Later: put a jumper between PGC and PGD (JP2 pins 1 and 2), then replug the\r\n"
               "Bus Pirate v3.\r\n");
        return;
    }
    bio_put(BP3_PGC, false);
    bio_output(BP3_PGC);
    reset_target();
    printf("The Bus Pirate v3 waits in its bootloader, with its MODE LED on.\r\n"
           "On its serial port, run:\r\n\r\n"
           "  pirate-loader --dev=PORT --hex=FIRMWARE.hex\r\n\r\n"
           "With a v7.1 file pirate-loader ends with ERROR [50] at page 42: that is\r\n"
           "expected, the firmware is complete. Press any key or the button when\r\n"
           "pirate-loader is done.\r\n");
    wait_key_or_button();
    bio_input(BP3_PGC);
    reset_target();
    printf("PGC released and the Bus Pirate v3 reset: it starts its firmware, if it has one.\r\n");
}

static bool write_bootloader(void) {
    bool confirmed = false;
    bool erased = false; // once erased, the flash is incomplete until an attempt succeeds
    while (true) {
        bp3_result_t result = flash_bootloader(&confirmed, &erased);
        if (result == BP3_OK) {
            return true;
        }
        if (result == BP3_CANCELLED) {
            printf("Nothing was written.\r\n");
            return false;
        }
        bio_input(BP3_PGD);
        bio_input(BP3_PGC);
        if (erased) {
            bio_put(BP3_MCLR, false);
            bio_output(BP3_MCLR);
            printf("The Bus Pirate v3 flash is incomplete: it stays in reset while you choose.\r\n");
        } else {
            bio_input(BP3_MCLR);
            printf("Nothing was written.\r\n");
        }
        drop_typed_input();
        if (bp_cmd_yes_no_exit("\r\nTry again?") != BP_YN_YES) {
            if (erased) {
                printf("Select this binmode again before using the Bus Pirate v3.\r\n");
            }
            return false;
        }
    }
}

// One job per selection: give the binary port back to the default binmode
static void leave_binmode(void) {
    binmode_cleanup();
    system_config.binmode_select = 0;
    binmode_load_save_config(false);
    if (system_config.binmode_select >= BINMODE_MAXPROTO) {
        system_config.binmode_select = 0;
    }
    binmode_setup();
    printf("\r\n%sActive binmode:%s %s\r\n",
           ui_term_color_info(),
           ui_term_color_reset(),
           binmodes[system_config.binmode_select].binmode_name);
}

void bp3unbrick_setup(void) {
    pins_claimed = false;
    psu_ours = false;
    psu_feeds_vout = false;
}

void bp3unbrick_run(void) {
    print_wiring();
    if (pins_free()) {
        claim_pins();
    }
    if (pins_claimed && io_reference_or_retry()) {
        lcd_redraw();
        system_config.binmode_suppress_monitor = true; // the voltage sweep would load the lines
        if (write_bootloader()) {
            // PGD, PGC, then MCLR: the bootloader starts with PGC released and finds no firmware
            bio_input(BP3_PGD);
            bio_input(BP3_PGC);
            bio_input(BP3_MCLR);
            uint32_t ms = (uint32_t)(absolute_time_diff_us(write_start, get_absolute_time()) / 1000);
            printf("\r\n%sBootloader v4.5 written and verified%s in %u.%us: %d words, all others erased.\r\n",
                   ui_term_color_info(),
                   ui_term_color_reset(),
                   (unsigned)(ms / 1000u),
                   (unsigned)((ms % 1000u) / 100u),
                   BP3UNBRICK_BOOTLOADER_WORDS);
            hold_for_pirate_loader();
        }
    }
    leave_binmode();
}

void bp3unbrick_cleanup(void) {
    bool redraw = pins_claimed || psu_ours;
    if (pins_claimed) {
        bio_input(BP3_PGD);
        bio_input(BP3_PGC);
        bio_input(BP3_MCLR);
        system_bio_update_purpose_and_label(false, BP3_PGC, BP_PIN_MODE, 0);
        system_bio_update_purpose_and_label(false, BP3_PGD, BP_PIN_MODE, 0);
        system_bio_update_purpose_and_label(false, BP3_MCLR, BP_PIN_MODE, 0);
        pins_claimed = false;
    }
    if (psu_ours) {
        psucmd_disable();
        psu_ours = false;
    }
    psu_feeds_vout = false;
    system_config.binmode_suppress_monitor = false;
    if (redraw) {
        lcd_redraw();
    }
}
