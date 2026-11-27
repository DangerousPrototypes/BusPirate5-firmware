/*
 * The MIT License (MIT)
 *
 * Copyright (c) 2019 Ha Thach (tinyusb.org)
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 *
 */
#include <stdio.h>
#include "pico/stdlib.h"
#include "pirate.h"
#include "tusb.h"
#include "pirate/mcu.h"
#include "system_config.h"

/*
 * BusPirate has obtained two VID/PID combinations from https://pid.codes/.
 * 0x1209 / 0x7331 - https://pid.codes/1209/7331/ - "Bus Pirate Next Gen CDC" 
 * 0x1209 / 0x7332 - https://pid.codes/1209/7332/ - "Bus Pirate Next Gen Logic Analyzer"
 *
 * The firmware is hard-coded to use the first of these.
 */

// USB 2.1 so Windows asks for the BOS descriptor, which binds WinUSB to the CMSIS-DAP interface
#define USB_BCD 0x0210

//--------------------------------------------------------------------+
// Device Descriptors
//--------------------------------------------------------------------+
tusb_desc_device_t const desc_device = {
    // clang-format off
    .bLength            = sizeof(tusb_desc_device_t),
    .bDescriptorType    = TUSB_DESC_DEVICE,
    .bcdUSB             = USB_BCD,

    // Use Interface Association Descriptor (IAD) for CDC
    // As required by USB Specs IAD's subclass must be common class (2) and protocol must be IAD (1)
    .bDeviceClass       = TUSB_CLASS_MISC,
    .bDeviceSubClass    = MISC_SUBCLASS_COMMON,
    .bDeviceProtocol    = MISC_PROTOCOL_IAD,

    .bMaxPacketSize0    = CFG_TUD_ENDPOINT0_SIZE,

    .idVendor           = USB_VID,
    .idProduct          = USB_PID,
    .bcdDevice          = 0x0102,

    .iManufacturer      = 0x01,
    .iProduct           = 0x02,
    .iSerialNumber      = 0x03,

    .bNumConfigurations = 0x01,
    // clang-format on
};

// Invoked when received GET DEVICE DESCRIPTOR
// Application return pointer to descriptor
uint8_t const* tud_descriptor_device_cb(void) {
    return (uint8_t const*)&desc_device;
}

//--------------------------------------------------------------------+
// Configuration Descriptor
//--------------------------------------------------------------------+

#define USB_TEST 1

enum {
#ifndef USB_TEST
    ITF_NUM_CDC = 0,
    ITF_NUM_CDC_DATA,
#else
    ITF_NUM_CDC_0 = 0,
    ITF_NUM_CDC_0_DATA,
    ITF_NUM_CDC_1,
    ITF_NUM_CDC_1_DATA,
    ITF_NUM_MSC,
#endif
    ITF_NUM_DAP,
    ITF_NUM_TOTAL
};

#if CFG_TUSB_MCU == OPT_MCU_LPC175X_6X || CFG_TUSB_MCU == OPT_MCU_LPC177X_8X || CFG_TUSB_MCU == OPT_MCU_LPC40XX
// LPC 17xx and 40xx endpoint type (bulk/interrupt/iso) are fixed by its number
// 0 control, 1 In, 2 Bulk, 3 Iso, 4 In, 5 Bulk etc ...
#define EPNUM_CDC_NOTIF 0x81
#define EPNUM_CDC_OUT 0x02
#define EPNUM_CDC_IN 0x82

#define EPNUM_MSC_OUT 0x05
#define EPNUM_MSC_IN 0x85

#elif CFG_TUSB_MCU == OPT_MCU_SAMG || CFG_TUSB_MCU == OPT_MCU_SAMX7X
// SAMG & SAME70 don't support a same endpoint number with different direction IN and OUT
//    e.g EP1 OUT & EP1 IN cannot exist together
#define EPNUM_CDC_NOTIF 0x81
#define EPNUM_CDC_OUT 0x02
#define EPNUM_CDC_IN 0x83

#define EPNUM_MSC_OUT 0x04
#define EPNUM_MSC_IN 0x85

#elif CFG_TUSB_MCU == OPT_MCU_CXD56
// CXD56 doesn't support a same endpoint number with different direction IN and OUT
//    e.g EP1 OUT & EP1 IN cannot exist together
// CXD56 USB driver has fixed endpoint type (bulk/interrupt/iso) and direction (IN/OUT) by its number
// 0 control (IN/OUT), 1 Bulk (IN), 2 Bulk (OUT), 3 In (IN), 4 Bulk (IN), 5 Bulk (OUT), 6 In (IN)
#define EPNUM_CDC_NOTIF 0x83
#define EPNUM_CDC_OUT 0x02
#define EPNUM_CDC_IN 0x81

#define EPNUM_MSC_OUT 0x05
#define EPNUM_MSC_IN 0x84

#else
#ifndef USB_TEST
#define EPNUM_CDC_NOTIF 0x81
#define EPNUM_CDC_OUT 0x02
#define EPNUM_CDC_IN 0x82

#define EPNUM_MSC_OUT 0x03
#define EPNUM_MSC_IN 0x83
#else
#define EPNUM_CDC_0_NOTIF 0x81
#define EPNUM_CDC_0_OUT 0x02
#define EPNUM_CDC_0_IN 0x82

#define EPNUM_CDC_1_NOTIF 0x83
#define EPNUM_CDC_1_OUT 0x04
#define EPNUM_CDC_1_IN 0x84

#define EPNUM_MSC_OUT 0x05
#define EPNUM_MSC_IN 0x85
#endif

#endif

// CMSIS-DAP v2 bulk endpoints
#define EPNUM_DAP_OUT 0x06
#define EPNUM_DAP_IN 0x86

#ifndef USB_TEST
#define CONFIG_TOTAL_LEN (TUD_CONFIG_DESC_LEN + TUD_CDC_DESC_LEN + TUD_MSC_DESC_LEN + TUD_VENDOR_DESC_LEN)
#else
#define CONFIG_TOTAL_LEN                                                                                               \
    (TUD_CONFIG_DESC_LEN + TUD_CDC_DESC_LEN + TUD_CDC_DESC_LEN + TUD_MSC_DESC_LEN + TUD_VENDOR_DESC_LEN)
#endif

// full speed configuration
uint8_t const desc_fs_configuration[] = {
    // Config number, interface count, string index, total length, attribute, power in mA
    TUD_CONFIG_DESCRIPTOR(1, ITF_NUM_TOTAL, 0, CONFIG_TOTAL_LEN, 0x00, 100),
#ifndef USB_TEST
    // Interface number, string index, EP notification address and size, EP data address (out, in) and size.
    TUD_CDC_DESCRIPTOR(ITF_NUM_CDC, 4, EPNUM_CDC_NOTIF, 8, EPNUM_CDC_OUT, EPNUM_CDC_IN, 64),
#else
    // 1st CDC: Interface number, string index, EP notification address and size, EP data address (out, in) and size.
    TUD_CDC_DESCRIPTOR(ITF_NUM_CDC_0, 4, EPNUM_CDC_0_NOTIF, 8, EPNUM_CDC_0_OUT, EPNUM_CDC_0_IN, 64),

    // 2nd CDC: Interface number, string index, EP notification address and size, EP data address (out, in) and size.
    TUD_CDC_DESCRIPTOR(ITF_NUM_CDC_1, 6, EPNUM_CDC_1_NOTIF, 8, EPNUM_CDC_1_OUT, EPNUM_CDC_1_IN, 64),

#endif
    // Interface number, string index, EP Out & EP In address, EP size
    TUD_MSC_DESCRIPTOR(ITF_NUM_MSC, 5, EPNUM_MSC_OUT, EPNUM_MSC_IN, 64),

    // CMSIS-DAP v2: Interface number, string index, EP Out & EP In address, EP size
    TUD_VENDOR_DESCRIPTOR(ITF_NUM_DAP, 7, EPNUM_DAP_OUT, EPNUM_DAP_IN, 64),
};

#if TUD_OPT_HIGH_SPEED
// Per USB specs: high speed capable device must report device_qualifier and other_speed_configuration

// high speed configuration
uint8_t const desc_hs_configuration[] = {
    // Config number, interface count, string index, total length, attribute, power in mA
    TUD_CONFIG_DESCRIPTOR(1, ITF_NUM_TOTAL, 0, CONFIG_TOTAL_LEN, 0x00, 100),

    // Interface number, string index, EP notification address and size, EP data address (out, in) and size.
    TUD_CDC_DESCRIPTOR(ITF_NUM_CDC, 4, EPNUM_CDC_NOTIF, 8, EPNUM_CDC_OUT, EPNUM_CDC_IN, 512),

    // Interface number, string index, EP Out & EP In address, EP size
    TUD_MSC_DESCRIPTOR(ITF_NUM_MSC, 5, EPNUM_MSC_OUT, EPNUM_MSC_IN, 512),
};

// other speed configuration
uint8_t desc_other_speed_config[CONFIG_TOTAL_LEN];

// device qualifier is mostly similar to device descriptor since we don't change configuration based on speed
tusb_desc_device_qualifier_t const desc_device_qualifier = {
    // clang-format off
    .bLength = sizeof(tusb_desc_device_qualifier_t),
    .bDescriptorType = TUSB_DESC_DEVICE_QUALIFIER,
    .bcdUSB = USB_BCD,

    .bDeviceClass = TUSB_CLASS_MISC,
    .bDeviceSubClass = MISC_SUBCLASS_COMMON,
    .bDeviceProtocol = MISC_PROTOCOL_IAD,

    .bMaxPacketSize0 = CFG_TUD_ENDPOINT0_SIZE,
    .bNumConfigurations = 0x01,
    .bReserved = 0x00
    // clang-format on
};

// Invoked when received GET DEVICE QUALIFIER DESCRIPTOR request
// Application return pointer to descriptor, whose contents must exist long enough for transfer to complete.
// device_qualifier descriptor describes information about a high-speed capable device that would
// change if the device were operating at the other speed. If not highspeed capable stall this request.
uint8_t const* tud_descriptor_device_qualifier_cb(void) {
    return (uint8_t const*)&desc_device_qualifier;
}

// Invoked when received GET OTHER SEED CONFIGURATION DESCRIPTOR request
// Application return pointer to descriptor, whose contents must exist long enough for transfer to complete
// Configuration descriptor in the other speed e.g if high speed then this is for full speed and vice versa
uint8_t const* tud_descriptor_other_speed_configuration_cb(uint8_t index) {
    (void)index; // for multiple configurations

    // if link speed is high return fullspeed config, and vice versa
    // Note: the descriptor type is OHER_SPEED_CONFIG instead of CONFIG
    memcpy(desc_other_speed_config,
           (tud_speed_get() == TUSB_SPEED_HIGH) ? desc_fs_configuration : desc_hs_configuration,
           CONFIG_TOTAL_LEN);

    desc_other_speed_config[1] = TUSB_DESC_OTHER_SPEED_CONFIG;

    return desc_other_speed_config;
}

#endif // highspeed

// Invoked when received GET CONFIGURATION DESCRIPTOR
// Application return pointer to descriptor
// Descriptor contents must exist long enough for transfer to complete
uint8_t const* tud_descriptor_configuration_cb(uint8_t index) {
    (void)index; // for multiple configurations

#if TUD_OPT_HIGH_SPEED
    // Although we are highspeed, host may be fullspeed.
    return (tud_speed_get() == TUSB_SPEED_HIGH) ? desc_hs_configuration : desc_fs_configuration;
#else
    return desc_fs_configuration;
#endif
}

//--------------------------------------------------------------------+
// String Descriptors
//--------------------------------------------------------------------+

// array of pointer to string descriptors
char const* string_desc_arr[] = {
    (const char[]){ 0x09, 0x04 }, // 0: is supported language is English (0x0409)
    "Bus Pirate",                 // 1: Manufacturer
#if (BP_VER == 5)
    "Bus Pirate 5",               // 2: Product
    "5buspirate",                 // 3: Serial -- now using chip ID (serial port can be remembered per device)
#elif (BP_VER == XL5)
    "Bus Pirate 5XL",             // 2: Product
    "5xbuspirate",                 // 3: Serial -- now using chip ID (serial port can be remembered per device)
#elif (BP_VER == 6)
    "Bus Pirate 6",               // 2: Product
    "6buspirate",                 // 3: Serial -- now using chip ID (serial port can be remembered per device)
#elif (BP_VER == 7)
    "Bus Pirate 7",               // 2: Product
    "7buspirate",                 // 3: Serial -- now using chip ID (serial port can be remembered per device)
#else
    #error "Unknown Bus Pirate version"
#endif
    "Bus Pirate CDC",             // 4: CDC Interface
    "Bus Pirate MSC",             // 5: MSC Interface
    "Bus Pirate BIN",             // 6: Binary CDC Interface
    "Bus Pirate CMSIS-DAP"        // 7: CMSIS-DAP Interface (hosts look for "CMSIS-DAP")
};
// automatically update if an additional string is later added to the table
#define STRING_DESC_ARR_ELEMENT_COUNT (sizeof(string_desc_arr) / sizeof(string_desc_arr[0]))

// temporary buffer returned from tud_descriptor_string_cb()
// relies on only a single outstanding call to this function
// until the buffer is no longer used by caller.
// "contents must exist long enough for transfer to complete"
// Safe because each string descriptor is a separate transfer,
// and only one is handled at a time.
// Use of "length" is ambiguous (byte count?  element count?),
// so use more explicit "BYTE_COUNT" or "ELEMENT_COUNT" instead.
#define MAXIMUM_DESCRIPTOR_STRING_ELEMENT_COUNT 32
static uint16_t _desc_str[MAXIMUM_DESCRIPTOR_STRING_ELEMENT_COUNT];

// Invoked when received GET STRING DESCRIPTOR request
// Application return pointer to descriptor, whose contents must exist long enough for transfer to complete
uint16_t const* tud_descriptor_string_cb(uint8_t index, uint16_t langid) {
    (void)langid;

    uint8_t chr_count;

    if (index == 0) { // supported language == English
        memcpy(&_desc_str[1], string_desc_arr[0], 2);
        chr_count = 1;
    } else if (index == 3 && !system_config.disable_unique_usb_serial_number) {
        // if unique serial number is disabled, will use default value from string table `5buspirate`

        //  1x  uint16_t for length/type
        // 16x  uint16_t to encode 64-bit value as hex (16x nibbles)
        static_assert(MAXIMUM_DESCRIPTOR_STRING_ELEMENT_COUNT >= 17);
        uint64_t unique_id = mcu_get_unique_id();
        for (uint_fast8_t t = 0; t < 16; t++) {
            uint8_t nibble = (unique_id >> (t * 4u)) & 0xF;
            _desc_str[16 - t] = nibble < 10 ? '0' + nibble : 'A' + nibble - 10;
        }
        chr_count = 16;
    } else if (index >= STRING_DESC_ARR_ELEMENT_COUNT) { // if not in table, return NULL
        return NULL;
    } else {
        // Note: the 0xEE index string is a Microsoft OS 1.0 Descriptors.
        // https://docs.microsoft.com/en-us/windows-hardware/drivers/usbcon/microsoft-defined-usb-descriptors
        const char* str = string_desc_arr[index];

        // Cap at max char ...
        chr_count = strlen(str);
        if (chr_count > (MAXIMUM_DESCRIPTOR_STRING_ELEMENT_COUNT - 1)) {
            // first array element stores the length, leaving N-1 for the string
            chr_count = MAXIMUM_DESCRIPTOR_STRING_ELEMENT_COUNT - 1;
        }

        // Convert ASCII string into UTF-16
        for (uint8_t i = 0; i < chr_count; i++) {
            _desc_str[1 + i] = str[i];
        }
    }

    // first byte is length (including header), second byte is string type
    _desc_str[0] = (TUSB_DESC_STRING << 8) | (2 * chr_count + 2);

    return _desc_str;
}

//--------------------------------------------------------------------+
// BOS Descriptor
//--------------------------------------------------------------------+

// Microsoft OS 2.0 descriptors: Windows binds WinUSB to the CMSIS-DAP interface without an INF.
// https://learn.microsoft.com/en-us/windows-hardware/drivers/usbcon/microsoft-os-2-0-descriptors-specification
#define VENDOR_REQUEST_MICROSOFT 1
#define MS_OS_FEATURE_DESC_LEN 0xA0
#define MS_OS_20_DESC_LEN (0x0A + 0x08 + MS_OS_FEATURE_DESC_LEN)

#define BOS_TOTAL_LEN (TUD_BOS_DESC_LEN + TUD_BOS_MICROSOFT_OS_DESC_LEN)

uint8_t const desc_bos[] = {
    // total length, number of device caps
    TUD_BOS_DESCRIPTOR(BOS_TOTAL_LEN, 1),

    // Microsoft OS 2.0 descriptor
    TUD_BOS_MS_OS_20_DESCRIPTOR(MS_OS_20_DESC_LEN, VENDOR_REQUEST_MICROSOFT)
};

uint8_t const desc_ms_os_20[] = {
    // clang-format off
    // Set header: length, type, windows version, total length
    U16_TO_U8S_LE(0x000A), U16_TO_U8S_LE(MS_OS_20_SET_HEADER_DESCRIPTOR), U32_TO_U8S_LE(0x06030000), U16_TO_U8S_LE(MS_OS_20_DESC_LEN),

    // Configuration subset header: length, type, configuration index, reserved, configuration total length
    U16_TO_U8S_LE(0x0008), U16_TO_U8S_LE(MS_OS_20_SUBSET_HEADER_CONFIGURATION), 0, 0, U16_TO_U8S_LE(MS_OS_20_DESC_LEN - 0x0A),

    // Function Subset header: length, type, first interface, reserved, subset length
    U16_TO_U8S_LE(0x0008), U16_TO_U8S_LE(MS_OS_20_SUBSET_HEADER_FUNCTION), ITF_NUM_DAP, 0, U16_TO_U8S_LE(MS_OS_FEATURE_DESC_LEN),

    // MS OS 2.0 Compatible ID descriptor: length, type, compatible ID, sub compatible ID
    U16_TO_U8S_LE(0x0014), U16_TO_U8S_LE(MS_OS_20_FEATURE_COMPATBLE_ID), 'W', 'I', 'N', 'U', 'S', 'B', 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, // sub-compatible

    // MS OS 2.0 Registry property descriptor: length, type
    U16_TO_U8S_LE(MS_OS_FEATURE_DESC_LEN - 0x08 - 0x14), U16_TO_U8S_LE(MS_OS_20_FEATURE_REG_PROPERTY),
    // wPropertyDataType, wPropertyNameLength and PropertyName "DeviceInterfaceGUIDs\0" in UTF-16
    U16_TO_U8S_LE(0x0007), U16_TO_U8S_LE(0x002A),
    'D', 0x00, 'e', 0x00, 'v', 0x00, 'i', 0x00, 'c', 0x00, 'e', 0x00, 'I', 0x00, 'n', 0x00, 't', 0x00, 'e', 0x00,
    'r', 0x00, 'f', 0x00, 'a', 0x00, 'c', 0x00, 'e', 0x00, 'G', 0x00, 'U', 0x00, 'I', 0x00, 'D', 0x00, 's', 0x00, 0x00, 0x00,
    U16_TO_U8S_LE(0x0050), // wPropertyDataLength
    // bPropertyData: the CMSIS-DAP v2 interface GUID "{CDB3B5AD-293B-4663-AA36-1AAE46463776}" as a UTF-16 multi-string
    '{', 0x00, 'C', 0x00, 'D', 0x00, 'B', 0x00, '3', 0x00, 'B', 0x00, '5', 0x00, 'A', 0x00, 'D', 0x00, '-', 0x00,
    '2', 0x00, '9', 0x00, '3', 0x00, 'B', 0x00, '-', 0x00, '4', 0x00, '6', 0x00, '6', 0x00, '3', 0x00, '-', 0x00,
    'A', 0x00, 'A', 0x00, '3', 0x00, '6', 0x00, '-', 0x00, '1', 0x00, 'A', 0x00, 'A', 0x00, 'E', 0x00, '4', 0x00,
    '6', 0x00, '4', 0x00, '6', 0x00, '3', 0x00, '7', 0x00, '7', 0x00, '6', 0x00, '}', 0x00, 0x00, 0x00, 0x00, 0x00,
    // clang-format on
};

TU_VERIFY_STATIC(sizeof(desc_ms_os_20) == MS_OS_20_DESC_LEN, "Incorrect size");

// Invoked when received GET BOS DESCRIPTOR request
uint8_t const* tud_descriptor_bos_cb(void) {
    return desc_bos;
}

// Invoked when a control transfer occurred on an interface of this class
bool tud_vendor_control_xfer_cb(uint8_t rhport, uint8_t stage, tusb_control_request_t const* request) {
    // nothing to do with DATA & ACK stage
    if (stage != CONTROL_STAGE_SETUP) {
        return true;
    }

    if (request->bRequest == VENDOR_REQUEST_MICROSOFT && request->wIndex == 7) {
        // Get Microsoft OS 2.0 compatible descriptor
        return tud_control_xfer(rhport, request, (void*)(uintptr_t)desc_ms_os_20, sizeof(desc_ms_os_20));
    }

    // stall unknown request
    return false;
}
