/*
 * Copyright (c) 2013-2017 ARM Limited. All rights reserved.
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
 * Project:      CMSIS-DAP Configuration
 * Title:        DAP_config.h CMSIS-DAP Configuration File
 *
 * Modified by Daniel Schaefer 2026: Bus Pirate buffered IO pin mapping.
 *
 *---------------------------------------------------------------------------*/

#ifndef __DAP_CONFIG_H__
#define __DAP_CONFIG_H__

#include "pico/stdlib.h"
#include "hardware/clocks.h"
#include "hardware/gpio.h"
#include "cmsis_compiler.h"
#include "pirate.h"

// Bus Pirate pin mapping, same as JTAG mode (mode/jtag.c).
// Every IO goes through a level shifter: BUFIOx is the RP2xxx side of the signal,
// BUFDIRx selects whether the shifter drives the target (1) or listens to it (0).
//   IO2 nTRST, IO3 nRESET, IO4 TDO, IO5 TMS/SWDIO, IO6 TCK/SWCLK, IO7 TDI
#define DAP_NTRST_BIO BIO2
#define DAP_NTRST_IO BUFIO2
#define DAP_NTRST_DIR BUFDIR2
#define DAP_NRESET_BIO BIO3
#define DAP_NRESET_IO BUFIO3
#define DAP_NRESET_DIR BUFDIR3
#define DAP_TDO_BIO BIO4
#define DAP_TDO_IO BUFIO4
#define DAP_TDO_DIR BUFDIR4
#define DAP_TMS_BIO BIO5
#define DAP_TMS_IO BUFIO5
#define DAP_TMS_DIR BUFDIR5
#define DAP_TCK_BIO BIO6
#define DAP_TCK_IO BUFIO6
#define DAP_TCK_DIR BUFDIR6
#define DAP_TDI_BIO BIO7
#define DAP_TDI_IO BUFIO7
#define DAP_TDI_DIR BUFDIR7

/// Processor Clock of the Cortex-M MCU used in the Debug Unit.
#define CPU_CLOCK clock_get_hz(clk_sys)

/// Number of processor cycles for I/O Port write operations.
#define IO_PORT_WRITE_CYCLES 2U

/// Indicate that Serial Wire Debug (SWD) communication mode is available at the Debug Access Port.
#define DAP_SWD 1

/// Indicate that JTAG communication mode is available at the Debug Port.
#define DAP_JTAG 1

/// Configure maximum number of JTAG devices on the scan chain connected to the Debug Access Port.
#define DAP_JTAG_DEV_CNT 8U

/// Default communication mode on the Debug Access Port.
#define DAP_DEFAULT_PORT 1U ///< Default JTAG/SWJ Port Mode: 1 = SWD, 2 = JTAG.

/// Default communication speed on the Debug Access Port for SWD and JTAG mode.
#define DAP_DEFAULT_SWJ_CLOCK 1000000U

/// Maximum Package Size for Command and Response data (full speed bulk endpoint).
#define DAP_PACKET_SIZE 64U

/// Maximum Package Buffers for Command and Response data.
#define DAP_PACKET_COUNT 8U

/// SWO is not supported.
#define SWO_UART 0
#define SWO_UART_MAX_BAUDRATE 10000000U
#define SWO_MANCHESTER 0
#define SWO_BUFFER_SIZE 4096U
#define SWO_STREAM 0

/// Clock frequency of the Test Domain Timer. Timer value is returned with \ref TIMESTAMP_GET.
#define TIMESTAMP_CLOCK 1000000U

/// Debug Unit is not connected to a fixed Target Device.
#define TARGET_DEVICE_FIXED 0

// The USB string descriptors already identify the probe
__STATIC_INLINE uint8_t DAP_GetVendorString(char* str) {
    (void)str;
    return (0U);
}

__STATIC_INLINE uint8_t DAP_GetProductString(char* str) {
    (void)str;
    return (0U);
}

__STATIC_INLINE uint8_t DAP_GetSerNumString(char* str) {
    (void)str;
    return (0U);
}

// Buffered IO helpers ------------------------------------

// Shifter first, then the pin: the RP2xxx never fights the shifter's output
__STATIC_FORCEINLINE void dap_pin_output(uint32_t io, uint32_t dir) {
    gpio_put(dir, 1);
    gpio_set_dir(io, GPIO_OUT);
}

// Pin first, then the shifter: the shifter never fights the RP2xxx's output
__STATIC_FORCEINLINE void dap_pin_input(uint32_t io, uint32_t dir) {
    gpio_set_dir(io, GPIO_IN);
    gpio_put(dir, 0);
}

// nTRST and nRESET are open drain: drive low to assert, float to release
__STATIC_FORCEINLINE void dap_pin_open_drain(uint32_t io, uint32_t dir, uint32_t bit) {
    if (bit & 1U) {
        dap_pin_input(io, dir);
    } else {
        gpio_put(io, 0);
        dap_pin_output(io, dir);
    }
}

// Configure DAP I/O pins ------------------------------

/** Setup JTAG I/O pins: TCK, TMS, TDI, TDO, nTRST, and nRESET.
 - TCK, TMS, TDI, nTRST, nRESET to output mode and set to high level.
 - TDO to input mode.
*/
__STATIC_INLINE void PORT_JTAG_SETUP(void) {
    gpio_put(DAP_TCK_IO, 1);
    gpio_put(DAP_TMS_IO, 1);
    gpio_put(DAP_TDI_IO, 1);
    dap_pin_output(DAP_TCK_IO, DAP_TCK_DIR);
    dap_pin_output(DAP_TMS_IO, DAP_TMS_DIR);
    dap_pin_output(DAP_TDI_IO, DAP_TDI_DIR);
    dap_pin_input(DAP_TDO_IO, DAP_TDO_DIR);
    dap_pin_input(DAP_NTRST_IO, DAP_NTRST_DIR);
    dap_pin_input(DAP_NRESET_IO, DAP_NRESET_DIR);
}

/** Setup SWD I/O pins: SWCLK, SWDIO, and nRESET.
 - SWCLK, SWDIO, nRESET to output mode and set to default high level.
 - TDI, nTRST to HighZ mode (pins are unused in SWD mode).
*/
__STATIC_INLINE void PORT_SWD_SETUP(void) {
    gpio_put(DAP_TCK_IO, 1);
    gpio_put(DAP_TMS_IO, 1);
    dap_pin_output(DAP_TCK_IO, DAP_TCK_DIR);
    dap_pin_output(DAP_TMS_IO, DAP_TMS_DIR);
    dap_pin_input(DAP_TDI_IO, DAP_TDI_DIR);
    dap_pin_input(DAP_TDO_IO, DAP_TDO_DIR);
    dap_pin_input(DAP_NTRST_IO, DAP_NTRST_DIR);
    dap_pin_input(DAP_NRESET_IO, DAP_NRESET_DIR);
}

/** Disable JTAG/SWD I/O Pins.
 - TCK/SWCLK, TMS/SWDIO, TDI, TDO, nTRST, nRESET to High-Z mode.
*/
__STATIC_INLINE void PORT_OFF(void) {
    dap_pin_input(DAP_TCK_IO, DAP_TCK_DIR);
    dap_pin_input(DAP_TMS_IO, DAP_TMS_DIR);
    dap_pin_input(DAP_TDI_IO, DAP_TDI_DIR);
    dap_pin_input(DAP_TDO_IO, DAP_TDO_DIR);
    dap_pin_input(DAP_NTRST_IO, DAP_NTRST_DIR);
    dap_pin_input(DAP_NRESET_IO, DAP_NRESET_DIR);
}

// SWCLK/TCK I/O pin -------------------------------------

__STATIC_FORCEINLINE uint32_t PIN_SWCLK_TCK_IN(void) {
    return gpio_get(DAP_TCK_IO);
}

__STATIC_FORCEINLINE void PIN_SWCLK_TCK_SET(void) {
    gpio_put(DAP_TCK_IO, 1);
}

__STATIC_FORCEINLINE void PIN_SWCLK_TCK_CLR(void) {
    gpio_put(DAP_TCK_IO, 0);
}

// SWDIO/TMS Pin I/O --------------------------------------

__STATIC_FORCEINLINE uint32_t PIN_SWDIO_TMS_IN(void) {
    return gpio_get(DAP_TMS_IO);
}

__STATIC_FORCEINLINE void PIN_SWDIO_TMS_SET(void) {
    gpio_put(DAP_TMS_IO, 1);
}

__STATIC_FORCEINLINE void PIN_SWDIO_TMS_CLR(void) {
    gpio_put(DAP_TMS_IO, 0);
}

__STATIC_FORCEINLINE uint32_t PIN_SWDIO_IN(void) {
    return gpio_get(DAP_TMS_IO);
}

__STATIC_FORCEINLINE void PIN_SWDIO_OUT(uint32_t bit) {
    gpio_put(DAP_TMS_IO, bit & 1U);
}

__STATIC_FORCEINLINE void PIN_SWDIO_OUT_ENABLE(void) {
    dap_pin_output(DAP_TMS_IO, DAP_TMS_DIR);
}

__STATIC_FORCEINLINE void PIN_SWDIO_OUT_DISABLE(void) {
    dap_pin_input(DAP_TMS_IO, DAP_TMS_DIR);
}

// TDI Pin I/O ---------------------------------------------

__STATIC_FORCEINLINE uint32_t PIN_TDI_IN(void) {
    return gpio_get(DAP_TDI_IO);
}

__STATIC_FORCEINLINE void PIN_TDI_OUT(uint32_t bit) {
    gpio_put(DAP_TDI_IO, bit & 1U);
}

// TDO Pin I/O ---------------------------------------------

__STATIC_FORCEINLINE uint32_t PIN_TDO_IN(void) {
    return gpio_get(DAP_TDO_IO);
}

// nTRST Pin I/O -------------------------------------------

__STATIC_FORCEINLINE uint32_t PIN_nTRST_IN(void) {
    return gpio_get(DAP_NTRST_IO);
}

__STATIC_FORCEINLINE void PIN_nTRST_OUT(uint32_t bit) {
    dap_pin_open_drain(DAP_NTRST_IO, DAP_NTRST_DIR, bit);
}

// nRESET Pin I/O------------------------------------------

__STATIC_FORCEINLINE uint32_t PIN_nRESET_IN(void) {
    return gpio_get(DAP_NRESET_IO);
}

__STATIC_FORCEINLINE void PIN_nRESET_OUT(uint32_t bit) {
    dap_pin_open_drain(DAP_NRESET_IO, DAP_NRESET_DIR, bit);
}

// Status LEDs: none, the LCD pin labels show the probe is active

__STATIC_INLINE void LED_CONNECTED_OUT(uint32_t bit) {
    (void)bit;
}

__STATIC_INLINE void LED_RUNNING_OUT(uint32_t bit) {
    (void)bit;
}

// Timestamp ------------------------------------------------

__STATIC_INLINE uint32_t TIMESTAMP_GET(void) {
    return time_us_32();
}

// Initialization -------------------------------------------

/** Setup of the Debug Unit I/O pins (called when Debug Unit is initialized).
 - all I/O pins: input buffer enabled, output pins are set to HighZ mode.
*/
__STATIC_INLINE void DAP_SETUP(void) {
    PORT_OFF();
}

/** Reset Target Device with custom specific I/O pin or command sequence.
\return 0 = no device specific reset sequence is implemented.
*/
__STATIC_INLINE uint8_t RESET_TARGET(void) {
    return (0U);
}

#endif /* __DAP_CONFIG_H__ */
