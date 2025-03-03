#define BP_DEBUG_OVERRIDE_DEFAULT_CATEGORY BP_DEBUG_CAT_OTP

#include <stdint.h>
#include <stdbool.h>
#include <inttypes.h>
#include <stdio.h>
#include <assert.h>
#include "../pirate.h"
#include "bp_otp.h"
#include "pico.h"
#include <boot/bootrom_constants.h>
#include "pico/bootrom.h"
#include "pico/unique_id.h"
#include "debug_rtt.h"

#ifndef BP_OTP_PRODUCT_VERSION_STRING
    #error "BP_OTP_PRODUCT_VERSION must be defined in the platform header as the portion of the string to follow `Bus Pirate `.  E.g., `6` for BP6, or `5XL` for the BP5XL"
#endif

// ****** IMPORTANT DEVELOPEMENT NOTE ******
// During development, it's REALLY useful to force the code to single-step through this process.
// To support this, this file has code that uses waits for the RTT terminal to accept input.
//
// To use this is a TWO STEP process:
// 1. Define WAIT_FOR_KEY() to be MyWaitForAnyKey_with_discards() in this file.
// 2. At the location in the code you want to start single-stepping, set the static `g_WaitForKey` to true.
//
// Because OTP fuses can only transition from 0 -> 1, this capability is critical to
// minimizing the number of RP2350 chips with invalid data during development.
#pragma region    // Support for RTT-based single-stepping

// TODO: update this to wait for actual keypresses over RTT (as in the earlier experimentation firmware)
//       to allow for review of all the things happening....
// #define WAIT_FOR_KEY()
#define WAIT_FOR_KEY() MyWaitForAnyKey_with_discards()

// decide where to single-step through the whitelabel process ... controlled via RTT (no USB connection required)
static volatile bool g_WaitForKey = false;
static void MyWaitForAnyKey_with_discards(void) {
    if (!g_WaitForKey) {
        return;
    }
    // clear any prior keypresses
    int t;
    do {
        t = SEGGER_RTT_GetKey();
    } while (t >= 0);

    int c = SEGGER_RTT_WaitKey();

    // clear any remaining kepresses (particularly useful for telnet, which does line-by-line input)
    do {
        t = SEGGER_RTT_GetKey();
    } while (t >= 0);

    return;
}

#pragma endregion // Support for RTT-based single-stepping
#pragma region    // DIE() macro
#define DIE() die(__LINE__)
// define as returning bool to allow:
//     funcA() || DIE();
// Otherwise, compiler complains because of using void in a boolean context.
static bool __attribute__((noreturn)) die(int line) {
    PRINT_ERROR("Whitelabel Error: Aborting whitelabel process @ line %d\n");
    hard_assert(false);
    while (1);
}
#pragma endregion    // DIE() macro
static char byte_to_printable_char(uint8_t byte) {
    if (byte < 0x20u) {
        return '.';
    }
    if (byte > 0x7Eu) {
        return '.';
    }
    return byte;
}
#pragma region    // Whitelabel OTP structures
typedef struct _BP_OTP_USB_BOOT_FLAGS {
    union {
        uint32_t as_uint32;
        struct {
            uint32_t usb_vid_valid          : 1; //  0: 0x000001
            uint32_t usb_pid_valid          : 1; //  1: 0x000002
            uint32_t usb_bcd_valid          : 1; //  2: 0x000004
            uint32_t usb_lang_id_valid      : 1; //  3: 0x000008
            uint32_t usb_manufacturer_valid : 1; //  4: 0x000010
            uint32_t usb_product_valid      : 1; //  5: 0x000020
            uint32_t usb_serial_valid       : 1; //  6: 0x000040
            uint32_t usb_max_power_valid    : 1; //  7: 0x000080
            uint32_t volume_label_valid     : 1; //  8: 0x000100
            uint32_t scsi_vendor_valid      : 1; //  9: 0x000200
            uint32_t scsi_product_valid     : 1; // 10: 0x000400
            uint32_t scsi_rev_valid         : 1; // 11: 0x000800
            uint32_t redirect_url_valid     : 1; // 12: 0x001000
            uint32_t redirect_name_valid    : 1; // 13: 0x002000
            uint32_t info_uf2_model_valid   : 1; // 14: 0x004000
            uint32_t info_uf2_boardid_valid : 1; // 15: 0x008000
            uint32_t _rfu_16                : 1; // 16: 0x010000
            uint32_t _rfu_17                : 1; // 17: 0x020000
            uint32_t _rfu_18                : 1; // 18: 0x040000
            uint32_t _rfu_19                : 1; // 19: 0x080000
            uint32_t _rfu_20                : 1; // 20: 0x100000
            uint32_t _rfu_21                : 1; // 21: 0x200000
            uint32_t white_label_addr_valid : 1; // 22: 0x400000
            uint32_t _rfu_23                : 1; // 23: 0x800000
            uint32_t _must_be_zero          : 8; // 24..31 are not used in raw OTP data
        };
    };
} BP_OTP_USB_BOOT_FLAGS;
static_assert(sizeof(BP_OTP_USB_BOOT_FLAGS) == sizeof(uint32_t), "BP_OTP_USB_BOOT_FLAGS must be 32-bits");

typedef struct _BP_OTP_WHITELABEL_STRDEF {
    union {
        uint16_t as_uint16;
        struct {
            uint16_t character_count : 7;
            uint16_t is_unicode      : 1; // Unicode only supported for usb manufacturer, product, and serial
            uint16_t row_offset      : 8; // USB_WHITE_LABEL_ADDR + row_offset == start row for the string
        };
    };
} BP_OTP_WHITELABEL_STRDEF;
static_assert(sizeof(BP_OTP_WHITELABEL_STRDEF) == sizeof(uint16_t), "BP_OTP_WHITELABEL_STRDEF must be 16-bits");

typedef struct _BP_OTP_WHITELABEL {
    uint16_t usb_vid; // USB vendor ID  ... 0x1209 for buspirate
    uint16_t usb_pid; // USB product ID ... 0x7332 for buspirate UF2 bootloader
    uint16_t usb_bcd_device_value; // not used by buspirate ... bootrom defaults to 0x0100
    uint16_t usb_strings_lang_id;  // not used by buspirate ... bootrom defaults to 0x0409 (US English)
    BP_OTP_WHITELABEL_STRDEF usb_manufacturer;
    BP_OTP_WHITELABEL_STRDEF usb_product;
    BP_OTP_WHITELABEL_STRDEF usb_serial;
    uint16_t usb_config_attributes_max_power; // not used by buspirate ... bootrom defaults to 
    BP_OTP_WHITELABEL_STRDEF storage_volume_label;
    BP_OTP_WHITELABEL_STRDEF scsi_vid; // scsi vendor ID  .. "Bus Pir8" for buspirate
    BP_OTP_WHITELABEL_STRDEF scsi_pid; // scsi product ID
    BP_OTP_WHITELABEL_STRDEF scsi_rev; // scsi revision
    BP_OTP_WHITELABEL_STRDEF index_htm_redirect_url;
    BP_OTP_WHITELABEL_STRDEF index_htm_redirect_name;
    BP_OTP_WHITELABEL_STRDEF info_uf2_model;
    BP_OTP_WHITELABEL_STRDEF info_uf2_boardid;
} BP_OTP_WHITELABEL;
static_assert(sizeof(BP_OTP_WHITELABEL) == 0x20u, "BP_OTP_WHITELABEL must be 32 bytes (16 rows)");

typedef struct _BP_OTP_ROW_RANGE {
    uint16_t start_row;
    uint16_t row_count;
} BP_OTP_ROW_RANGE;
#pragma endregion // Whitelabel OTP structures

// NEXT STEPS: Another Pico to sacrafice to the USB Whitelabel testing....
// Current code is single-stepping successfully....

#pragma region    // Expected Results

// Expected results:
// | Row   | Data     | Chars    | Type   | Description
// |-------|----------|----------|--------|-------------------------------------------
// | 0x059 | 0x40F733 |          | RBIT-3 | USB BOOT FLAGS
// | 0x05A | 0x40F733 |          | RBIT-3 | ""
// | 0x05B | 0x40F733 |          | RBIT-3 | ""
// | 0x05C |   0x00C0 |          | ECC    | WHITE_LABEL_ADDR

// | Row   | Data     | Chars    | Type   | Description (static whitelabel structure)
// |-------|----------|----------|--------|-------------------------------------------
// | 0x0C0 |   0x1209 |          | ECC    | USB VID
// | 0x0C1 |   0x7332 |          | ECC    | USB PID
// | 0x0C2 |   0x0000 |          | ECC    | USB BCD Device
// | 0x0C3 |   0x0000 |          | ECC    | USB LangID for strings
// | 0x0C4 |   0x230A |          | ECC    | USB MANU
// | 0x0C5 |   0x2311 | *****    | ECC    | USB PROD
// | 0x0C6 |   0x0000 |          | ECC    | USB Serial Number
// | 0x0C7 |   0x0000 |          | ECC    | USB config attributes & max power
// | 0x0C8 |   0x1B08 |          | ECC    | STOR VOLUME LABEL
// | 0x0C9 |   0x1F08 |          | ECC    | SCSI INQUIRY VENDOR
// | 0x0CA |   0x2311 | *****    | ECC    | SCSI INQUIRY PRODUCT
// | 0x0CB |   0x0000 |          | ECC    | SCSI INQUIRY REVISION
// | 0x0CC |   0x1016 |          | ECC    | redirect URL
// | 0x0CD |   0x140D |          | ECC    | redirect name
// | 0x0CE |   0x2311 | *****    | ECC    | info_uf2.txt product
// | 0x0CF |   0x3417 |          | ECC    | info_uf2.txt boardid

// | Row   | Data     | Chars    | Type   | Description (static portion)
// |-------|----------|----------|--------|-------------------------------------------
// | 0x0D0 |   0x7468 | `ht`     | ECC    | STRDEF offset 0x10 Len 0x16 = "https://buspirate.com/"
// | 0x0D1 |   0x7074 | `tp`     | ECC    | 
// | 0x0D2 |   0x3a73 | `s:`     | ECC    |
// | 0x0D3 |   0x2f2f | `//`     | ECC    |
// | 0x0D4 |   0x7562 | `bu`     | ECC    | STRDEF offset 0x14 Len 0x0D = "buspirate.com"
// | 0x0D5 |   0x7073 | `sp`     | ECC    |
// | 0x0D6 |   0x7269 | `ir`     | ECC    |
// | 0x0D7 |   0x7461 | `at`     | ECC    |
// | 0x0D8 |   0x2e65 | `e.`     | ECC    |
// | 0x0D9 |   0x6f63 | `co`     | ECC    |
// | 0x0DA |   0x2f6d | `m/`     | ECC    |
// | 0x0DB |   0x5042 | `BP`     | ECC    | STRDEF offset 0x1B Len 0x08 = "BP__BOOT"
// | 0x0DC |   0x5f5f | `__`     | ECC    |
// | 0x0DD |   0x4f42 | `BO`     | ECC    |
// | 0x0DE |   0x544f | `OT`     | ECC    |
// | 0x0DF |   0x7542 | `Bu`     | ECC    | STRDEF offset 0x1F Len 0x08 = "Bus Pir8"
// | 0x0E0 |   0x2073 | `s `     | ECC    |
// | 0x0E1 |   0x6950 | `Pi`     | ECC    |
// | 0x0E2 |   0x3872 | `r8`     | ECC    |
// | 0x0E3 |   0x7542 | `Bu`     | ECC    | STRDEF offset 0x23 Len 0x0A = "Bus Pirate", Len 0x11 = "Bus Pirate 6 Rev 2"
// | 0x0E4 |   0x2073 | `s `     | ECC    |
// | 0x0E5 |   0x6950 | `Pi`     | ECC    |
// | 0x0E6 |   0x6172 | `ra`     | ECC    |
// | 0x0E7 |   0x6574 | `te`     | ECC    |


// | Row   | Data     | Chars    | Type   | Description ()
// |-------|----------|----------|--------|-------------------------------------------
// | 0x0E8 |   0x3620 | ` 6`     | ECC    |
// | 0x0E9 |   0x5220 | ` R`     | ECC    |
// | 0x0EA |   0x5645 | `EV`     | ECC    |
// | 0x0EB |   0x0032 | `2`<nul> | ECC    |
// | 0x0EC |   0x0000 |          | ECC    |
// | 0x0ED |   0x0000 |          | ECC    |
// | 0x0EE |   0x0000 |          | ECC    |
// | 0x0EF |   0x0000 |          | ECC    |
// | 0x0F0 |   0x0000 |          | ECC    |
// | 0x0F1 |   0x0000 |          | ECC    |
// | 0x0F2 |   0x0000 |          | ECC    |
// | 0x0F3 |   0x0000 |          | ECC    |
// | 0x0F4 |   0x3830 | `08`     | ECC    | STRDEF offset 0x34 Len 0x17 = 
// | 0x0F5 |   0x313A | `:1`     | ECC    | e.g., S/N 7F6E5D4C3B2A1908 --> "08:19:2A:3B:4C:5D:6E:7F" 
// | 0x0F6 |   0x3A39 | `9:`     | ECC    | e.g., S/N AD15221F44292F36 --> "36:2F:29:44:1F:22:15:AD"
// | 0x0F7 |   0x4132 | `2A`     | ECC    | e.g., S/N A74936336C64D158 --> "58:D1:64:6C:33:36:49:A7"
// | 0x0F8 |   0x.... | `:3`     | ECC    |
// | 0x0F9 |   0x.... | `B:`     | ECC    |
// | 0x0FA |   0x.... | `4C`     | ECC    |
// | 0x0FB |   0x.... | `:5`     | ECC    |
// | 0x0FC |   0x.... | `D:`     | ECC    |
// | 0x0FD |   0x.... | `6E`     | ECC    |
// | 0x0FE |   0x.... | `:7`     | ECC    |
// | 0x0FF |   0x.... | `F`<nul> | ECC    |

#pragma endregion // Expected Results

#pragma region    // static/const USB whitelabel data


static const BP_OTP_USB_BOOT_FLAGS usb_boot_flags = {
    .usb_vid_valid          = 1,
    .usb_pid_valid          = 1,
    .usb_bcd_valid          = 0,
    .usb_lang_id_valid      = 0,
    .usb_manufacturer_valid = 1,
    .usb_product_valid      = 1,
    .usb_serial_valid       = 0,
    .usb_max_power_valid    = 0,
    .volume_label_valid     = 1,
    .scsi_vendor_valid      = 1,
    .scsi_product_valid     = 1,
    .scsi_rev_valid         = 0,
    .redirect_url_valid     = 1,
    .redirect_name_valid    = 1,
    .info_uf2_model_valid   = 1,
    .info_uf2_boardid_valid = 1,
    // 6 bits are rfu
    .white_label_addr_valid = 1,
    // 1 bit rfu
    // 8 bits unused (only 24 bits per OTP row)
};

static const uint16_t _static_portion[] = {
    // offset 0x10, chars 0x16: "https://buspirate.com/"
    // offset 0x14, chars 0x0D:       "buspirate.com"
    0x7468, // `ht` // Row 0x0d0 -- offset 0x10
    0x7074, // `tp` // Row 0x0d1 -- offset 0x11
    0x3a73, // `s:` // Row 0x0d2 -- offset 0x12
    0x2f2f, // `//` // Row 0x0d3 -- offset 0x13
    0x7562, // `bu` // Row 0x0d4 -- offset 0x14
    0x7073, // `sp` // Row 0x0d5 -- offset 0x15
    0x7269, // `ir` // Row 0x0d6 -- offset 0x16
    0x7461, // `at` // Row 0x0d7 -- offset 0x17
    0x2e65, // `e.` // Row 0x0d8 -- offset 0x18
    0x6f63, // `co` // Row 0x0d9 -- offset 0x19
    0x2f6d, // `m/` // Row 0x0da -- offset 0x1a
    // offset 0x1B, chars 0x08: "BP__BOOT"
    0x5042, // `BP` // Row 0x0db -- offset 0x1b
    0x5f5f, // `__` // Row 0x0dc -- offset 0x1c
    0x4f42, // `BO` // Row 0x0dd -- offset 0x1d
    0x544f, // `OT` // Row 0x0de -- offset 0x1e
    // offset 0x1F, chars 0x08: "Bus Pir8"
    0x7542, // `Bu` // Row 0x0df -- offset 0x1f
    0x2073, // `s ` // Row 0x0e0 -- offset 0x20
    0x6950, // `Pi` // Row 0x0e1 -- offset 0x21
    0x3872, // `r8` // Row 0x0e2 -- offset 0x22
    // offset 0x23, chars 0x0A: "Bus Pirate"
    0x7542, // `Bu` // Row 0x0e3 -- offset 0x23
    0x2073, // `s ` // Row 0x0e4 -- offset 0x24
    0x6950, // `Pi` // Row 0x0e5 -- offset 0x25
    0x6172, // `ra` // Row 0x0e6 -- offset 0x26
    0x6574, // `te` // Row 0x0e7 -- offset 0x27
};
static_assert(ARRAY_SIZE(_static_portion) == 0x18u); // 24 rows
#define BP_OTP_ROW__STATIC_PORTION_OFFSET (sizeof(BP_OTP_WHITELABEL)/2u)


// NOTE: Reserved for board id string exposed in INFO_UF2.TXT file
//       Ian is choosing to use this to make it
//       easier to get the board serial number
//       formatted as follows:
//           "%02X:%02X:%02X:%02X:%02X:%02X:%02X:%02X"
//
//           01:23:45:67:89:AB:CD:EF
//           ....-....1....-....2... == 23 characters
//       With NULL terminator it would still fit within 12 rows...
// This is placed to be at the very end of the whitelabel data page
//
#define BP_OTP_ROW__BOARD_ID_STRING_MAX_ROWCOUNT  (0x0Cu) // 12 rows
#define BP_OTP_ROW__BOARD_ID_STRING_OFFSET        (0x40u - BP_OTP_ROW__BOARD_ID_STRING_MAX_ROWCOUNT)
#define BP_OTP_ROW__BOARD_ID_STRING_MAX_CHARCOUNT ((BP_OTP_ROW__BOARD_ID_STRING_MAX_ROWCOUNT * 2u) - 1u)

// NOTE: Reserved for product string, which is only portion
//       that changes length between boards
//       Due to string re-use, this must be appended after the static portion
#define BP_OTP_ROW__PRODUCT_VERSION_STRING_OFFSET ( \
    (sizeof(BP_OTP_WHITELABEL)/2u) +  \
    (sizeof(_static_portion)/2u)      \
)
#define BP_OTP_ROW__PRODUCT_VERSION_STRING_MAX_ROWCOUNT ( \
    BP_OTP_ROW__BOARD_ID_STRING_OFFSET -       \
    BP_OTP_ROW__PRODUCT_VERSION_STRING_OFFSET  \
)
#define BP_OTP_ROW__PRODUCT_VERSION_STRING_MAX_CHARCOUNT ((BP_OTP_ROW__PRODUCT_VERSION_STRING_MAX_ROWCOUNT * 2u) - 1u)

// These are the offsets ... put here as static asserts simply to help
// validate the structure of the OTP data
static_assert(BP_OTP_ROW__PRODUCT_VERSION_STRING_OFFSET        == 0x28u);
static_assert(BP_OTP_ROW__BOARD_ID_STRING_OFFSET               == 0x34u);
static_assert(BP_OTP_ROW__BOARD_ID_STRING_MAX_CHARCOUNT        == 0x17u);
static_assert(BP_OTP_ROW__PRODUCT_VERSION_STRING_MAX_CHARCOUNT == 0x17u);


// Verify that everything will fit into one page
// NOTE: If make these strings have unique addresses or even NULL termination,
//       then this WILL NOT fit into a single OTP page.  That'll be OK...
//       especially where those strings are used in at least one other place
//       in the firmware.
static_assert(
    (sizeof(BP_OTP_WHITELABEL)/2u)                      + // fixed-size whitelabel struct
    ARRAY_SIZE(_static_portion)                         + // static non-variable portion
    BP_OTP_ROW__PRODUCT_VERSION_STRING_MAX_ROWCOUNT     +
    BP_OTP_ROW__BOARD_ID_STRING_MAX_ROWCOUNT
    <= 0x40u,
    "Whitelabel data will not fit into a single OTP page"
);


static const char  _product_string[] = " " BP_OTP_PRODUCT_VERSION_STRING;
// note: using a string ensures the byte immediately following the last charcter is readable
//       which is important since writing ECC requires an even number of bytes.
static_assert(sizeof(char) == sizeof(uint8_t), "char must be 8-bits");
// Since the string includes NULL and whitelabel uses counted strings,
// can calculate the number of rows needed by simply dividing by 2 (rounds down).
static_assert(ARRAY_SIZE(_product_string)/2 <= BP_OTP_ROW__PRODUCT_VERSION_STRING_MAX_ROWCOUNT);
#pragma endregion // static/const USB whitelabel data

#pragma region    // Internal (static) helper functions
static uint16_t strdef_to_start_row(uint16_t base, BP_OTP_WHITELABEL_STRDEF strdef) {
    return base + strdef.row_offset;
}
static uint16_t strdef_to_next_unused_row(uint16_t base, BP_OTP_WHITELABEL_STRDEF strdef) {
    uint16_t result = base;
    if (strdef.is_unicode) {
        result += strdef.character_count; // each character is a full row
    } else {
        result += (strdef.character_count + 1) / 2u; // every two characters is a row ... rounding up
    }
    return result;
}
static inline __attribute__((always_inline)) uint16_t my_max_row(uint16_t a, uint16_t b) {
    return (a > b) ? a : b;
}

bool internal_get_whitelabel_row_range(BP_OTP_ROW_RANGE *result_out) {
    // NOTE: This is entirely read-only against the OTP fuses
    memset(result_out, 0, sizeof(BP_OTP_ROW_RANGE));
    
    BP_OTP_USB_BOOT_FLAGS current_usb_boot_flags;
    if (!bp_otp_read_redundant_rows_2_of_3(0x059, &current_usb_boot_flags.as_uint32)) {
        PRINT_ERROR("Whitelabel Error: Raw OTP rows 0x59..0x5B (USB BOOT FLAGS) did not find majority agreement\n");
        return false;
    }
    PRINT_DEBUG("Whitelabel Debug: found USB BOOT FLAGS: %06x\n", current_usb_boot_flags.as_uint32);

    uint16_t base;
    if (!bp_otp_read_single_row_ecc(0x05C, &base)) {
        PRINT_ERROR("Whitelabel Error: OTP row 0x05C could not be read (WHITE_LABEL_ADDR)\n");
        return false;
    }
    if (base < 0x0C0u || base > 0xEC0u) {
        PRINT_ERROR("Whitelabel Error: WHITE_LABEL_ADDR 0x%03X is out of range [0x0C0 .. 0xEC0]\n", base);
        return false;
    }
    PRINT_DEBUG("Whitelabel Debug: WHITE_LABEL_ADDR points to row index 0x%03x\n", base);

    uint16_t start_row        = base;
    uint16_t first_unused_row = base + sizeof(BP_OTP_WHITELABEL)/2u; // whitelabel structure (16 rows) is implied

    // Read in the whitelabel structure
    BP_OTP_WHITELABEL whitelabel = {0};
    if (!bp_otp_read_ecc_data(base, &whitelabel, sizeof(whitelabel))) {
        PRINT_ERROR("Whitelabel Error: Failed to read existing whitelabel data\n");
        return false;
    }

    // Now go through each STRDEF that's marked valid, calculate the row(s) it uses, and update the first_unused_row if needed
    if (current_usb_boot_flags.usb_manufacturer_valid == 1) { first_unused_row = my_max_row(first_unused_row, strdef_to_next_unused_row( base, whitelabel.usb_manufacturer        )); }
    if (current_usb_boot_flags.usb_product_valid      == 1) { first_unused_row = my_max_row(first_unused_row, strdef_to_next_unused_row( base, whitelabel.usb_product             )); }
    if (current_usb_boot_flags.usb_serial_valid       == 1) { first_unused_row = my_max_row(first_unused_row, strdef_to_next_unused_row( base, whitelabel.usb_serial              )); }
    if (current_usb_boot_flags.volume_label_valid     == 1) { first_unused_row = my_max_row(first_unused_row, strdef_to_next_unused_row( base, whitelabel.storage_volume_label    )); }
    if (current_usb_boot_flags.scsi_vendor_valid      == 1) { first_unused_row = my_max_row(first_unused_row, strdef_to_next_unused_row( base, whitelabel.scsi_vid                )); }
    if (current_usb_boot_flags.scsi_product_valid     == 1) { first_unused_row = my_max_row(first_unused_row, strdef_to_next_unused_row( base, whitelabel.scsi_pid                )); }
    if (current_usb_boot_flags.scsi_rev_valid         == 1) { first_unused_row = my_max_row(first_unused_row, strdef_to_next_unused_row( base, whitelabel.scsi_rev                )); }
    if (current_usb_boot_flags.redirect_url_valid     == 1) { first_unused_row = my_max_row(first_unused_row, strdef_to_next_unused_row( base, whitelabel.index_htm_redirect_url  )); }
    if (current_usb_boot_flags.redirect_name_valid    == 1) { first_unused_row = my_max_row(first_unused_row, strdef_to_next_unused_row( base, whitelabel.index_htm_redirect_name )); }
    if (current_usb_boot_flags.info_uf2_model_valid   == 1) { first_unused_row = my_max_row(first_unused_row, strdef_to_next_unused_row( base, whitelabel.info_uf2_model          )); }
    if (current_usb_boot_flags.info_uf2_boardid_valid == 1) { first_unused_row = my_max_row(first_unused_row, strdef_to_next_unused_row( base, whitelabel.info_uf2_boardid        )); }

    // return the results
    result_out->start_row = start_row;
    result_out->row_count = first_unused_row - start_row;
    return true;
}
#pragma endregion // Internal helper functions


// Restartable whitelabel process ... controlled via RTT (no USB connection required)
void bp_otp_apply_whitelabel_data(void) {

    // // Uncomment next line to single-step (using RTT for input)
    // g_WaitForKey = true;

    static const uint16_t base = 0x0c0; // written so this can be changed easily
    static const size_t product_extension_rows = sizeof(_product_string) / 2u; // sizeof() includes null; rounds down to even number
    uint16_t product_char_count = strlen("Bus Pirate") + strlen(_product_string);
    
    // 1. write the static portion         --> Rows 0x0d0 .. 0x0e7
    for (uint16_t i = 0; i < ARRAY_SIZE(_static_portion); ++i) {
        uint16_t row = base + BP_OTP_ROW__STATIC_PORTION_OFFSET +  i;
        PRINT_DEBUG("Whitelabel Debug: Write static portion index %d: row 0x%03x, data 0x%04x\n", i, row, _static_portion[i]);
        WAIT_FOR_KEY();
        bp_otp_write_single_row_ecc(row, _static_portion[i]) || DIE();
    }
    PRINT_DEBUG("Whitelabel Debug: Version extension: '%s'\n", _product_string);

    // 2. also write the product version extension ... appends to the "Bus Pirate" string at the end of the static portion
    for (uint16_t i = 0; i < product_extension_rows; ++i) {
        uint16_t row = base + BP_OTP_ROW__PRODUCT_VERSION_STRING_OFFSET + i;
        uint16_t data = 0;
        // First  character is stored in LSB
        // Second character is stored in MSB
        data  |= (uint8_t)(_product_string[2u * i + 1]);
        data <<= 8;
        data  |= (uint8_t)(_product_string[2u * i    ]);

        PRINT_DEBUG("Whitelabel Debug: Write product version extension index %d: row 0x%03x, data 0x%04x\n", i, row, data);
        WAIT_FOR_KEY();
        bp_otp_write_single_row_ecc(row, data) || DIE();
    }

    // 3. write the INFO_UF2.TXT board id
    if (true) {
        uint8_t board_id_as_string[BP_OTP_ROW__BOARD_ID_STRING_MAX_CHARCOUNT + 1u] = { 0 };
        pico_unique_board_id_t id;
        pico_get_unique_board_id(&id); 
        //convert the unique ID to ANSI string
        snprintf(
            board_id_as_string, sizeof(board_id_as_string),
            "%02X:%02X:%02X:%02X:%02X:%02X:%02X:%02X",
            id.id[0], id.id[1], id.id[2], id.id[3], id.id[4], id.id[5], id.id[6], id.id[7]
        );

        PRINT_DEBUG("Whitelabel Debug: Manufacturing ID string: %s\r\n", board_id_as_string);
        WAIT_FOR_KEY();

        // Write the actual string's data
        size_t row_count = (strlen(board_id_as_string)+1) / 2u;
        for (size_t i = 0; i < row_count; ++i) {
            uint16_t row = base + BP_OTP_ROW__BOARD_ID_STRING_OFFSET + i;
            uint16_t data = 0;
            // First  character is stored in LSB
            // Second character is stored in MSB
            data  |= (uint8_t)(board_id_as_string[2u * i + 1]);
            data <<= 8;
            data  |= (uint8_t)(board_id_as_string[2u * i    ]);

            PRINT_DEBUG("Whitelabel Debug: Writing row 0x%03x: 0x%04x (`%c%c`)\n", row, data, byte_to_printable_char(data & 0xFFu), byte_to_printable_char(data >> 8)); WAIT_FOR_KEY();
            bp_otp_write_single_row_ecc(row, data) || DIE();
        }
        


    }

    // NOTE: MUST UPDATE TO WRITE THE INFO_UF2.TXT BOARD ID

    // 4. encode the product string length and offset
    BP_OTP_WHITELABEL_STRDEF product_revision_strdef = {
        .character_count = product_char_count,
        .is_unicode      = 0,
        .row_offset      = 0x23u, // NOTE: This is not the offset defined above, as it re-uses the USB manufacturer as a prefix
    };
    uint16_t tmp_p = product_revision_strdef.as_uint16;

    // 4. Write the portions of the first 16 rows that have valid data:
    PRINT_DEBUG("Whitelabel Debug: Write WHITELABEL index 0: row 0x%03x\n", base + 0x0u);  WAIT_FOR_KEY();  bp_otp_write_single_row_ecc(base + 0x0u, 0x1209u) || DIE();  // USB VID == 0x1209
    PRINT_DEBUG("Whitelabel Debug: Write WHITELABEL index 1: row 0x%03x\n", base + 0x1u);  WAIT_FOR_KEY();  bp_otp_write_single_row_ecc(base + 0x1u, 0x7332u) || DIE();  // USB PID == 0x7332
    //INT_DEBUG("Whitelabel Debug: Write WHITELABEL index 2: row 0x%03x\n", base + 0x2u);  WAIT_FOR_KEY();  bp_otp_write_single_row_ecc(base + 0x2u, 0x....u) || DIE();  // USB BCD Device
    //INT_DEBUG("Whitelabel Debug: Write WHITELABEL index 3: row 0x%03x\n", base + 0x3u);  WAIT_FOR_KEY();  bp_otp_write_single_row_ecc(base + 0x3u, 0x....u) || DIE();  // USB LangID for strings
    PRINT_DEBUG("Whitelabel Debug: Write WHITELABEL index 4: row 0x%03x\n", base + 0x4u);  WAIT_FOR_KEY();  bp_otp_write_single_row_ecc(base + 0x4u, 0x230Au) || DIE();  // USB MANU              ten   chars @ offset 0x23
    PRINT_DEBUG("Whitelabel Debug: Write WHITELABEL index 5: row 0x%03x\n", base + 0x5u);  WAIT_FOR_KEY();  bp_otp_write_single_row_ecc(base + 0x5u, tmp_p  ) || DIE();  // USB PROD              XXX   chars @ offset 0x23
    //INT_DEBUG("Whitelabel Debug: Write WHITELABEL index 6: row 0x%03x\n", base + 0x6u);  WAIT_FOR_KEY();  bp_otp_write_single_row_ecc(base + 0x6u, 0x....u) || DIE();  // USB Serial Number
    //INT_DEBUG("Whitelabel Debug: Write WHITELABEL index 7: row 0x%03x\n", base + 0x7u);  WAIT_FOR_KEY();  bp_otp_write_single_row_ecc(base + 0x7u, 0x....u) || DIE();  // USB config attributes & max power
    PRINT_DEBUG("Whitelabel Debug: Write WHITELABEL index 8: row 0x%03x\n", base + 0x8u);  WAIT_FOR_KEY();  bp_otp_write_single_row_ecc(base + 0x8u, 0x1B08u) || DIE();  // STOR VOLUME LABEL     eight chars @ offset 0x1b
    PRINT_DEBUG("Whitelabel Debug: Write WHITELABEL index 9: row 0x%03x\n", base + 0x9u);  WAIT_FOR_KEY();  bp_otp_write_single_row_ecc(base + 0x9u, 0x1F08u) || DIE();  // SCSI INQUIRY VENDOR   eight chars @ offset 0x1f
    PRINT_DEBUG("Whitelabel Debug: Write WHITELABEL index A: row 0x%03x\n", base + 0xAu);  WAIT_FOR_KEY();  bp_otp_write_single_row_ecc(base + 0xAu, tmp_p  ) || DIE();  // SCSI INQUIRY PRODUCT  XXX   chars @ offset 0x23
    PRINT_DEBUG("Whitelabel Debug: Write WHITELABEL index C: row 0x%03x\n", base + 0xCu);  WAIT_FOR_KEY();  bp_otp_write_single_row_ecc(base + 0xCu, 0x1016u) || DIE();  // redirect URL          22    chars @ offset 0x10
    PRINT_DEBUG("Whitelabel Debug: Write WHITELABEL index D: row 0x%03x\n", base + 0xDu);  WAIT_FOR_KEY();  bp_otp_write_single_row_ecc(base + 0xDu, 0x140du) || DIE();  // redirect name         13    chars @ offset 0x14
    PRINT_DEBUG("Whitelabel Debug: Write WHITELABEL index E: row 0x%03x\n", base + 0xEu);  WAIT_FOR_KEY();  bp_otp_write_single_row_ecc(base + 0xEu, tmp_p  ) || DIE();  // info_uf2.txt product  XXX   chars @ offset 0x23
    PRINT_DEBUG("Whitelabel Debug: Write WHITELABEL index F: row 0x%03x\n", base + 0xFu);  WAIT_FOR_KEY();  bp_otp_write_single_row_ecc(base + 0xFu, 0x3417u) || DIE();  // info_uf2.txt board ID 23    chars @ offset 0x34

    // 5. write the `WHITE_LABEL_ADDR` to point to the base address used here
    // NOTE: this is a fixed OTP row ... only get one shot to write this, so ensuring the above values are correctly written first is important!
    PRINT_DEBUG("Whitelabel Debug: Write WHITELABEL_BASE_ADDR 0x%03x to row 0x05C\n", base); WAIT_FOR_KEY();
    bp_otp_write_single_row_ecc(0x05c, base) || DIE();

    // 6. Read the existing USB boot flags ... stored RBIT-3 (three rows, majority voting per bit)
    BP_OTP_USB_BOOT_FLAGS old_usb_boot_flags;
    if (!bp_otp_read_redundant_rows_2_of_3(0x059, &old_usb_boot_flags.as_uint32)) {
        PRINT_ERROR("Whitelabel Error: Raw OTP rows 0x59..0x5B (old USB BOOT FLAGS) did not find majority agreement\n");
        return;
    }
    if ((old_usb_boot_flags.as_uint32 & usb_boot_flags.as_uint32) == usb_boot_flags.as_uint32) {
        // No need to write ... all the requested flags are already set
        PRINT_INFO("Whitelabel Info: USB BOOT FLAGS already set\n");
        return;
    }
    if ((old_usb_boot_flags.as_uint32 & ~usb_boot_flags.as_uint32) != 0) {
        PRINT_WARNING("Whitelabel Warning: Old USB BOOT FLAGS 0x%06x vs. intended %06x has additional bits: %06x\n",
            old_usb_boot_flags.as_uint32, usb_boot_flags.as_uint32,
            old_usb_boot_flags.as_uint32 & ~usb_boot_flags.as_uint32
        );
        // continue anyways? YES ... it just means there's some additional valid data
    }

    // 7. NON-ECC write the USB boot flags ... RBIT-3 encoding
    BP_OTP_USB_BOOT_FLAGS new_flags = { .as_uint32 = old_usb_boot_flags.as_uint32 | usb_boot_flags.as_uint32 };
    PRINT_DEBUG("Whitelabel Debug: Writing USB BOOT FLAGS: 0x%06x -> 0x%06x\n", old_usb_boot_flags.as_uint32, new_flags);  WAIT_FOR_KEY();
    if (!bp_otp_write_redundant_rows_2_of_3(0x059, new_flags.as_uint32)) {
        PRINT_ERROR("Whitelabel Error: Failed to write USB BOOT FLAGS\n");
        return;
    }

    PRINT_INFO("Whitelabel Info: USB BOOT FLAGS updated\n"); WAIT_FOR_KEY();
    return;

    // That's it!
}
bool bp_otp_lock_whitelabel(void) {

    // Which pages do we expect to have the whitelabel data?
    // NOTE: See TODO near end of function ... if expect_* variables change, so will that area of code
    const uint16_t expect_start_row      = 0x0C0;
    const uint16_t expect_used_row_count = 0x040;

    // // Uncomment next line to single-step (using RTT for input)
    // g_WaitForKey = true;

    BP_OTP_USB_BOOT_FLAGS old_usb_boot_flags;
    if (!bp_otp_read_redundant_rows_2_of_3(0x059, &old_usb_boot_flags.as_uint32)) {
        PRINT_ERROR("Whitelabel Error: Raw OTP rows 0x59..0x5B (USB BOOT FLAGS) did not find majority agreement\n");
        return false;
    }
    PRINT_DEBUG("Whitelabel Debug: found USB BOOT FLAGS: %06x\n", old_usb_boot_flags.as_uint32);

    if ((old_usb_boot_flags.as_uint32 & usb_boot_flags.as_uint32) != usb_boot_flags.as_uint32) {
        PRINT_ERROR("Whitelabel Error: USB BOOT FLAGS 0x%06x does not have all required bits set (0x%06x)\n", old_usb_boot_flags.as_uint32, usb_boot_flags.as_uint32);
        return false;
    }

    BP_OTP_ROW_RANGE whitelabel_range = {0};
    if (!internal_get_whitelabel_row_range(&whitelabel_range)) {
        PRINT_ERROR("Whitelabel Error: Failed to get whitelabel row range\n");
        return false;
    }

    if (whitelabel_range.start_row != expect_start_row) {
        PRINT_ERROR("Whitelabel Error: Whitelabel data does not start at expected row: 0x%03X != 0x%03X\n", whitelabel_range.start_row, expect_start_row);
        return false;
    }
    if (whitelabel_range.row_count > expect_used_row_count) {
        PRINT_ERROR("Whitelabel Error: Whitelabel data uses more than expected row count: 0x%03X > 0x%03X\n", whitelabel_range.row_count, expect_used_row_count);
        return false;
    }

    const uint8_t LOCK0_VALUE = 0x3Fu; // NO_KEY_STATE: 0b00, KEY_R: 0b111, KEY_W: 0b111 (no key; read-only without key)
    const uint8_t LOCK1_VALUE = 0x15u; // LOCK_S: 0b01, LOCK_NS: 0b01, LOCK_BL: 0b01 (read-only for all)


    // NOTE: CURRENTLY HARD-CODED TO LOCK OTP ROW RANGE 0x0C0 .. 0x0FF
    // TODO: allow locking different page
    // TODO: allow locking more than one page
    PRINT_WARNING("Whitelabel Warning: Locking OTP rows 0x0C0 .. 0x0FF\n");

    // TODO: is there an SDK API for locking pages?
    PRINT_DEBUG("Setting PAGE3_LOCK0 (0xF86) to `0x3Fu` (no keys; read-only without key)\n"); WAIT_FOR_KEY();
    if (!bp_otp_write_single_row_byte3x(0xF86, 0x3Fu)) {
        PRINT_ERROR("Whitelabel Error: Failed to write PAGE3_LOCK0\n");
        return false;
    }
    PRINT_DEBUG("Whitelabel Debug: Setting PAGE3_LOCK1 (0xF87) to `0x15` (read-only for secure, non-secure, bootloader)\n"); WAIT_FOR_KEY();
    if (!bp_otp_write_single_row_byte3x(0xF87, 0x15u)) {
        PRINT_ERROR("Whitelabel Error: Failed to write PAGE3_LOCK1\n");
        return false;
    }
    PRINT_DEBUG("Whitelabel Debug: Board ID string successfully applied and locked down.\n");
    return true;
}

