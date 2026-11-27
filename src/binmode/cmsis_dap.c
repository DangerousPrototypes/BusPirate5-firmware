/*
 * CMSIS-DAP v2 debug probe binary mode
 *
 * Copyright (c) 2026 Daniel Schaefer
 *
 * SPDX-License-Identifier: MIT
 *
 * USB (core1) and DAP execution (core0) only share two single-producer,
 * single-consumer packet rings, so neither core blocks the other:
 *   requests:  USB OUT -> core1 -> ring -> core0 DAP_ExecuteCommand
 *   responses: core0 -> ring -> core1 -> USB IN
 * CMSIS-DAP v2 packets are delimited by USB transfers, which is why this is a
 * small class driver instead of the TinyUSB vendor class (its FIFO merges packets).
 */

#include <stdio.h>
#include "pico/stdlib.h"
#include "hardware/sync.h"
#include "pirate.h"
#include "system_config.h"
#include "pirate/bio.h"
#include "tusb.h"
#include "device/usbd_pvt.h"
#include "lib/cmsis-dap/DAP_config.h"
#include "lib/cmsis-dap/DAP.h"
#include "binmode/cmsis_dap.h"
#include "ui/ui_term.h"
#include "ui/ui_help.h"

const char cmsis_dap_name[] = "CMSIS-DAP SWD/JTAG debug probe";

static const char pin_labels[][5] = { "TRST", "SRST", "TDO", "TMS", "TCK", "TDI" };
static const uint8_t pin_bios[] = { DAP_NTRST_BIO, DAP_NRESET_BIO, DAP_TDO_BIO, DAP_TMS_BIO, DAP_TCK_BIO, DAP_TDI_BIO };

typedef struct {
    uint8_t data[DAP_PACKET_COUNT][DAP_PACKET_SIZE];
    uint16_t len[DAP_PACKET_COUNT];
    volatile uint32_t wptr;
    volatile uint32_t rptr;
} dap_ring_t;

#define RING_SLOT(ptr) ((ptr) % DAP_PACKET_COUNT)
#define RING_FULL(ring) ((ring).wptr - (ring).rptr >= DAP_PACKET_COUNT)
#define RING_EMPTY(ring) ((ring).wptr == (ring).rptr)

static dap_ring_t requests;  // written by core1, read by core0
static dap_ring_t responses; // written by core0, read by core1

// set by core0 while the binmode is selected
static volatile bool dap_active = false;

// core1 only
static uint8_t dap_rhport;
static uint8_t dap_ep_out;
static uint8_t dap_ep_in;
static bool dap_out_armed;
static bool dap_in_busy;
static bool dap_in_reject;
static uint32_t dap_rejects;
static uint8_t dap_reject_response[1] = { ID_DAP_Invalid };

//--------------------------------------------------------------------+
// USB class driver (core1)
//--------------------------------------------------------------------+

static void dap_usb_kick(void) {
    if (!dap_ep_out) {
        return;
    }

    if (!dap_out_armed && !RING_FULL(requests)) {
        dap_out_armed =
            usbd_edpt_xfer(dap_rhport, dap_ep_out, requests.data[RING_SLOT(requests.wptr)], DAP_PACKET_SIZE);
    }

    if (dap_in_busy) {
        return;
    }
    // answer requests that arrived while the binmode was not selected first, they are older
    if (dap_rejects) {
        dap_in_busy = dap_in_reject = usbd_edpt_xfer(dap_rhport, dap_ep_in, dap_reject_response, 1);
    } else if (!RING_EMPTY(responses)) {
        __dmb();
        uint32_t slot = RING_SLOT(responses.rptr);
        dap_in_busy = usbd_edpt_xfer(dap_rhport, dap_ep_in, responses.data[slot], responses.len[slot]);
    }
}

static void dap_usb_init(void) {}

static bool dap_usb_deinit(void) {
    return true;
}

static void dap_usb_reset(uint8_t rhport) {
    (void)rhport;
    dap_ep_out = 0;
    dap_ep_in = 0;
    dap_out_armed = false;
    dap_in_busy = false;
    dap_in_reject = false;
    dap_rejects = 0;
}

static uint16_t dap_usb_open(uint8_t rhport, tusb_desc_interface_t const* itf_desc, uint16_t max_len) {
    TU_VERIFY(TUSB_CLASS_VENDOR_SPECIFIC == itf_desc->bInterfaceClass && itf_desc->bNumEndpoints == 2, 0);
    uint16_t const drv_len = sizeof(tusb_desc_interface_t) + 2 * sizeof(tusb_desc_endpoint_t);
    TU_VERIFY(max_len >= drv_len, 0);

    tusb_desc_endpoint_t const* ep_desc = (tusb_desc_endpoint_t const*)tu_desc_next(itf_desc);
    for (uint8_t i = 0; i < 2; i++, ep_desc = (tusb_desc_endpoint_t const*)tu_desc_next(ep_desc)) {
        TU_ASSERT(usbd_edpt_open(rhport, ep_desc), 0);
        if (tu_edpt_dir(ep_desc->bEndpointAddress) == TUSB_DIR_IN) {
            dap_ep_in = ep_desc->bEndpointAddress;
        } else {
            dap_ep_out = ep_desc->bEndpointAddress;
        }
    }
    dap_rhport = rhport;

    // responses queued for a previous host session are stale
    responses.rptr = responses.wptr;
    dap_usb_kick();
    return drv_len;
}

static bool dap_usb_control_xfer_cb(uint8_t rhport, uint8_t stage, tusb_control_request_t const* request) {
    (void)rhport;
    (void)stage;
    (void)request;
    return false;
}

static bool dap_usb_xfer_cb(uint8_t rhport, uint8_t ep_addr, xfer_result_t result, uint32_t xferred_bytes) {
    (void)rhport;
    (void)result;

    if (ep_addr == dap_ep_out) {
        dap_out_armed = false;
        uint8_t* request = requests.data[RING_SLOT(requests.wptr)];
        if (xferred_bytes == 0) {
            // nothing to do, the slot is re-armed below
        } else if (request[0] == ID_DAP_TransferAbort) {
            // handled out of band so it can stop a transfer core0 is executing, it has no response
            DAP_TransferAbort = 1U;
        } else if (!dap_active) {
            dap_rejects++;
        } else {
            requests.len[RING_SLOT(requests.wptr)] = (uint16_t)xferred_bytes;
            __dmb();
            requests.wptr++;
        }
    } else if (ep_addr == dap_ep_in) {
        dap_in_busy = false;
        if (dap_in_reject) {
            dap_in_reject = false;
            dap_rejects--;
        } else {
            responses.rptr++;
        }
    } else {
        return false;
    }

    dap_usb_kick();
    return true;
}

static usbd_class_driver_t const dap_usb_driver = {
#if CFG_TUSB_DEBUG >= 2
    .name = "CMSIS-DAP",
#endif
    .init = dap_usb_init,
    .deinit = dap_usb_deinit,
    .reset = dap_usb_reset,
    .open = dap_usb_open,
    .control_xfer_cb = dap_usb_control_xfer_cb,
    .xfer_cb = dap_usb_xfer_cb,
    .sof = NULL,
};

// Add the CMSIS-DAP driver to the TinyUSB stack
usbd_class_driver_t const* usbd_app_driver_get_cb(uint8_t* driver_count) {
    *driver_count = 1;
    return &dap_usb_driver;
}

void cmsis_dap_usb_task(void) {
    dap_usb_kick();
}

//--------------------------------------------------------------------+
// Binmode (core0)
//--------------------------------------------------------------------+

void cmsis_dap_setup(void) {
    for (uint8_t i = 0; i < count_of(pin_bios); i++) {
        system_bio_update_purpose_and_label(true, pin_bios[i], BP_PIN_MODE, pin_labels[i]);
    }
    bio_init();
    DAP_Setup();
    // requests left over from a previous session were never answered, drop them
    requests.rptr = requests.wptr;
    dap_active = true;
}

void cmsis_dap_setup_message(void) {
    printf("%sTarget voltage:%s supply target VTref to VOUT/VREF, or enable the PSU\r\n",
           ui_term_color_info(),
           ui_term_color_reset());
    printf("%sPinout:%s IO2 nTRST, IO3 nRESET, IO4 TDO, IO5 TMS/SWDIO, IO6 TCK/SWCLK, IO7 TDI\r\n",
           ui_term_color_info(),
           ui_term_color_reset());
    printf(
        "%sOpenOCD:%s -c \"adapter driver cmsis-dap; cmsis-dap backend usb_bulk; adapter usb vid_pid 0x%04x 0x%04x\"\r\n",
        ui_term_color_info(),
        ui_term_color_reset(),
        USB_VID,
        USB_PID);
    ui_help_sanity_check(true, 0x00);
}

void cmsis_dap_service(void) {
    while (!RING_EMPTY(requests) && !RING_FULL(responses)) {
        uint32_t wptr = requests.wptr;
        __dmb();
        // atomic command support: hold DAP_QueueCommands packets until a packet that is not queued arrives
        uint32_t n = requests.rptr;
        while (n != wptr && requests.data[RING_SLOT(n)][0] == ID_DAP_QueueCommands) {
            n++;
        }
        if (n == wptr) {
            return;
        }

        uint8_t* request = requests.data[RING_SLOT(requests.rptr)];
        if (request[0] == ID_DAP_QueueCommands) {
            request[0] = ID_DAP_ExecuteCommands;
        }
        uint32_t slot = RING_SLOT(responses.wptr);
        responses.len[slot] = (uint16_t)DAP_ExecuteCommand(request, responses.data[slot]);
        __dmb();
        responses.wptr++;
        requests.rptr++;
    }
}

void cmsis_dap_cleanup(void) {
    dap_active = false;
    PORT_OFF();
    dap_swd_pio_release();
    for (uint8_t i = 0; i < count_of(pin_bios); i++) {
        system_bio_update_purpose_and_label(false, pin_bios[i], BP_PIN_MODE, 0);
    }
    bio_init();
}
