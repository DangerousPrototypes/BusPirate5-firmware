/*
 * Copyright (c) 2013-2017 ARM Limited. All rights reserved.
 * Copyright (c) 2022 Raspberry Pi Ltd
 * Copyright (c) 2026 Daniel Schaefer
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 * Licensed under the Apache License, Version 2.0 (the License); you may
 * not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 * www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an AS IS BASIS, WITHOUT
 * WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *
 * ----------------------------------------------------------------------
 *
 * SWD through PIO for the CMSIS-DAP binmode
 *
 * SWJ_Sequence, SWD_Sequence and SWD_Transfer follow ARM's reference SW_DP.c
 * (also in this directory) and, for driving them through PIO, the Raspberry Pi
 * debugprobe's src/sw_dp_pio.c (https://github.com/raspberrypi/debugprobe).
 *
 * Modified by Daniel Schaefer 2026: written for the Bus Pirate PIO program
 * (dap_swd.pio) with the SWDIO level shifter, queued commands, PIO setup and
 * teardown, DAP_SWJ_Pins support, bit-banged fallback.
 *
 * In SWD mode a PIO state machine clocks SWCLK and shifts SWDIO (dap_swd.pio),
 * the CPU only queues commands and collects read data. JTAG and SWD without a
 * free PIO state machine use the bit-banged code in SW_DP.c/JTAG_DP.c.
 */

#include "pico/stdlib.h"
#include "hardware/pio.h"
#include "hardware/clocks.h"
#include "DAP_config.h"
#include "DAP.h"
#include "dap_swd.pio.h"

#define SWD_PIO PIO_MODE_PIO
#define PIN_SWCLK DAP_TCK_IO
#define PIN_SWDIO DAP_TMS_IO
#define PIN_SWDIO_DIR DAP_TMS_DIR
#define PIN_MASK ((1u << PIN_SWCLK) | (1u << PIN_SWDIO) | (1u << PIN_SWDIO_DIR))

bool dap_swd_pio_active;

static int swd_sm = -1;
static int swd_offset = -1;
static uint32_t swd_clock = DAP_DEFAULT_SWJ_CLOCK;

#define CMD(routine, bits) (((uint32_t)(bits) - 1u) | ((uint32_t)(swd_offset + dap_swd_offset_##routine) << 8))
#define CMD_DIR_OUT (CMD(dir_out_cmd, 1) | (1u << 13))
#define CMD_DIR_IN CMD(dir_in_cmd, 1)

static inline void swd_put(uint32_t word) {
    while (pio_sm_is_tx_fifo_full(SWD_PIO, swd_sm)) {
        tight_loop_contents();
    }
    SWD_PIO->txf[swd_sm] = word;
}

static inline uint32_t swd_get(void) {
    while (pio_sm_is_rx_fifo_empty(SWD_PIO, swd_sm)) {
        tight_loop_contents();
    }
    return SWD_PIO->rxf[swd_sm];
}

static inline uint32_t parity32(uint32_t v) {
    v ^= v >> 16;
    v ^= v >> 8;
    v ^= v >> 4;
    v ^= v >> 2;
    v ^= v >> 1;
    return v & 1u;
}

// 1 to 256 bits, data beyond 32 bits is 0
static inline void swd_write(uint32_t bits, uint32_t data) {
    swd_put(CMD(write_cmd, bits));
    swd_put(data);
}

// 1 to 32 bits, collect with swd_read_result()
static inline void swd_read(uint32_t bits) {
    swd_put(CMD(read_cmd, bits));
}

static inline uint32_t swd_read_result(uint32_t bits) {
    return swd_get() >> (32u - bits);
}

// Wait until every queued bit is on the wire. Read results nobody collected are
// dropped, otherwise a full RX FIFO would block the push and TXSTALL never sets.
static void swd_flush(void) {
    uint32_t stall = 1u << (PIO_FDEBUG_TXSTALL_LSB + swd_sm);
    SWD_PIO->fdebug = stall;
    while (!(SWD_PIO->fdebug & stall)) {
        if (!pio_sm_is_rx_fifo_empty(SWD_PIO, swd_sm)) {
            (void)SWD_PIO->rxf[swd_sm];
        }
    }
}

static void swd_apply_clock(void) {
    // round the divider up so SWCLK never exceeds the requested clock
    uint64_t div256 = ((uint64_t)clock_get_hz(clk_sys) * 256u + 4u * swd_clock - 1u) / (4u * swd_clock);
    if (div256 < 256u) {
        div256 = 256u;
    } else if (div256 > 0xffffffu) {
        div256 = 0xffffffu;
    }
    pio_sm_set_clkdiv_int_frac(SWD_PIO, swd_sm, (uint16_t)(div256 >> 8), (uint8_t)div256);
}

void dap_swd_pio_set_clock(uint32_t hz) {
    swd_clock = hz ? hz : 1u;
    if (dap_swd_pio_active) {
        swd_flush();
        swd_apply_clock();
    }
}

void dap_swd_pio_start(void) {
    if (dap_swd_pio_active) {
        return;
    }
    if (swd_offset < 0) {
        if (!pio_can_add_program(SWD_PIO, &dap_swd_program)) {
            return;
        }
        swd_sm = pio_claim_unused_sm(SWD_PIO, false);
        if (swd_sm < 0) {
            return;
        }
        swd_offset = pio_add_program(SWD_PIO, &dap_swd_program);
    }

    pio_sm_config c = dap_swd_program_get_default_config(swd_offset);
    sm_config_set_out_pins(&c, PIN_SWDIO, 1);
    sm_config_set_in_pins(&c, PIN_SWDIO);
    sm_config_set_set_pins(&c, PIN_SWDIO_DIR, 1);
    sm_config_set_sideset_pins(&c, PIN_SWCLK);
    sm_config_set_out_shift(&c, true, false, 32);
    sm_config_set_in_shift(&c, true, false, 32);
    pio_sm_init(SWD_PIO, swd_sm, swd_offset + dap_swd_offset_get_next_cmd, &c);
    swd_apply_clock();

    // same state as PORT_SWD_SETUP left the SIO in, except SWCLK idles low
    pio_sm_set_pins_with_mask(SWD_PIO, swd_sm, (1u << PIN_SWDIO) | (1u << PIN_SWDIO_DIR), PIN_MASK);
    pio_sm_set_pindirs_with_mask(SWD_PIO, swd_sm, PIN_MASK, PIN_MASK);
    pio_gpio_init(SWD_PIO, PIN_SWDIO_DIR);
    pio_gpio_init(SWD_PIO, PIN_SWDIO);
    pio_gpio_init(SWD_PIO, PIN_SWCLK);
    pio_sm_set_enabled(SWD_PIO, swd_sm, true);
    dap_swd_pio_active = true;
}

// Give SWCLK/SWDIO back to the SIO, SWDIO released
void dap_swd_pio_stop(void) {
    if (!dap_swd_pio_active) {
        return;
    }
    swd_flush();
    pio_sm_set_enabled(SWD_PIO, swd_sm, false);
    pio_sm_clear_fifos(SWD_PIO, swd_sm);

    gpio_set_dir(PIN_SWDIO, GPIO_IN);
    gpio_put(PIN_SWDIO_DIR, 0);
    gpio_put(PIN_SWCLK, 0);
    gpio_set_dir(PIN_SWCLK, GPIO_OUT);
    gpio_set_dir(PIN_SWDIO_DIR, GPIO_OUT);
    // SWDIO pin first: it must not drive while the PIO still has the shifter turned towards it
    gpio_set_function(PIN_SWDIO, GPIO_FUNC_SIO);
    gpio_set_function(PIN_SWDIO_DIR, GPIO_FUNC_SIO);
    gpio_set_function(PIN_SWCLK, GPIO_FUNC_SIO);
    dap_swd_pio_active = false;
}

// Stop and free the state machine and program memory
void dap_swd_pio_release(void) {
    dap_swd_pio_stop();
    if (swd_offset >= 0) {
        pio_remove_program(SWD_PIO, &dap_swd_program, swd_offset);
        pio_sm_unclaim(SWD_PIO, swd_sm);
        swd_offset = -1;
        swd_sm = -1;
    }
}

void dap_swd_pio_swdio_dir(uint32_t out) {
    swd_put(out ? CMD_DIR_OUT : CMD_DIR_IN);
}

// DAP_SWJ_Pins on SWCLK/SWDIO while the PIO owns them
void dap_swd_pio_set_pin(uint32_t pin, uint32_t value) {
    swd_flush();
    // pio_sm_set_pins_with_mask() must not run on an enabled state machine. Re-enabling
    // at get_next_cmd would re-run its "pull side 0" and drop SWCLK, so wait in hold_pins.
    pio_sm_set_enabled(SWD_PIO, swd_sm, false);
    pio_sm_set_pins_with_mask(SWD_PIO, swd_sm, value ? (1u << pin) : 0u, 1u << pin);
    pio_sm_exec(SWD_PIO, swd_sm, pio_encode_jmp(swd_offset + dap_swd_offset_hold_pins));
    pio_sm_set_enabled(SWD_PIO, swd_sm, true);
}

void dap_swd_pio_flush(void) {
    if (dap_swd_pio_active) {
        swd_flush();
    }
}

// CMSIS-DAP entry points ------------------------------------------------------

void SWJ_Sequence(uint32_t count, const uint8_t* data) {
    if (!dap_swd_pio_active) {
        SWJ_Sequence_GPIO(count, data);
        return;
    }
    while (count) {
        uint32_t bits = count > 32u ? 32u : count;
        uint32_t val = 0;
        for (uint32_t i = 0; i < (bits + 7u) / 8u; i++) {
            val |= (uint32_t)*data++ << (8u * i);
        }
        swd_write(bits, val);
        count -= bits;
    }
}

void SWD_Sequence(uint32_t info, const uint8_t* swdo, uint8_t* swdi) {
    if (!dap_swd_pio_active) {
        SWD_Sequence_GPIO(info, swdo, swdi);
        return;
    }
    uint32_t count = info & SWD_SEQUENCE_CLK;
    if (count == 0u) {
        count = 64u;
    }
    while (count) {
        uint32_t bits = count > 32u ? 32u : count;
        uint32_t bytes = (bits + 7u) / 8u;
        if (info & SWD_SEQUENCE_DIN) {
            swd_read(bits);
            uint32_t val = swd_read_result(bits);
            for (uint32_t i = 0; i < bytes; i++) {
                *swdi++ = (uint8_t)(val >> (8u * i));
            }
        } else {
            uint32_t val = 0;
            for (uint32_t i = 0; i < bytes; i++) {
                val |= (uint32_t)*swdo++ << (8u * i);
            }
            swd_write(bits, val);
        }
        count -= bits;
    }
}

// SWD Transfer I/O
//   request: A[3:2] RnW APnDP
//   data:    DATA[31:0]
//   return:  ACK[2:0]
uint8_t __not_in_flash_func(SWD_Transfer)(uint32_t request, uint32_t* data) {
    if (!dap_swd_pio_active) {
        return SWD_Transfer_GPIO(request, data);
    }

    uint32_t trn = DAP_Data.swd_conf.turnaround;
    uint32_t idle = DAP_Data.transfer.idle_cycles;

    // start, APnDP, RnW, A2, A3, parity, stop, park
    uint32_t parity = parity32(request & 0xfu);
    swd_write(8, 0x81u | ((request & 0xfu) << 1) | (parity << 5));
    swd_put(CMD_DIR_IN);
    // turnaround and ACK in one read, the turnaround bits are discarded
    swd_read(trn + 3u);
    uint32_t ack = (swd_read_result(trn + 3u) >> trn) & 7u;

    if (ack == DAP_TRANSFER_OK) {
        if (request & DAP_TRANSFER_RnW) {
            // data, then parity and turnaround in one read
            swd_read(32);
            swd_read(1u + trn);
            swd_put(CMD_DIR_OUT);
            if (idle) {
                swd_write(idle, 0);
            }
            uint32_t val = swd_get();
            uint32_t bit = swd_read_result(1u + trn) & 1u;
            if (parity32(val) != bit) {
                ack = DAP_TRANSFER_ERROR;
            }
            if (data) {
                *data = val;
            }
        } else {
            // turnaround with SWDIO released, the 1s set the level SWDIO starts driving with
            swd_write(trn, 0xffffffffu);
            swd_put(CMD_DIR_OUT);
            uint32_t val = *data;
            swd_write(32, val);
            // parity, then idle cycles as 0s
            swd_write(1u + idle, parity32(val));
        }
        if (request & DAP_TRANSFER_TIMESTAMP) {
            DAP_Data.timestamp = TIMESTAMP_GET();
        }
        return (uint8_t)ack;
    }

    if ((ack == DAP_TRANSFER_WAIT) || (ack == DAP_TRANSFER_FAULT)) {
        if (DAP_Data.swd_conf.data_phase && (request & DAP_TRANSFER_RnW)) {
            // dummy read of data and parity, then turnaround
            swd_write(32u + 1u + trn, 0);
        } else {
            swd_write(trn, 0xffffffffu);
        }
        swd_put(CMD_DIR_OUT);
        if (DAP_Data.swd_conf.data_phase && !(request & DAP_TRANSFER_RnW)) {
            swd_write(32u + 1u, 0);
        }
        return (uint8_t)ack;
    }

    // protocol error: back off the data phase
    swd_write(trn + 32u + 1u, 0);
    swd_put(CMD_DIR_OUT);
    return (uint8_t)ack;
}
