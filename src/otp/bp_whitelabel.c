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

#define WHITELABEL_DEBUG(...)
#define WAIT_FOR_KEY()
//#define WHITELABEL_DEBUG(...) SEGGER_RTT_printf(0, __VA_ARGS__)
//#define WHITELABEL_DEBUG(...) printf(__VA_ARGS__);


#define USB_WHITELABEL_MAX_CHARS_BP_VERSION 8
#define USB_WHITELABEL_MAX_CHARS_BP_MANU    40


static char byte_to_printable_char(uint8_t byte) {
    if (byte < 0x20u) {
        return '.';
    }
    if (byte > 0x7Eu) {
        return '.';
    }
    return byte;
}

/* GCC is awesome. */
#define ARRAY_SIZE(arr) \
    (sizeof(arr) / sizeof((arr)[0]) \
     + sizeof(typeof(int[1 - 2 * \
           !!__builtin_types_compatible_p(typeof(arr), \
                 typeof(&arr[0]))])) * 0)


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
static const char  _product_string[] = " " BP_OTP_PRODUCT_VERSION_STRING;
// note: using a string ensures the byte immediately following the last charcter is readable
//       which is important since writing ECC requires an even number of bytes.
static_assert(sizeof(char) == sizeof(uint8_t), "char must be 8-bits");
static_assert(ARRAY_SIZE(_product_string) <= USB_WHITELABEL_MAX_CHARS_BP_VERSION + 1);

// NOTE: Reserved for product string:
//                  // Rows 0x0e8 .. 0x0eb (space character + 7 additional characters maximum)

// NOTE: Reserved for manufacturing data:
//                  // Rows 0x0ec .. 0x0ff (40 characters maximum)

// Leaving the manufacturing data portion uncoded...
// That will be filled in by another process


// Detect if firmware is ready for whitelabeling

// don't want to use that difficult-to-parse API in many places....
static int write_ecc_wrapper(uint16_t starting_row, const void* buffer, size_t buffer_size) {
    otp_cmd_t cmd;
    cmd.flags = starting_row;
    cmd.flags |= OTP_CMD_ECC_BITS;
    cmd.flags |= OTP_CMD_WRITE_BITS;
    return rom_func_otp_access((uint8_t*)buffer, buffer_size, cmd);
}
static int read_ecc_wrapper(uint16_t starting_row, void* buffer, size_t buffer_size) {
    // TODO: use own ECC decoding functions ...
    otp_cmd_t cmd;
    cmd.flags = starting_row;
    cmd.flags |= OTP_CMD_ECC_BITS;
    return rom_func_otp_access((uint8_t*)buffer, buffer_size, cmd);
}
static int write_raw_wrapper(uint16_t starting_row, const void* buffer, size_t buffer_size) {
    otp_cmd_t cmd;
    cmd.flags = starting_row;
    cmd.flags |= OTP_CMD_WRITE_BITS;
    return rom_func_otp_access((uint8_t*)buffer, buffer_size, cmd);
}
static int read_raw_wrapper(uint16_t starting_row, void* buffer, size_t buffer_size) {
    // TODO: use own ECC decoding functions ...
    otp_cmd_t cmd;
    cmd.flags = starting_row;
    return rom_func_otp_access((uint8_t*)buffer, buffer_size, cmd);
}
static bool write_single_otp_ecc_row(uint16_t row, uint16_t data) {
    uint16_t existing_data;
    int r;
    r = read_ecc_wrapper(row, &existing_data, sizeof(existing_data));
    if (BOOTROM_OK != r) {
        WHITELABEL_DEBUG("WHITELABEL ERROR: Failed to read OTP ecc row %03x: %d (0x%x)\n", row, r, r);
        return false;
    }
    if (existing_data == data) {
        // already written, nothing more to do for this row
        return true;
    }
    r = write_ecc_wrapper(row, &data, sizeof(data));
    if (BOOTROM_OK != r) {
        WHITELABEL_DEBUG("WHITELABEL ERROR: Failed to write OTP ecc row %03x: %d (0x%x)\n", row, r, r);
        return false;
    }
    // and verify the data is now there
    r = read_ecc_wrapper(row, &existing_data, sizeof(existing_data));
    if (BOOTROM_OK != r) {
        WHITELABEL_DEBUG("WHITELABEL ERROR: Failed to read OTP ecc row %03x: %d (0x%x)\n", row, r, r);
        return false;
    }
    if (existing_data != data) {
        WHITELABEL_DEBUG("WHITELABEL ERROR: Failed to verify OTP ecc row %03x: %d (0x%x) has data 0x%04x\n", row, r, r, data);
        return false;
    }
    return true;
}
static bool write_single_otp_raw_row(uint16_t row, uint32_t data) {
    uint32_t existing_data;
    int r;
    r = write_raw_wrapper(row, &existing_data, sizeof(existing_data));
    if (BOOTROM_OK != r) {
        WHITELABEL_DEBUG("WHITELABEL ERROR: Failed to read OTP raw row %03x: %d (0x%x)\n", row, r, r);
        return false;
    }
    if (existing_data == data) {
        // already written, nothing more to do for this row
        return true;
    }
    r = write_raw_wrapper(row, &data, sizeof(data));
    if (BOOTROM_OK != r) {
        WHITELABEL_DEBUG("WHITELABEL ERROR: Failed to write OTP raw row %03x: %d (0x%x)\n", row, r, r);
        return false;
    }
    // and verify the data is now there
    r = read_raw_wrapper(row, &existing_data, sizeof(existing_data));
    if (BOOTROM_OK != r) {
        WHITELABEL_DEBUG("WHITELABEL ERROR: Failed to read OTP raw row %03x: %d (0x%x)\n", row, r, r);
        return false;
    }
    if (existing_data != data) {
        WHITELABEL_DEBUG("WHITELABEL ERROR: Failed to verify OTP raw row %03x: %d (0x%x) has data 0x%06x\n", row, r, r, data);
        return false;
    }
    return true;
}
static void write_single_otp_ecc_row_or_die(uint16_t row, uint16_t data) {
    if (!write_single_otp_ecc_row(row, data)) {
        hard_assert(false);
    }
}
static void write_single_otp_raw_row_or_die(uint16_t row, uint32_t data) {
    if (!write_single_otp_raw_row(row, data)) {
        hard_assert(false);
    }
}
static bool read_otp_2_of_3(uint16_t start_row, uint32_t* out_data) {
    *out_data = 0xFFFFFFFFu;

    // 1. read the base address
    uint32_t v[3];
    int r[3];
    r[0] = read_raw_wrapper(start_row+0, &(v[0]), sizeof(uint32_t)); // DO NOT DIE ON FAILURE OF ANY ONE ROW
    r[1] = read_raw_wrapper(start_row+1, &(v[1]), sizeof(uint32_t)); // DO NOT DIE ON FAILURE OF ANY ONE ROW
    r[2] = read_raw_wrapper(start_row+2, &(v[2]), sizeof(uint32_t)); // DO NOT DIE ON FAILURE OF ANY ONE ROW
    
    if ((r[0] == BOOTROM_OK) && (r[1] == BOOTROM_OK) && (r[2] == BOOTROM_OK)) {
        // All three value read successfully ... so use bitwise majority voting
        WHITELABEL_DEBUG("Read OTP 2-of-3: rows 0x%03x, 0x%03x, and 0x%03x all read successfully (0x%06x, 0x%06x, 0x%06x)\n",
            start_row+0, start_row+1, start_row+2,
            v[0], v[1], v[2]
        );
        uint32_t result = 0u;
        for (uint32_t mask = 0x00800000u; mask; mask >>= 1) {
            uint_fast8_t count = 0;
            if (v[0] & mask) { ++count; }
            if (v[1] & mask) { ++count; }
            if (v[2] & mask) { ++count; }
            if (count >= 2) {
                result |= mask;
            }
        }
        WHITELABEL_DEBUG("Read OTP 2-of-3: Bit-by-bit voting result: 0x%06x\n", result);
        *out_data = result;
        return true;
    }

    // else at most two reads succeeded.  Can only accept a perfect match of the data.
    if ((r[0] == BOOTROM_OK) && (r[1] == BOOTROM_OK) && (v[0] == v[1]) && ((v[0] & 0xFF000000u) == 0)) {
        WHITELABEL_DEBUG("Read OTP 2-of-3: rows 0x%03x and 0x%03x agree on data 0x%06x\n", start_row+0, start_row+1, v[0]);
        *out_data = v[0];
        return true;
    } else
    if ((r[0] == BOOTROM_OK) && (r[2] == BOOTROM_OK) && (v[0] == v[2]) && ((v[0] & 0xFF000000u) == 0)) {
        WHITELABEL_DEBUG("Read OTP 2-of-3: rows 0x%03x and 0x%03x agree on data 0x%06x\n", start_row+0, start_row+2, v[0]);
        *out_data = v[0];
        return true;
    } else
    if ((r[1] == BOOTROM_OK) && (r[2] == BOOTROM_OK) && (v[1] == v[2]) && ((v[1] & 0xFF000000u) == 0)) {
        WHITELABEL_DEBUG("Read OTP 2-of-3: rows 0x%03x and 0x%03x agree on data 0x%06x\n", start_row+1, start_row+2, v[1]);
        *out_data = v[1];
        return true;
    } else
    {
        WHITELABEL_DEBUG("Read OTP 2-of-3: rows 0x%03x, 0x%03x, and 0x%03x  (%d,%d,%d) --> (0x%06x, 0x%06x, 0x%06x) --> NO AGREEMENT\n",
            start_row+0, start_row+1, start_row+2,
            r[0], r[1], r[2],
            v[0], v[1], v[2]
        );
        return false;
    }
}
static bool write_otp_2_of_3(uint16_t start_row, uint32_t new_value) {

    // 1. read the old data
    WHITELABEL_DEBUG("Write OTP 2-of-3: row 0x%03x\n", start_row); WAIT_FOR_KEY();

    uint32_t old_voted_bits;
    if (!read_otp_2_of_3(start_row, &old_voted_bits)) {
        WHITELABEL_DEBUG("Failed to read agreed-upon old bits for OTP 2-of-3: rows 0x%03x, 0x%03x, and 0x%03x\n", start_row+0, start_row+1, start_row+2);
        return false;
    }

    // If any bits are already voted upon as set, there's no way to unset them.
    if (old_voted_bits & (~new_value)) {
        WHITELABEL_DEBUG("Fail: Old voted-upon value 0x%06x has bits set that are not in the new value 0x%06x ---> 0x%06x\n",
            old_voted_bits, new_value,
            old_voted_bits & (~new_value)
        );
        return false;
    }

    // 2. Read each row individually, OR in the requested bits to be set, and write back the new value
    //    Note that each individual row may have bits set that are not in the new value.  That's OK.
    uint32_t old_data;
    int r;
    for (uint16_t i = 0; i < 3; ++i) {
        r = read_raw_wrapper(start_row+i, &old_data, sizeof(old_data));
        if (BOOTROM_OK != r) {
            WHITELABEL_DEBUG("Warning: unable to read old bits for OTP 2-of-3: row 0x%03x\n", start_row+i);
            continue; // to next OTP row, if any
        }
        if ((old_data & new_value) == new_value) {
            // no change needed
            WHITELABEL_DEBUG("Warning: skipping update to row 0x%03x: old value 0x%06x already has bits 0x%06x\n", start_row+i, old_data, new_value);
            continue; // to next OTP row, if any
        }

        uint32_t to_write = old_data | new_value;
        WHITELABEL_DEBUG("Write USB_BOOT_FLAGS: updating row 0x%03x: 0x%06x --> 0x%06x\n", start_row+i, old_data, to_write); WAIT_FOR_KEY();
        r = write_raw_wrapper(start_row+i, &to_write, sizeof(to_write));
        if (BOOTROM_OK != r) {
            WHITELABEL_DEBUG("Failed to write new bits for OTP 2-of-3: row 0x%03x: 0x%06x --> 0x%06x\n", start_row+i, old_data, to_write);
            continue; // to next OTP row, if any
        }
        WHITELABEL_DEBUG("Wrote new bits for OTP 2-of-3: row 0x%03x: 0x%06x --> 0x%06x\n", start_row+i, old_data, to_write);
    }

    // 3. Can we read the data as now voted upon?
    uint32_t new_voted_bits;
    if (!read_otp_2_of_3(start_row, &new_voted_bits)) {
        WHITELABEL_DEBUG("Failed to read agreed-upon new bits for OTP 2-of-3: rows 0x%03x, 0x%03x, and 0x%03x\n", start_row+0, start_row+1, start_row+2);
        return false;
    }

    // 4. Verify the new data matches the requested value
    if (new_voted_bits != new_value) {
        WHITELABEL_DEBUG("OTP 2-of-3: rows 0x%03x, 0x%03x, and 0x%03x: 0x%06x -> 0x%06x, but got 0x%06x\n",
            start_row+0, start_row+1, start_row+2,
            old_voted_bits, new_value, new_voted_bits
        );
        return false;
    }

    return true;
}
static bool read_otp_byte_3x(uint16_t row, uint8_t* out_data) {
    *out_data = 0xFFu;

    // 1. read the data
    OTP_RAW_READ_RESULT v;
    int r;
    r = read_raw_wrapper(row, &v.as_uint32, sizeof(uint32_t)); // DO NOT DIE ON FAILURE OF ANY ONE ROW
    if (BOOTROM_OK != r) {
        WHITELABEL_DEBUG("Failed to read OTP byte 3x: row 0x%03x: %d (0x%x)\n", row, r, r);
        return false;
    }
    // use bit-by-bit majority voting
    WHITELABEL_DEBUG("Read OTP byte_3x row 0x%03x: (0x%02x, 0x%02x, 0x%02x)\n", row, v.as_bytes[0], v.as_bytes[1], v.as_bytes[2]);
    uint8_t result = 0u;
    for (uint8_t mask = 0x80u; mask; mask >>= 1) {
        uint_fast8_t count = 0;
        if (v.as_bytes[0] & mask) { ++count; }
        if (v.as_bytes[1] & mask) { ++count; }
        if (v.as_bytes[2] & mask) { ++count; }
        if (count >= 2) {
            result |= mask;
        }
    }
    WHITELABEL_DEBUG("Read OTP byte_3x row 0x%03x: Bit-by-bit voting result: 0x%02x\n", row, result);
    *out_data = result;
    return true;
}
static bool write_otp_byte_3x(uint16_t row, uint8_t new_value) {

    // 1. read the old data
    WHITELABEL_DEBUG("Write OTP byte_3x: row 0x%03x\n", row); WAIT_FOR_KEY();

    uint8_t old_voted_bits;
    if (!read_otp_byte_3x(row, &old_voted_bits)) {
        WHITELABEL_DEBUG("Failed to read agreed-upon old bits for OTP byte_3x: row 0x%03x\n", row);
        return false;
    }

    // If any bits are already voted upon as set, there's no way to unset them.
    if (old_voted_bits & (~new_value)) {
        WHITELABEL_DEBUG("Fail: Old voted-upon byte_3x value 0x%02x has bits set that are not in the new value 0x%02x ---> 0x%02x\n",
            old_voted_bits, new_value,
            old_voted_bits & (~new_value)
        );
        return false;
    }

    // 2. Read the row raw, OR in the requested bits to be set, and write back the new value.
    //    Note that each individual byte may have bits set that are not in the new value.  That's OK.
    OTP_RAW_READ_RESULT old_raw_data;
    int r;
    r = read_raw_wrapper(row, &old_raw_data, sizeof(old_raw_data));
    if (BOOTROM_OK != r) {
        WHITELABEL_DEBUG("Warning: unable to read old bits for OTP byte_3x: row 0x%03x\n", row);
        return false;
    }
    OTP_RAW_READ_RESULT new_value_3x = { .as_bytes = { new_value, new_value, new_value } };
    OTP_RAW_READ_RESULT to_write = { .as_uint32 = old_raw_data.as_uint32 | new_value_3x.as_uint32 };
    if (old_raw_data.as_uint32 == to_write.as_uint32) {
        // no change needed
        WHITELABEL_DEBUG("Warning: skipping update to row 0x%03x: old value 0x%06x already has bits 0x%06x\n", row, old_raw_data.as_uint32, new_value_3x.as_uint32);
        return true;
    }

    WHITELABEL_DEBUG("Write OTP byte_3x: updating row 0x%03x: 0x%06x --> 0x%06x\n", row, old_raw_data.as_uint32, to_write.as_uint32); WAIT_FOR_KEY();

    r = write_raw_wrapper(row, &to_write, sizeof(to_write));
    if (BOOTROM_OK != r) {
        WHITELABEL_DEBUG("Failed to write new bits for byte_3x: row 0x%03x: 0x%06x --> 0x%06x\n", row, old_raw_data.as_uint32, to_write.as_uint32);
        return false;
    }

    // 3. Can we read the data as now voted upon?
    uint8_t new_voted_bits;
    if (!read_otp_byte_3x(row, &new_voted_bits)) {
        WHITELABEL_DEBUG("Failed to read agreed-upon new bits for OTP byte_3x: row 0x%03x\n", row);
        return false;
    }

    // 4. Verify the new data matches the requested value
    if (new_voted_bits != new_value) {
        WHITELABEL_DEBUG("OTP byte_3x: row 0x%03x: 0x%02x -> 0x%02x, but got 0x%02x\n",
            row, old_voted_bits, new_value, new_voted_bits
        );
        return false;
    }

    return true;
}

void apply_whitelabel_data(void) {
    static const uint16_t base = 0x0c0; // written so this can be changed easily
    static const size_t product_extension_rows = sizeof(_product_string) /2u; // sizeof() includes null; round down to even number
    uint16_t product_char_count = strlen("Bus Pirate") + strlen(_product_string);
    
    // 1. write the static portion         --> Rows 0x0d0 .. 0x0e7
    for (uint16_t i = 0; i < ARRAY_SIZE(_static_portion); ++i) {
        uint16_t row = base + 0x10u +  i;
        WHITELABEL_DEBUG("Write static portion index %d: row 0x%03x, data 0x%04x\n", i, row, _static_portion[i]);
        WAIT_FOR_KEY();
        write_single_otp_ecc_row_or_die(row, _static_portion[i]);
    }
    WHITELABEL_DEBUG("Version extension: '%s'\n", _product_string);
    // 2. also write the product version extension
    for (uint16_t i = 0; i < product_extension_rows; ++i) {
        uint16_t row = base + 0x28u + i;
        uint16_t data = 0;
        // First  character is stored in LSB
        // Second character is stored in MSB
        data  |= (uint8_t)(_product_string[2u * i + 1]);
        data <<= 8;
        data  |= (uint8_t)(_product_string[2u * i    ]);

        WHITELABEL_DEBUG("Write product version extension index %d: row 0x%03x, data 0x%04x\n", i, row, data);
        WAIT_FOR_KEY();
        write_single_otp_ecc_row_or_die(row, data);
    }

    // NOTE: DOES NOT WRITE THE MANUFACTURING DATA PORTION

    // 3. encode the product string length and offset
    uint16_t tmp_p = 0x2300 | product_char_count;

    // 4. Write the portions of the first 16 rows that have valid data:
    WHITELABEL_DEBUG("Write WHITELABEL index 0: row 0x%03x\n", base + 0x0u); WAIT_FOR_KEY(); write_single_otp_ecc_row_or_die(base + 0x0u, 0x1209u);   // USB VID == 0x1209
    WHITELABEL_DEBUG("Write WHITELABEL index 1: row 0x%03x\n", base + 0x1u); WAIT_FOR_KEY(); write_single_otp_ecc_row_or_die(base + 0x1u, 0x7332u);   // USB PID == 0x7332
    //write_single_otp_ecc_row_or_die(base + 0x.u, 0x....u); // USB BCD Device
    //write_single_otp_ecc_row_or_die(base + 0x.u, 0x....u); // USB LangID for strings
    WHITELABEL_DEBUG("Write WHITELABEL index 4: row 0x%03x\n", base + 0x4u); WAIT_FOR_KEY(); write_single_otp_ecc_row_or_die(base + 0x4u, 0x230Au);   // USB MANU              ten   chars @ offset 0x23
    WHITELABEL_DEBUG("Write WHITELABEL index 5: row 0x%03x\n", base + 0x5u); WAIT_FOR_KEY(); write_single_otp_ecc_row_or_die(base + 0x5u, tmp_p  );   // USB PROD              XXX   chars @ offset 0x23
    //write_single_otp_ecc_row_or_die(base + 0x.u, 0x....u); // USB Serial Number
    //write_single_otp_ecc_row_or_die(base + 0x.u, 0x....u); // USB config attributes & max power
    WHITELABEL_DEBUG("Write WHITELABEL index 8: row 0x%03x\n", base + 0x8u); WAIT_FOR_KEY(); write_single_otp_ecc_row_or_die(base + 0x8u, 0x1B08u);   // STOR VOLUME LABEL     eight chars @ offset 0x1b
    WHITELABEL_DEBUG("Write WHITELABEL index 9: row 0x%03x\n", base + 0x9u); WAIT_FOR_KEY(); write_single_otp_ecc_row_or_die(base + 0x9u, 0x1F08u);   // SCSI INQUIRY VENDOR   eight chars @ offset 0x1f
    WHITELABEL_DEBUG("Write WHITELABEL index A: row 0x%03x\n", base + 0xAu); WAIT_FOR_KEY(); write_single_otp_ecc_row_or_die(base + 0xAu, tmp_p  );   // SCSI INQUIRY PRODUCT  XXX   chars @ offset 0x23
    WHITELABEL_DEBUG("Write WHITELABEL index C: row 0x%03x\n", base + 0xCu); WAIT_FOR_KEY(); write_single_otp_ecc_row_or_die(base + 0xCu, 0x1016u);   // redirect URL          22    chars @ offset 0x10
    WHITELABEL_DEBUG("Write WHITELABEL index D: row 0x%03x\n", base + 0xDu); WAIT_FOR_KEY(); write_single_otp_ecc_row_or_die(base + 0xDu, 0x140du);   // redirect name         13    chars @ offset 0x14
    WHITELABEL_DEBUG("Write WHITELABEL index E: row 0x%03x\n", base + 0xEu); WAIT_FOR_KEY(); write_single_otp_ecc_row_or_die(base + 0xEu, tmp_p  );   // info_uf2.txt product  XXX   chars @ offset 0x23
    //write_single_otp_ecc_row_or_die(base + 0xFu, 0x2Cxxu); // info_uf2.txt manufacturing string ... to be added later

    // 5. write the `WHITE_LABEL_ADDR` to point to the base address used here
    WHITELABEL_DEBUG("Write WHITELABEL_BASE_ADDR 0x%03x to row 0x05C\n", base); WAIT_FOR_KEY();
    write_single_otp_ecc_row_or_die(0x05c, base);

    // 6. NON-ECC write the USB boot flags ... writes go to three (3) consecutive rows
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
    OTP_USB_BOOT_FLAGS old_usb_boot_flags;
    if (!read_otp_2_of_3(0x059, &old_usb_boot_flags.as_uint32)) {
        WHITELABEL_DEBUG("WHITELABEL ERROR: Raw OTP rows 0x59..0x5B (old USB BOOT FLAGS) did not find majority agreement\n");
        return;
    }
    if ((old_usb_boot_flags.as_uint32 & usb_boot_flags.as_uint32) == usb_boot_flags.as_uint32) {
        // No need to write ... all the requested flags are already set
        WHITELABEL_DEBUG("WHITELABEL INFO: USB BOOT FLAGS already set\n");
        return;
    }
    if ((old_usb_boot_flags.as_uint32 & ~usb_boot_flags.as_uint32) != 0) {
        WHITELABEL_DEBUG("WHITELABEL WARNING: Old USB BOOT FLAGS 0x%06x vs. intended %06x has additional bits: %06x\n",
            old_usb_boot_flags.as_uint32, usb_boot_flags.as_uint32,
            old_usb_boot_flags.as_uint32 & ~usb_boot_flags.as_uint32
        );
        // continue anyways?  This allow the code to run even after manufacturing string has been applied....
    }
    OTP_USB_BOOT_FLAGS new_flags = { .as_uint32 = old_usb_boot_flags.as_uint32 | usb_boot_flags.as_uint32 };

    WHITELABEL_DEBUG("Writing USB BOOT FLAGS: 0x%06x -> 0x%06x\n", old_usb_boot_flags.as_uint32, new_flags);  WAIT_FOR_KEY();
    if (!write_otp_2_of_3(0x059, new_flags.as_uint32)) {
        WHITELABEL_DEBUG("WHITELABEL ERROR: Failed to write USB BOOT FLAGS\n");
        return;
    }

    WHITELABEL_DEBUG("WHITELABEL INFO: USB BOOT FLAGS updated\n");
    return;

    // That's it!
}


bool apply_manufacturing_string(const char* manufacturing_data_string) {
    uint16_t base;
    // Follow the breadcrumbs to find the base address
    OTP_USB_BOOT_FLAGS old_usb_boot_flags;
    if (!read_otp_2_of_3(0x059, &old_usb_boot_flags.as_uint32)) {
        WHITELABEL_DEBUG("WHITELABEL ERROR: Raw OTP rows 0x59..0x5B (USB BOOT FLAGS) did not find majority agreement\n");
        return false;
    }
    WHITELABEL_DEBUG("found USB BOOT FLAGS: %06x\n", old_usb_boot_flags.as_uint32);

    // validate the USB BOOT FLAGS?
    if (!old_usb_boot_flags.white_label_addr_valid) {
        WHITELABEL_DEBUG("WHITELABEL ERROR: No white label data to update (%06x)\n", old_usb_boot_flags.as_uint32);
        return false;
    }

    int r = read_ecc_wrapper(0x5C, &base, sizeof(base));
    if (BOOTROM_OK != r) {
        WHITELABEL_DEBUG("WHITELABEL ERROR: OTP row 0x5C could not be read (WHITE_LABEL_ADDR): %d (0x%x)\n", r, r);
        return false;
    }
    if (base != 0x0C0) {
        // TODO: other validation as needed
        WHITELABEL_DEBUG("WHITELABEL ERROR: OTP row 0x5C (WHITE_LABEL_ADDR) is not set to 0x0C0, but 0x%03x\n", base);
        return false;
    }
    WHITELABEL_DEBUG("WHITE_LABEL_ADDR points to row index 0x%03x\n", base);

    // Validate the static portion of the strings matches
    // That static portion starts at offset 0x10 from the base whitelabel structure.
    uint8_t* white_label_ecc_alias = (uint8_t*)(OTP_DATA_BASE + ((base+0x10) * 2u));
    if (memcmp(white_label_ecc_alias, _static_portion, sizeof(_static_portion)) != 0) {
        WHITELABEL_DEBUG("WHITELABEL ERROR: Static portion of white label data at row 0x%03x (alias 0x%08x) does not match\n", base, white_label_ecc_alias);
        return false;
    }

    WHITELABEL_DEBUG("Static portion of the strings look ok\n");

    size_t char_count = strlen(manufacturing_data_string);
    if (char_count > 40) {
        WHITELABEL_DEBUG("WHITELABEL ERROR: Manufacturing data string is too long (%d chars)\n", char_count);
        return false;
    }
    uint16_t strdef = 0x2c00u | char_count;

    // check if the STRDEF is already set ... if it is, verify it matches the current provided string's length
    uint16_t oldstrdef;
    if (read_ecc_wrapper(base + 0xFu, &oldstrdef, sizeof(oldstrdef)) != BOOTROM_OK) {
        WHITELABEL_DEBUG("WHITELABEL ERROR: Failed to read prior manufacturing STRDEF\n");
        return false;
    } else if ((oldstrdef != 0u) && (oldstrdef != strdef)) {
        WHITELABEL_DEBUG("WHITELABEL ERROR: Old manufacturing STRDEF (0x%06x) does not match new (0x%06x)\n", oldstrdef, strdef);
        return false;
    }

    WHITELABEL_DEBUG("Writing the manufacturing string data\n"); WAIT_FOR_KEY();

    // first write the actual string's data
    for (size_t i = 0; i < (char_count+1)/2; ++i) {
        uint16_t row = base + 0x2Cu + i;
        uint16_t data = 0;
        // First  character is stored in LSB
        // Second character is stored in MSB
        data  |= (uint8_t)(manufacturing_data_string[2u * i + 1]);
        data <<= 8;
        data  |= (uint8_t)(manufacturing_data_string[2u * i    ]);

        WHITELABEL_DEBUG("Writing row 0x%03x: 0x%04x (`%c%c`)\n", row, data, byte_to_printable_char(data & 0xFFu), byte_to_printable_char(data >> 8)); WAIT_FOR_KEY();
        if (!write_single_otp_ecc_row(row, data)) {
            WHITELABEL_DEBUG("WHITELABEL ERROR: Failed with partial manufacturing string data written.\n");
            return false;
        }
    }

    WHITELABEL_DEBUG("Writing row 0x%03x: STRDEF %04x\n", base + 0xFu, strdef); WAIT_FOR_KEY();
    if (!write_single_otp_ecc_row(base + 0xFu, strdef)) {
        WHITELABEL_DEBUG("WHITELABEL ERROR: Failed to write manufacturing string length and offset\n");
    }

    WHITELABEL_DEBUG("Updating the USB_BOOT_FLAGS to mark the manufacturing string as valid\n");

    OTP_USB_BOOT_FLAGS new_usb_boot_flags = { .as_uint32 = old_usb_boot_flags.as_uint32 };
    new_usb_boot_flags.info_uf2_boardid_valid = 1;

    WHITELABEL_DEBUG("Writing rows 0x059..0x05B: 0x%06x --> 0x%06x\n", old_usb_boot_flags.as_uint32, new_usb_boot_flags.as_uint32); WAIT_FOR_KEY();
    if (!write_otp_2_of_3(0x059, new_usb_boot_flags.as_uint32)) {
        WHITELABEL_DEBUG("Failed to update USB BOOT FLAGs\n");
    }

    if (base != 0x0c0) {
        WHITELABEL_DEBUG("WHITELABEL ERROR: Not yet supporting locking of OTP page unless base address is 0x0C0\n");
        return false;
    }
    // Finally, update page protections for page 3 (`0x0c0 .. 0x0ff`).
    // Row `0xF86`, PAGE3_LOCK0 = `0x3F3F3F` = `0b00'111'111` : NO_KEY_STATE: 0b00, KEY_R: 0b111, KEY_W: 0b111 (no key; read-only without key)
    // Row `0xF87`, PAGE3_LOCK1 = `0x151515` = `0b00'01'01'01` : LOCK_S: 0b01, LOCK_NS: 0b01, LOCK_BL: 0b01 (read-only for all)

    WHITELABEL_DEBUG("Setting PAGE3_LOCK0 (0xF86) to `0x3Fu` (no keys; read-only without key)\n"); WAIT_FOR_KEY();
    if (!write_otp_byte_3x(0xF86, 0x3Fu)) {
        WHITELABEL_DEBUG("WHITELABEL ERROR: Failed to write PAGE3_LOCK0\n");
        return false;
    }
    WHITELABEL_DEBUG("Setting PAGE3_LOCK1 (0xF87) to `0x151515` (read-only for all three)\n"); WAIT_FOR_KEY();
    if (!write_otp_byte_3x(0xF87, 0x15u)) {
        WHITELABEL_DEBUG("WHITELABEL ERROR: Failed to write PAGE3_LOCK1\n");
        return false;
    }

    WHITELABEL_DEBUG("Manufacturing string successfully applied and locked down.\n");
    return true;
}



                 