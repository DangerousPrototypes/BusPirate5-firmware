/**
 * Copyright (c) 2023 mjcross
 * Modified 2024 by Ian Lesnet for Bus Pirate 5+
 *
 * SPDX-License-Identifier: BSD-3-Clause
 **/
#include <stdio.h>
#include <stdlib.h>
#include "pico/stdlib.h"
#include "hardware/gpio.h"
#include "hardware/pio.h"
#include "pirate.h"
#include "pio_config.h"
#include "hardware/clocks.h"
#include "onewire_library.pio.h"
#include "pirate/onewire_library.h"

OW ow;

// Create a driver instance and populate the provided OW structure.
// Returns: True on success.
// ow: A pointer to a blank OW structure to hold the driver parameters.
// pio: The PIO hardware instance to use.
// offset: The location of the onewire program in the PIO shared address space.
// gpio: The pin to use for the bus (NB: see the README).
bool ow_init(uint8_t bits_per_word, uint bufdir, uint inpin) {
    ow.gpio = bufdir;
    ow.pio = PIO_MODE_PIO;
    ow.sm = 0;

    ow.offset = pio_add_program(ow.pio, &onewire_program);
    ow.jmp_reset = onewire_reset_instr(ow.offset); // assemble the bus reset instruction

    gpio_init(inpin); // enable the gpio and clear any output value
    gpio_init(bufdir);
    pio_gpio_init(ow.pio, bufdir);                                           // set the function to PIO output
    onewire_sm_init(ow.pio, ow.sm, ow.offset, bits_per_word, bufdir, inpin); // set 8 bits per word
    return true;
}

// cleanup PIO when done
void ow_cleanup(void) {
    pio_remove_program(ow.pio, &onewire_program, ow.offset);
}

// Send a binary word on the bus (LSB first).
// ow: A pointer to an OW driver struct.
// data: The word to be sent.
void ow_send(uint data) {
    pio_sm_put_blocking(ow.pio, ow.sm, (uint32_t)data);
    pio_sm_get_blocking(ow.pio, ow.sm); // discard the response
}

// Read a binary word from the bus.
// Returns: the word read (LSB first).
// ow: pointer to an OW driver struct
uint8_t ow_read(void) {
    pio_sm_put_blocking(ow.pio, ow.sm, 0xff);                   // generate read slots
    return (uint8_t)(pio_sm_get_blocking(ow.pio, ow.sm) >> 24); // shift response into bits 0..7
}

// Reset the bus and detect any connected slaves.
// Returns: true if any slaves responded.
// ow: pointer to an OW driver struct
bool ow_reset(void) {
    pio_sm_exec_wait_blocking(ow.pio, ow.sm, ow.jmp_reset);
    if ((pio_sm_get_blocking(ow.pio, ow.sm) & 1) == 0) { // apply pin mask (see pio program)
        return true;                                     // a slave pulled the bus low
    }
    return false;
}

// Find ROM codes (64-bit hardware addresses) of all connected devices.
// See https://www.analog.com/en/app-notes/1wire-search-algorithm.html
// Returns: the number of devices found (up to maxdevs) or -1 if an error occurrred.
// ow: pointer to an OW driver struct
// romcodes: location at which store the addresses (NULL means don't store)
// maxdevs: maximum number of devices to find (0 means no limit)
// command: 1-Wire search command (e.g. OW_SEARCHROM or OW_ALARM_SEARCH)
/*
int ow_romsearch (uint64_t *romcodes, int maxdevs, uint command) {
    int index;
    uint64_t romcode = 0ull;
    int branch_point;
    int next_branch_point = -1;
    int num_found = 0;
    bool finished = false;

    onewire_sm_init (ow.pio, ow.sm, ow.offset, ow.gpio, 1); // set driver to 1-bit mode

    while (finished == false && (maxdevs == 0 || num_found < maxdevs )) {
        finished = true;
        branch_point = next_branch_point;
        if (ow_reset (ow) == false) {
            num_found = 0;     // no slaves present
            finished = true;
            break;
        }
        for (int i = 0; i < 8; i += 1) {    // send search command as single bits
            ow_send (ow, command >> i);
        }
        for (index = 0; index < 64; index += 1) {   // determine romcode bits 0..63 (see ref)
            uint a = ow_read (ow);
            uint b = ow_read (ow);
            if (a == 0 && b == 0) {         // (a, b) = (0, 0)
                if (index == branch_point) {
                    ow_send (ow, 1);
                    romcode |= (1ull << index);
                } else {
                    if (index > branch_point || (romcode & (1ull << index)) == 0) {
                        ow_send(ow, 0);
                        finished = false;
                        romcode &= ~(1ull << index);
                        next_branch_point = index;
                    } else {                // index < branch_point or romcode[index] = 1
                        ow_send (ow, 1);
                    }
                }
            } else if (a != 0 && b != 0) {  // (a, b) = (1, 1) error (e.g. device disconnected)
                num_found = -2;             // function will return -1
                finished = true;
                break;                      // terminate for loop
            } else {
                if (a == 0) {               // (a, b) = (0, 1) or (1, 0)
                    ow_send (ow, 0);
                    romcode &= ~(1ull << index);
                } else {
                    ow_send (ow, 1);
                    romcode |= (1ull << index);
                }
            }
        }                                   // end of for loop

        if (romcodes != NULL) {
            romcodes[num_found] = romcode;  // store the romcode
        }
        num_found += 1;
    }                                       // end of while loop

    onewire_sm_init (ow.pio, ow.sm, ow.offset, ow.gpio, 8); // restore 8-bit mode
    return num_found;
}
*/