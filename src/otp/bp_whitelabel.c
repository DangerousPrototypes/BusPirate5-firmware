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


// TODO: update this to wait for actual keypresses over RTT (as in the earlier experimentation firmware)
//       to allow for review of all the things happening....
#define WAIT_FOR_KEY()
// #define WAIT_FOR_KEY() MyWaitForAnyKey_with_discards()

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

#define DIE() die(__LINE__)

// define as returning bool to allow:
//     funcA() || DIE();
// Otherwise, compiler complains because of using void in a boolean context.
static bool __attribute__((noreturn)) die(int line) {
    PRINT_ERROR("Whitelabel Error: Aborting whitelabel process @ line %d\n");
    hard_assert(false);
    while (1);
}


static char byte_to_printable_char(uint8_t byte) {
    if (byte < 0x20u) {
        return '.';
    }
    if (byte > 0x7Eu) {
        return '.';
    }
    return byte;
}

typedef struct _OTP_USB_BOOT_FLAGS {
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
} OTP_USB_BOOT_FLAGS;

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
static_assert(ARRAY_SIZE(_static_portion) == 24);

// NOTE: Reserved for product string:
//                  // Rows 0x0e8 .. 0x0ef (8 rows == space character + 15 additional characters maximum)
#define OTP_ROW__PRODUCT_VERSION_STRING_OFFSET 0x28u
#define OTP_ROW__PRODUCT_VERSION_STRING_MAX_ROWCOUNT 8u

// NOTE: Reserved for manufacturing data:
//                  // Rows 0x0f0 .. 0x0ff (16 rows == 32 characters maximum)
#define OTP_ROW__MANUFACTURING_DATA_STRING_OFFSET 0x30u
#define OTP_ROW__MANUFACTURING_DATA_STRING_MAX_ROWCOUNT 16u

// Verify that everything will fit into one page
static_assert(
    0x10u + // fixed-size whitelabel struct
    ARRAY_SIZE(_static_portion) +
    OTP_ROW__PRODUCT_VERSION_STRING_MAX_ROWCOUNT +
    OTP_ROW__MANUFACTURING_DATA_STRING_MAX_ROWCOUNT
    <= 0x40u,
    "Whitelabel data will not fit into a single OTP page"
);


static const char  _product_string[] = " " BP_OTP_PRODUCT_VERSION_STRING;
// note: using a string ensures the byte immediately following the last charcter is readable
//       which is important since writing ECC requires an even number of bytes.
static_assert(sizeof(char) == sizeof(uint8_t), "char must be 8-bits");
// Since the string includes NULL and whitelabel uses counted strings,
// can calculate the number of rows needed by simply dividing by 2 (rounds down).
static_assert(ARRAY_SIZE(_product_string)/2 <= OTP_ROW__PRODUCT_VERSION_STRING_MAX_ROWCOUNT);

// Leaving the manufacturing data portion uncoded...
// That will be filled in by another process
#define USB_WHITELABEL_MAX_CHARS_BP_VERSION (OTP_ROW__PRODUCT_VERSION_STRING_MAX_ROWCOUNT*2u)
#define USB_WHITELABEL_MAX_CHARS_BP_MANU    (OTP_ROW__MANUFACTURING_DATA_STRING_MAX_ROWCOUNT*2u)


// Restartable whitelabel process ... controlled via RTT (no USB connection required)
void bp_otp_apply_whitelabel_data(void) {

    // // Uncomment next line to single-step (using RTT for input)
    // g_WaitForKey = true;

    static const uint16_t base = 0x0c0; // written so this can be changed easily
    static const size_t product_extension_rows = sizeof(_product_string) /2u; // sizeof() includes null; round down to even number
    uint16_t product_char_count = strlen("Bus Pirate") + strlen(_product_string);
    
    // 1. write the static portion         --> Rows 0x0d0 .. 0x0e7
    for (uint16_t i = 0; i < ARRAY_SIZE(_static_portion); ++i) {
        uint16_t row = base + 0x10u +  i;
        PRINT_DEBUG("Whitelabel Debug: Write static portion index %d: row 0x%03x, data 0x%04x\n", i, row, _static_portion[i]);
        WAIT_FOR_KEY();
        bp_otp_write_single_row_ecc(row, _static_portion[i]) || DIE();
    }
    PRINT_DEBUG("Whitelabel Debug: Version extension: '%s'\n", _product_string);
    // 2. also write the product version extension
    for (uint16_t i = 0; i < product_extension_rows; ++i) {
        uint16_t row = base + 0x28u + i;
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

    // NOTE: DOES NOT WRITE THE MANUFACTURING DATA PORTION

    // 3. encode the product string length and offset
    uint16_t tmp_p = 0x2300 | product_char_count;

    // 4. Write the portions of the first 16 rows that have valid data:
    PRINT_DEBUG("Whitelabel Debug: Write WHITELABEL index 0: row 0x%03x\n", base + 0x0u);   WAIT_FOR_KEY(); bp_otp_write_single_row_ecc(base + 0x0u, 0x1209u) || DIE();   // USB VID == 0x1209
    PRINT_DEBUG("Whitelabel Debug: Write WHITELABEL index 1: row 0x%03x\n", base + 0x1u);   WAIT_FOR_KEY(); bp_otp_write_single_row_ecc(base + 0x1u, 0x7332u) || DIE();   // USB PID == 0x7332
    //PRINT_DEBUG("Whitelabel Debug: Write WHITELABEL index 2: row 0x%03x\n", base + 0x2u); WAIT_FOR_KEY(); bp_otp_write_single_row_ecc(base + 0x2u, 0x....u) || DIE();   // USB BCD Device
    //PRINT_DEBUG("Whitelabel Debug: Write WHITELABEL index 3: row 0x%03x\n", base + 0x3u); WAIT_FOR_KEY(); bp_otp_write_single_row_ecc(base + 0x3u, 0x....u) || DIE();   // USB LangID for strings
    PRINT_DEBUG("Whitelabel Debug: Write WHITELABEL index 4: row 0x%03x\n", base + 0x4u);   WAIT_FOR_KEY(); bp_otp_write_single_row_ecc(base + 0x4u, 0x230Au) || DIE();   // USB MANU              ten   chars @ offset 0x23
    PRINT_DEBUG("Whitelabel Debug: Write WHITELABEL index 5: row 0x%03x\n", base + 0x5u);   WAIT_FOR_KEY(); bp_otp_write_single_row_ecc(base + 0x5u, tmp_p  ) || DIE();   // USB PROD              XXX   chars @ offset 0x23
    //PRINT_DEBUG("Whitelabel Debug: Write WHITELABEL index 6: row 0x%03x\n", base + 0x6u); WAIT_FOR_KEY(); bp_otp_write_single_row_ecc(base + 0x6u, 0x....u) || DIE();   // USB Serial Number
    //PRINT_DEBUG("Whitelabel Debug: Write WHITELABEL index 7: row 0x%03x\n", base + 0x7u); WAIT_FOR_KEY(); bp_otp_write_single_row_ecc(base + 0x7u, 0x....u) || DIE();   // USB config attributes & max power
    PRINT_DEBUG("Whitelabel Debug: Write WHITELABEL index 8: row 0x%03x\n", base + 0x8u);   WAIT_FOR_KEY(); bp_otp_write_single_row_ecc(base + 0x8u, 0x1B08u) || DIE();   // STOR VOLUME LABEL     eight chars @ offset 0x1b
    PRINT_DEBUG("Whitelabel Debug: Write WHITELABEL index 9: row 0x%03x\n", base + 0x9u);   WAIT_FOR_KEY(); bp_otp_write_single_row_ecc(base + 0x9u, 0x1F08u) || DIE();   // SCSI INQUIRY VENDOR   eight chars @ offset 0x1f
    PRINT_DEBUG("Whitelabel Debug: Write WHITELABEL index A: row 0x%03x\n", base + 0xAu);   WAIT_FOR_KEY(); bp_otp_write_single_row_ecc(base + 0xAu, tmp_p  ) || DIE();   // SCSI INQUIRY PRODUCT  XXX   chars @ offset 0x23
    PRINT_DEBUG("Whitelabel Debug: Write WHITELABEL index C: row 0x%03x\n", base + 0xCu);   WAIT_FOR_KEY(); bp_otp_write_single_row_ecc(base + 0xCu, 0x1016u) || DIE();   // redirect URL          22    chars @ offset 0x10
    PRINT_DEBUG("Whitelabel Debug: Write WHITELABEL index D: row 0x%03x\n", base + 0xDu);   WAIT_FOR_KEY(); bp_otp_write_single_row_ecc(base + 0xDu, 0x140du) || DIE();   // redirect name         13    chars @ offset 0x14
    PRINT_DEBUG("Whitelabel Debug: Write WHITELABEL index E: row 0x%03x\n", base + 0xEu);   WAIT_FOR_KEY(); bp_otp_write_single_row_ecc(base + 0xEu, tmp_p  ) || DIE();   // info_uf2.txt product  XXX   chars @ offset 0x23
    //PRINT_DEBUG("Whitelabel Debug: Write WHITELABEL index F: row 0x%03x\n", base + 0xFu); WAIT_FOR_KEY(); bp_otp_write_single_row_ecc(base + 0xFu, 0x....u) || DIE();   // info_uf2.txt manufacturing string ... to be added later

    // 5. write the `WHITE_LABEL_ADDR` to point to the base address used here
    // NOTE: this is a fixed OTP row ... only get one shot to write this, so ensuring the above values are correctly written first is important!
    PRINT_DEBUG("Whitelabel Debug: Write WHITELABEL_BASE_ADDR 0x%03x to row 0x05C\n", base); WAIT_FOR_KEY();
    bp_otp_write_single_row_ecc(0x05c, base) || DIE();

    // 6. NON-ECC write the USB boot flags ... writes go to three (3) consecutive rows
    // NOTE: Flags can be migrated from 0 --> 1 later.  This is used to allow manufacturing data to be written later, for example.
    OTP_USB_BOOT_FLAGS usb_boot_flags = {
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
        .info_uf2_boardid_valid = 0,
        .white_label_addr_valid = 1,
    };
    hard_assert(usb_boot_flags.as_uint32 == 0x407733u); // manually calculated ... should match the friendly description above.
    
    // 7. Read the existing USB boot flags
    // NOTE: This is a fixed set of OTP rows, stored redundantly using 2-of-3 voting
    OTP_USB_BOOT_FLAGS old_usb_boot_flags;
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
        // continue anyways?  This allow the code to run even after manufacturing string has been applied....
    }
    OTP_USB_BOOT_FLAGS new_flags = { .as_uint32 = old_usb_boot_flags.as_uint32 | usb_boot_flags.as_uint32 };

    PRINT_DEBUG("Whitelabel Debug: Writing USB BOOT FLAGS: 0x%06x -> 0x%06x\n", old_usb_boot_flags.as_uint32, new_flags);  WAIT_FOR_KEY();
    if (!bp_otp_write_redundant_rows_2_of_3(0x059, new_flags.as_uint32)) {
        PRINT_ERROR("Whitelabel Error: Failed to write USB BOOT FLAGS\n");
        return;
    }

    PRINT_INFO("Whitelabel Info: USB BOOT FLAGS updated\n");
    return;

    // That's it!
}


bool bp_otp_apply_manufacturing_string(const char* manufacturing_data_string) {

    // // Uncomment next line to single-step (using RTT for input)
    // g_WaitForKey = true;

    uint16_t base;
    // Follow the breadcrumbs to find the base address
    OTP_USB_BOOT_FLAGS old_usb_boot_flags;
    if (!bp_otp_read_redundant_rows_2_of_3(0x059, &old_usb_boot_flags.as_uint32)) {
        PRINT_ERROR("Whitelabel Error: Raw OTP rows 0x59..0x5B (USB BOOT FLAGS) did not find majority agreement\n");
        return false;
    }
    PRINT_DEBUG("Whitelabel Debug: found USB BOOT FLAGS: %06x\n", old_usb_boot_flags.as_uint32);

    // validate the USB BOOT FLAGS?
    if (!old_usb_boot_flags.white_label_addr_valid) {
        PRINT_ERROR("Whitelabel Error: No white label data to update (%06x)\n", old_usb_boot_flags.as_uint32);
        return false;
    }
    if (old_usb_boot_flags.info_uf2_boardid_valid) {
        PRINT_DEBUG("Whitelabel Debug: Manufacturing string already set ... exiting\n");
        return true;
    }

    if (!bp_otp_read_single_row_ecc(0x05C, &base)) {
        PRINT_ERROR("Whitelabel Error: OTP row 0x5C could not be read (WHITE_LABEL_ADDR)\n");
        return false;
    }
    if (base != 0x0C0) {
        // TODO: other validation as needed
        PRINT_ERROR("Whitelabel Error: OTP row 0x5C (WHITE_LABEL_ADDR) is not set to 0x0C0, but 0x%03x\n", base);
        return false;
    }
    PRINT_DEBUG("Whitelabel Debug: WHITE_LABEL_ADDR points to row index 0x%03x\n", base);

    // Validate the static portion of the strings matches
    // That static portion starts at offset 0x10 from the base whitelabel structure.
    uint16_t _written_static_portion[ARRAY_SIZE(_static_portion)];
    if (!bp_otp_read_ecc_data(base+0x10, _written_static_portion, sizeof(_written_static_portion))) {
        PRINT_ERROR("Whitelabel Error: Failed to read static portion of white label data at row 0x%03x (0x%03x)\n", base, base+0x10);
        return false;
    }
    if (memcmp(_written_static_portion, _static_portion, sizeof(_static_portion)) != 0) {
        PRINT_ERROR("Whitelabel Error: Static portion of white label data at row 0x%03x (0x%03x) does not match\n", base, base+0x10);
        return false;
    }

    PRINT_DEBUG("Whitelabel Debug: Static portion of the strings look ok\n");

    size_t char_count = strlen(manufacturing_data_string);
    if (char_count > USB_WHITELABEL_MAX_CHARS_BP_MANU) {
        PRINT_ERROR("Whitelabel Error: Manufacturing data string is too long (%d chars > %d chars)\n", char_count, USB_WHITELABEL_MAX_CHARS_BP_MANU);
        return false;
    }

    // create the STRDEF value for the manufacturing data.
    // It currently goes to a fixed offset from the base address.
    uint16_t strdef = OTP_ROW__MANUFACTURING_DATA_STRING_OFFSET;
    strdef <<= 8;
    strdef |= char_count;

    // check if the STRDEF is already set ... if it is, verify it matches the current provided string's length
    uint16_t oldstrdef;
    if (bp_otp_read_single_row_ecc(base + 0xFu, &oldstrdef)) {
        PRINT_ERROR("Whitelabel Error: Failed to read prior manufacturing STRDEF\n");
        return false;
    } else if ((oldstrdef != 0u) && (oldstrdef != strdef)) {
        PRINT_ERROR("Whitelabel Error: Old manufacturing STRDEF (0x%06x) does not match new (0x%06x)\n", oldstrdef, strdef);
        return false;
    }

    PRINT_DEBUG("Whitelabel Debug: Writing the manufacturing string data\n"); WAIT_FOR_KEY();

    // first write the actual string's data
    for (size_t i = 0; i < (char_count+1)/2; ++i) {
        uint16_t row = base + OTP_ROW__MANUFACTURING_DATA_STRING_OFFSET + i;
        uint16_t data = 0;
        // First  character is stored in LSB
        // Second character is stored in MSB
        data  |= (uint8_t)(manufacturing_data_string[2u * i + 1]);
        data <<= 8;
        data  |= (uint8_t)(manufacturing_data_string[2u * i    ]);

        PRINT_DEBUG("Whitelabel Debug: Writing row 0x%03x: 0x%04x (`%c%c`)\n", row, data, byte_to_printable_char(data & 0xFFu), byte_to_printable_char(data >> 8)); WAIT_FOR_KEY();
        if (!bp_otp_write_single_row_ecc(row, data)) {
            PRINT_ERROR("Whitelabel Error: Failed with partial manufacturing string data written.\n");
            return false;
        }
    }

    PRINT_DEBUG("Whitelabel Debug: Writing row 0x%03x: STRDEF %04x\n", base + 0xFu, strdef); WAIT_FOR_KEY();
    if (!bp_otp_write_single_row_ecc(base + 0xFu, strdef)) {
        PRINT_ERROR("Whitelabel Error: Failed to write manufacturing string length and offset\n");
    }

    PRINT_DEBUG("Whitelabel Debug: Updating the USB_BOOT_FLAGS to mark the manufacturing string as valid\n");

    OTP_USB_BOOT_FLAGS new_usb_boot_flags = { .as_uint32 = old_usb_boot_flags.as_uint32 };
    new_usb_boot_flags.info_uf2_boardid_valid = 1;

    PRINT_DEBUG("Whitelabel Debug: Writing rows 0x059..0x05B: 0x%06x --> 0x%06x\n", old_usb_boot_flags.as_uint32, new_usb_boot_flags.as_uint32); WAIT_FOR_KEY();
    if (!bp_otp_write_redundant_rows_2_of_3(0x059, new_usb_boot_flags.as_uint32)) {
        PRINT_ERROR("Whitelabel Error: Failed to update USB BOOT FLAGs\n");
        return false;
    }

    if (base != 0x0c0) {
        // Currently hard-coded the mapping from pages to PAGEn_LOCK0 / PAGEn_LOCK1
        PRINT_ERROR("Whitelabel Error: Not yet supporting locking of OTP page unless base address is 0x0C0\n");
        return false;
    }

    // Finally, update page protections for page 3 (`0x0c0 .. 0x0ff`).
    // Row `0xF86`, PAGE3_LOCK0 = `0x3F3F3F` = `0b00'111'111`  : NO_KEY_STATE: 0b00, KEY_R: 0b111, KEY_W: 0b111 (no key; read-only without key)
    // Row `0xF87`, PAGE3_LOCK1 = `0x151515` = `0b00'01'01'01` : LOCK_S: 0b01, LOCK_NS: 0b01, LOCK_BL: 0b01 (read-only for all)
    PRINT_DEBUG("Whitelabel Debug: Setting PAGE3_LOCK0 (0xF86) to `0x3Fu` (no keys; read-only without key)\n"); WAIT_FOR_KEY();
    if (!bp_otp_write_single_row_byte3x(0xF86, 0x3Fu)) {
        PRINT_ERROR("Whitelabel Error: Failed to write PAGE3_LOCK0\n");
        return false;
    }
    PRINT_DEBUG("Whitelabel Debug: Setting PAGE3_LOCK1 (0xF87) to `0x151515` (read-only for all three)\n"); WAIT_FOR_KEY();
    if (!bp_otp_write_single_row_byte3x(0xF87, 0x15u)) {
        PRINT_ERROR("Whitelabel Error: Failed to write PAGE3_LOCK1\n");
        return false;
    }

    PRINT_DEBUG("Whitelabel Debug: Manufacturing string successfully applied and locked down.\n");
    return true;
}



                 