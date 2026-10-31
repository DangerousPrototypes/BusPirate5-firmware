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
#include <stdlib.h>
#include <string.h>
#include "pico/stdlib.h"
#include "pirate.h"
// #include "hardware/gpio.h"
// #include "hardware/pio.h"
// #include "pico/binary_info.h"
#include "hardware/clocks.h"
#include "hw1wire.pio.h"
#include "pirate/hw1wire_pio.h"

struct owobj owobj;

void onewire_init(uint pin, uint dir) {
    // owobj.pio = pio;
    // owobj.sm = sm;
    owobj.pin = pin;
    owobj.dir = dir;
    // bool success = pio_claim_free_sm_and_add_program_for_gpio_range(&onewire_program, &owobj.pio, &owobj.sm,
    // &owobj.offset, dir, 9, true); hard_assert(success);
    owobj.pio = PIO_MODE_PIO;
    owobj.sm = 0;
    owobj.offset = pio_add_program(owobj.pio, &onewire_program);
#ifdef BP_PIO_SHOW_ASSIGNMENT
    printf("pio %d, sm %d, offset %d\n", PIO_NUM(owobj.pio), owobj.sm, owobj.offset);
#endif
    // gpio_set_function(owobj.dir, GPIO_FUNC_PIO1);
    onewire_program_init(owobj.pio, owobj.sm, owobj.offset, owobj.pin, owobj.dir);
    onewire_set_fifo_thresh(8);
    pio_sm_set_enabled(owobj.pio, owobj.sm, true);
}

void onewire_cleanup(void) {
    // pio_remove_program_and_unclaim_sm(&onewire_program, owobj.pio, owobj.sm, owobj.offset);
    pio_sm_set_enabled(owobj.pio, owobj.sm, false);
    pio_remove_program(owobj.pio, &onewire_program, owobj.offset);
}

void onewire_set_fifo_thresh(uint thresh) {
    PIO pio = owobj.pio;
    uint sm = owobj.sm;
    uint offset = owobj.offset;
    uint old, new;
    uint8_t waiting_addr;
    uint need_restart;
    int timeout;

    if (thresh >= 32) {
        thresh = 0;
    }

    old = pio->sm[sm].shiftctrl;
    old &= PIO_SM0_SHIFTCTRL_PUSH_THRESH_BITS | PIO_SM0_SHIFTCTRL_PULL_THRESH_BITS;

    new = ((thresh & 0x1fu) << PIO_SM0_SHIFTCTRL_PUSH_THRESH_LSB) |
          ((thresh & 0x1fu) << PIO_SM0_SHIFTCTRL_PULL_THRESH_LSB);

    if (old != new) {
        need_restart = 0;
        if (pio->ctrl & (1u << sm)) {
            /* If state machine is enabled, it must be disabled
               and restarted when we change fifo thresholds,
               or evil things happen */

            /* When we attempt fifo threshold switching, we assume
               that all fifo operations have been done and hence
               all bits have been almost processed, but the
               state machine might not have reached the wating state
               as it still does some delays to ensure timing for
               the very last bit (Similar for reset).
               Just wait for the 'wating' state to be reached */
            waiting_addr = offset + onewire_offset_waiting;
            timeout = 2000;
            while (pio_sm_get_pc(pio, sm) != waiting_addr) {
                sleep_us(1);
                if (timeout-- < 0) {
                    /* FIXME: do something clever in case of
                       timeout */
                }
            }

            pio_sm_set_enabled(pio, sm, false);
            need_restart = 1;
        }

        hw_write_masked(
            &pio->sm[sm].shiftctrl, new, PIO_SM0_SHIFTCTRL_PUSH_THRESH_BITS | PIO_SM0_SHIFTCTRL_PULL_THRESH_BITS);

        if (need_restart) {
            pio_sm_restart(pio, sm);
            pio_sm_set_enabled(pio, sm, true);
        }
    }
}

int onewire_reset(void) {
    int ret;
    PIO pio = owobj.pio;
    uint sm = owobj.sm;
    uint offset = owobj.offset;
    uint8_t waiting_addr;
    int timeout;
    uint div;

    /* Switch to slow timing for reset */
    div = clock_get_hz(clk_sys) / 1000000 * 70;
    pio_sm_set_clkdiv_int_frac(pio, sm, div, 0);
    pio_sm_clkdiv_restart(pio, sm);
    onewire_set_fifo_thresh(1);

    // onewire_do_reset(pio, sm, offset);
    pio_sm_exec(pio, sm, pio_encode_jmp(offset + onewire_offset_reset));
    while (pio_sm_get_rx_fifo_level(pio, sm) == 0)
        /* wait */;
    ret = ((pio_sm_get(pio, sm) & 0x80000000) == 0);

    /* when rx fifo has filled we still need to wait for
       the remaineder of the reset to execute before we
       can manipulate the clkdiv.
       Just wait until we reach the waiting state */
    waiting_addr = offset + onewire_offset_waiting;
    timeout = 2000;
    while (pio_sm_get_pc(pio, sm) != waiting_addr) {
        sleep_us(1);
        if (timeout-- < 0) {
            /* FIXME: do something clever in case of
               timeout */
        }
    }

    /* Restore normal timing */
    div = clock_get_hz(clk_sys) / 1000000 * 3;
    pio_sm_set_clkdiv_int_frac(pio, sm, div, 0);
    pio_sm_clkdiv_restart(pio, sm);

    return ret; // 1=detected, 0=not
}

/* Wait for idle state to be reached. This is only
   useful when you know that all but the last bit
   have been processe (after having checked fifos) */
void onewire_wait_for_idle(void) {
    PIO pio = owobj.pio;
    uint sm = owobj.sm;
    uint offset = owobj.offset;
    uint8_t waiting_addr;
    int timeout;

    /* when rx fifo has filled the bit timing has not
       fully completed.
       Just wait until we reach the waiting state */
    waiting_addr = offset + onewire_offset_waiting;
    timeout = 2000;
    while (pio_sm_get_pc(pio, sm) != waiting_addr) {
        sleep_us(1);
        if (timeout-- < 0) {
            /* FIXME: do something clever in case of
               timeout */
        }
    }

    return;
}

/* Transmit a byte */
void onewire_tx_byte(uint byte) {
    PIO pio = owobj.pio;
    uint sm = owobj.sm;

    onewire_set_fifo_thresh(8);
    pio->txf[sm] = byte;
    while (pio_sm_get_rx_fifo_level(pio, sm) == 0)
        /* wait */;
    pio_sm_get(pio, sm); /* read to drain RX fifo */
    return;
}

/* Receive a byte */
uint onewire_rx_byte(void) {
    PIO pio = owobj.pio;
    uint sm = owobj.sm;

    onewire_set_fifo_thresh(8);
    pio->txf[sm] = 0xff;
    while (pio_sm_get_rx_fifo_level(pio, sm) == 0)
        /* wait */;
    /* Returned byte is in 31..24 of RX fifo! */
    return (pio_sm_get(pio, sm) >> 24) & 0xff;
}

/* Do a ROM search triplet.
   Receive two bits and store the read values to
   id_bit and cmp_id_bit respectively.
   Then transmit a bit with this logic:
     id_bit | cmp_id_bit | tx-bit
          0 |          1 |      0
          1 |          0 |      1
          0 |          0 | search_direction
          1 |          1 |      1
    The actually transmitted bit is returned via search_direction.

    Refer to MAXIM APPLICATION NOTE 187 "1-Wire Search Algorithm"
 */
void onewire_triplet(int* id_bit, int* cmp_id_bit, unsigned char* search_direction) {
    PIO pio = owobj.pio;
    uint sm = owobj.sm;
    uint fiforx;

    onewire_set_fifo_thresh(2);
    pio->txf[sm] = 0x3;
    while (pio_sm_get_rx_fifo_level(pio, sm) == 0)
        /* wait */;

    fiforx = pio_sm_get(pio, sm);
    *id_bit = (fiforx >> 30) & 1;
    *cmp_id_bit = (fiforx >> 31) & 1;
    if ((*id_bit == 0) && (*cmp_id_bit == 1)) {
        *search_direction = 0;
    } else if ((*id_bit == 1) && (*cmp_id_bit == 0)) {
        *search_direction = 1;
    } else if ((*id_bit == 0) && (*cmp_id_bit == 0)) {
        /* do not change search direction */
    } else {
        *search_direction = 1;
    }

    onewire_set_fifo_thresh(1);
    pio->txf[sm] = *search_direction;
    while (pio_sm_get_rx_fifo_level(pio, sm) == 0)
        /* wait */;
    fiforx = pio_sm_get(pio, sm);
    return;
}

unsigned char calc_crc8_buf(unsigned char* data, int len) {
    int i, j;
    unsigned char crc8;

    // See Application Note 27
    crc8 = 0;
    for (j = 0; j < len; j++) {
        crc8 = crc8 ^ data[j];
        for (i = 0; i < 8; ++i) {
            if (crc8 & 1) {
                crc8 = (crc8 >> 1) ^ 0x8c;
            } else {
                crc8 = (crc8 >> 1);
            }
        }
    }

    return crc8;
}

/* Select a device by ROM ID */
int onewire_select(unsigned char* romid) {
    int i;

    if (!onewire_reset()) {
        return 0;
    }
    onewire_tx_byte(0x55); // Match ROM command
    for (i = 0; i < 8; i++) {
        onewire_tx_byte(romid[i]);
    }
    return 1;
}

/* This is code stolen from MAXIM AN3684, slightly modified
   to interface to the PIO onewire and to eliminate global
   variables. */

#define TRUE 1
#define FALSE 0

typedef unsigned char byte;

//--------------------------------------------------------------------------
// Calculate the CRC8 of the byte value provided with the current
// global 'crc8' value.
// Returns current global crc8 value
//
unsigned char calc_crc8(unsigned char data) {
    int i;

    // See Application Note 27
    owobj.crc8 = owobj.crc8 ^ data;
    for (i = 0; i < 8; ++i) {
        if (owobj.crc8 & 1) {
            owobj.crc8 = (owobj.crc8 >> 1) ^ 0x8c;
        } else {
            owobj.crc8 = (owobj.crc8 >> 1);
        }
    }

    return owobj.crc8;
}

//--------------------------------------------------------------------------
// The 'OWSearch' function does a general search.  This function
// continues from the previous search state. The search state
// can be reset by using the 'OWFirst' function.
//
// Returns:   TRUE (1) : when a 1-Wire device was found and its
//                       Serial Number placed in the global ROM
//            FALSE (0): when no new device was found.  Either the
//                       last search was the last device or there
//                       are no devices on the 1-Wire Net.
//
int OWSearch(struct owobj* search_owobj) {
    int id_bit_number;
    int last_zero;
    int rom_byte_number;
    int search_result;
    int id_bit;
    int cmp_id_bit;
    unsigned char rom_byte_mask;
    unsigned char search_direction;

    // initialize for search
    id_bit_number = 1;
    last_zero = 0;
    rom_byte_number = 0;
    rom_byte_mask = 1;
    search_result = FALSE;
    search_owobj->crc8 = 0;

    // if the last call was not the last one
    if (!search_owobj->LastDeviceFlag) {
        // 1-Wire reset
        if (!onewire_reset()) {
            // reset the search
            search_owobj->LastDiscrepancy = 0;
            search_owobj->LastDeviceFlag = FALSE;
            search_owobj->LastFamilyDiscrepancy = 0;
            return FALSE;
        }

        // issue the search command
        onewire_tx_byte(0xf0);

        // loop to do the search
        do {
            // if this discrepancy if before the Last Discrepancy
            // on a previous next then pick the same as last time
            if (id_bit_number < search_owobj->LastDiscrepancy) {
                if ((search_owobj->ROM_NO[rom_byte_number] & rom_byte_mask) > 0) {
                    search_direction = 1;
                } else {
                    search_direction = 0;
                }
            } else {
                // if equal to last pick 1, if not then pick 0
                if (id_bit_number == search_owobj->LastDiscrepancy) {
                    search_direction = 1;
                } else {
                    search_direction = 0;
                }
            }

            // Perform a triple operation on the DS2482 which will perform 2 read bits and 1 write bit
            onewire_triplet(&id_bit, &cmp_id_bit, &search_direction);

            // check for no devices on 1-Wire
            if ((id_bit) && (cmp_id_bit)) {
                break;
            } else {
                if ((!id_bit) && (!cmp_id_bit) && (search_direction == 0)) {
                    last_zero = id_bit_number;

                    // check for Last discrepancy in family
                    if (last_zero < 9) {
                        search_owobj->LastFamilyDiscrepancy = last_zero;
                    }
                }

                // set or clear the bit in the ROM byte rom_byte_number
                // with mask rom_byte_mask
                if (search_direction == 1) {
                    search_owobj->ROM_NO[rom_byte_number] |= rom_byte_mask;
                } else {
                    search_owobj->ROM_NO[rom_byte_number] &= (byte)~rom_byte_mask;
                }

                // increment the byte counter id_bit_number
                // and shift the mask rom_byte_mask
                id_bit_number++;
                rom_byte_mask <<= 1;

                // if the mask is 0 then go to new SerialNum byte rom_byte_number and reset mask
                if (rom_byte_mask == 0) {
                    calc_crc8(search_owobj->ROM_NO[rom_byte_number]); // accumulate the CRC
                    rom_byte_number++;
                    rom_byte_mask = 1;
                }
            }
        } while (rom_byte_number < 8); // loop until through all ROM bytes 0-7

        // if the search was successful then
        if (!((id_bit_number < 65) || (search_owobj->crc8 != 0))) {
            // search successful so set LastDiscrepancy,LastDeviceFlag,search_result
            search_owobj->LastDiscrepancy = last_zero;

            // check for last device
            if (search_owobj->LastDiscrepancy == 0) {
                search_owobj->LastDeviceFlag = TRUE;
            }

            search_result = TRUE;
        }
    }

    // if no device found then reset counters so next 'search' will be like a first
    if (!search_result || (search_owobj->ROM_NO[0] == 0)) {
        search_owobj->LastDiscrepancy = 0;
        search_owobj->LastDeviceFlag = FALSE;
        search_owobj->LastFamilyDiscrepancy = 0;
        search_result = FALSE;
    }

    return search_result;
}

int OWSearchReset(struct owobj* search_owobj) {
    // reset the search state
    search_owobj->LastDiscrepancy = 0;
    search_owobj->LastDeviceFlag = FALSE;
    search_owobj->LastFamilyDiscrepancy = 0;
    return 0;
}

//--------------------------------------------------------------------------
// Find the 'first' devices on the 1-Wire network
// Return TRUE  : device found, ROM number in ROM_NO buffer
//        FALSE : no device present
//
int OWFirst(struct owobj* search_owobj) {
    // reset the search state
    search_owobj->LastDiscrepancy = 0;
    search_owobj->LastDeviceFlag = FALSE;
    search_owobj->LastFamilyDiscrepancy = 0;
    return OWSearch(search_owobj);
}

//--------------------------------------------------------------------------
// Find the 'next' devices on the 1-Wire network
// Return TRUE  : device found, ROM number in ROM_NO buffer
//        FALSE : device not found, end of search
//
int OWNext(struct owobj* search_owobj) {
    // leave the search state alone
    return OWSearch(search_owobj);
}

/* End of MAXIM AN3684 code */

