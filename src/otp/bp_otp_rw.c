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

// Detect if firmware is ready for whitelabeling
// don't want to use that difficult-to-parse API in many places....
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

// RP2350 OTP storage is strongly recommended to use some form of
// error correction.  Most rows will use ECC, but three other forms exist:
// (1) 2-of-3 voting of a single byte in a single row
// (2) 2-of-3 voting of 24-bits across three consecutive OTP rows (RBIT-3)
// (3) 3-of-8 voting of 24-bits across eight consecutive OTP rows (RBIT-8)
//
// A note on RBIT-8:
// RBIT-8 is used _ONLY_ for CRIT0 and CRIT1. It works similarly to RBIT-3,
// except that each bit is considered set if at least three (3) of the eight
// rows have that bit set.  Thus, it's not a simple majority vote, instead
// tending to favor considering bits as set.
// 
static bool write_single_otp_ecc_row(uint16_t row, uint16_t data) {
    uint16_t existing_data;
    int r;
    r = read_ecc_wrapper(row, &existing_data, sizeof(existing_data));
    if (BOOTROM_OK != r) {
        PRINT_ERROR("Whitelabel Error: Failed to read OTP ecc row %03x: %d (0x%x)\n", row, r, r);
        return false;
    }
    if (existing_data == data) {
        // already written, nothing more to do for this row
        return true;
    }
    r = write_ecc_wrapper(row, &data, sizeof(data));
    if (BOOTROM_OK != r) {
        PRINT_ERROR("Whitelabel Error: Failed to write OTP ecc row %03x: %d (0x%x)\n", row, r, r);
        return false;
    }
    // and verify the data is now there
    r = read_ecc_wrapper(row, &existing_data, sizeof(existing_data));
    if (BOOTROM_OK != r) {
        PRINT_ERROR("Whitelabel Error: Failed to read OTP ecc row %03x: %d (0x%x)\n", row, r, r);
        return false;
    }
    if (existing_data != data) {
        PRINT_ERROR("Whitelabel Error: Failed to verify OTP ecc row %03x: %d (0x%x) has data 0x%04x\n", row, r, r, data);
        return false;
    }
    return true;
}
static bool write_single_otp_raw_row(uint16_t row, uint32_t data) {
    uint32_t existing_data;
    int r;
    r = write_raw_wrapper(row, &existing_data, sizeof(existing_data));
    if (BOOTROM_OK != r) {
        PRINT_ERROR("Whitelabel Warn: Failed to read OTP raw row %03x: %d (0x%x)\n", row, r, r);
        return false;
    }
    if (existing_data == data) {
        // already written, nothing more to do for this row
        return true;
    }
    r = write_raw_wrapper(row, &data, sizeof(data));
    if (BOOTROM_OK != r) {
        PRINT_ERROR("Whitelabel Warn: Failed to write OTP raw row %03x: %d (0x%x)\n", row, r, r);
        return false;
    }
    // and verify the data is now there
    r = read_raw_wrapper(row, &existing_data, sizeof(existing_data));
    if (BOOTROM_OK != r) {
        PRINT_ERROR("Whitelabel Warn: Failed to read OTP raw row %03x: %d (0x%x)\n", row, r, r);
        return false;
    }
    if (existing_data != data) {
        PRINT_ERROR("Whitelabel Warn: Failed to verify OTP raw row %03x: %d (0x%x) has data 0x%06x\n", row, r, r, data);
        return false;
    }
    return true;
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
        PRINT_VERBOSE("Whitelabel Read OTP 2-of-3: rows 0x%03x, 0x%03x, and 0x%03x all read successfully (0x%06x, 0x%06x, 0x%06x)\n",
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
        PRINT_VERBOSE("Whitelabel Read OTP 2-of-3: Bit-by-bit voting result: 0x%06x\n", result);
        *out_data = result;
        return true;
    }

    // else at most two reads succeeded.  Can only accept a perfect match of the data.
    if ((r[0] == BOOTROM_OK) && (r[1] == BOOTROM_OK) && (v[0] == v[1]) && ((v[0] & 0xFF000000u) == 0)) {
        PRINT_VERBOSE("Whitelabel Read OTP 2-of-3: rows 0x%03x and 0x%03x agree on data 0x%06x\n", start_row+0, start_row+1, v[0]);
        *out_data = v[0];
        return true;
    } else
    if ((r[0] == BOOTROM_OK) && (r[2] == BOOTROM_OK) && (v[0] == v[2]) && ((v[0] & 0xFF000000u) == 0)) {
        PRINT_VERBOSE("Whitelabel Read OTP 2-of-3: rows 0x%03x and 0x%03x agree on data 0x%06x\n", start_row+0, start_row+2, v[0]);
        *out_data = v[0];
        return true;
    } else
    if ((r[1] == BOOTROM_OK) && (r[2] == BOOTROM_OK) && (v[1] == v[2]) && ((v[1] & 0xFF000000u) == 0)) {
        PRINT_VERBOSE("Whitelabel Read OTP 2-of-3: rows 0x%03x and 0x%03x agree on data 0x%06x\n", start_row+1, start_row+2, v[1]);
        *out_data = v[1];
        return true;
    } else
    {
        PRINT_ERROR("Whitelabel Error: Read OTP 2-of-3: rows 0x%03x, 0x%03x, and 0x%03x  (%d,%d,%d) --> (0x%06x, 0x%06x, 0x%06x) --> NO AGREEMENT\n",
            start_row+0, start_row+1, start_row+2,
            r[0], r[1], r[2],
            v[0], v[1], v[2]
        );
        return false;
    }
}
static bool write_otp_2_of_3(uint16_t start_row, uint32_t new_value) {

    // 1. read the old data
    PRINT_DEBUG("Whitelabel Debug: Write OTP 2-of-3: row 0x%03x\n", start_row); WAIT_FOR_KEY();

    uint32_t old_voted_bits;
    if (!read_otp_2_of_3(start_row, &old_voted_bits)) {
        PRINT_DEBUG("Whitelabel Debug: Failed to read agreed-upon old bits for OTP 2-of-3: rows 0x%03x, 0x%03x, and 0x%03x\n", start_row+0, start_row+1, start_row+2);
        return false;
    }

    // If any bits are already voted upon as set, there's no way to unset them.
    if (old_voted_bits & (~new_value)) {
        PRINT_ERROR("Whitelabel Error: Fail: Old voted-upon value 0x%06x has bits set that are not in the new value 0x%06x ---> 0x%06x\n",
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
            PRINT_WARNING("Whitelabel Warning: unable to read old bits for OTP 2-of-3: row 0x%03x\n", start_row+i);
            continue; // to next OTP row, if any
        }
        if ((old_data & new_value) == new_value) {
            // no change needed
            PRINT_WARNING("Whitelabel Warning: skipping update to row 0x%03x: old value 0x%06x already has bits 0x%06x\n", start_row+i, old_data, new_value);
            continue; // to next OTP row, if any
        }

        uint32_t to_write = old_data | new_value;
        PRINT_DEBUG("Whitelabel Debug: Write USB_BOOT_FLAGS: updating row 0x%03x: 0x%06x --> 0x%06x\n", start_row+i, old_data, to_write); WAIT_FOR_KEY();
        r = write_raw_wrapper(start_row+i, &to_write, sizeof(to_write));
        if (BOOTROM_OK != r) {
            PRINT_ERROR("Whitelabel Error: Failed to write new bits for OTP 2-of-3: row 0x%03x: 0x%06x --> 0x%06x\n", start_row+i, old_data, to_write);
            continue; // to next OTP row, if any
        }
        PRINT_DEBUG("Whitelabel Debug: Wrote new bits for OTP 2-of-3: row 0x%03x: 0x%06x --> 0x%06x\n", start_row+i, old_data, to_write);
    }

    // 3. Can we read the data as now voted upon?
    uint32_t new_voted_bits;
    if (!read_otp_2_of_3(start_row, &new_voted_bits)) {
        PRINT_ERROR("Whitelabel Error: Failed to read agreed-upon new bits for OTP 2-of-3: rows 0x%03x, 0x%03x, and 0x%03x\n", start_row+0, start_row+1, start_row+2);
        return false;
    }

    // 4. Verify the new data matches the requested value
    if (new_voted_bits != new_value) {
        PRINT_ERROR("Whitelabel Error: OTP 2-of-3: rows 0x%03x, 0x%03x, and 0x%03x: 0x%06x -> 0x%06x, but got 0x%06x\n",
            start_row+0, start_row+1, start_row+2,
            old_voted_bits, new_value, new_voted_bits
        );
        return false;
    }
    PRINT_DEBUG("Whitelabel Debug: Successfully update the RBIT3 (2-of-3 voting) rows\n");

    return true;
}
static bool read_otp_byte_3x(uint16_t row, uint8_t* out_data) {
    *out_data = 0xFFu;

    // 1. read the data
    OTP_RAW_READ_RESULT v;
    int r;
    r = read_raw_wrapper(row, &v.as_uint32, sizeof(uint32_t)); // DO NOT DIE ON FAILURE OF ANY ONE ROW
    if (BOOTROM_OK != r) {
        PRINT_ERROR("Whitelabel Error: Failed to read OTP byte 3x: row 0x%03x: %d (0x%x)\n", row, r, r);
        return false;
    }
    // use bit-by-bit majority voting
    PRINT_DEBUG("Whitelabel Debug: Read OTP byte_3x row 0x%03x: (0x%02x, 0x%02x, 0x%02x)\n", row, v.as_bytes[0], v.as_bytes[1], v.as_bytes[2]);
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
    PRINT_DEBUG("Whitelabel Debug: Read OTP byte_3x row 0x%03x: Bit-by-bit voting result: 0x%02x\n", row, result);
    *out_data = result;
    return true;
}
static bool write_otp_byte_3x(uint16_t row, uint8_t new_value) {

    // 1. read the old data
    PRINT_DEBUG("Whitelabel Debug: Write OTP byte_3x: row 0x%03x\n", row); WAIT_FOR_KEY();

    uint8_t old_voted_bits;
    if (!read_otp_byte_3x(row, &old_voted_bits)) {
        PRINT_ERROR("Whitelabel Error: Failed to read agreed-upon old bits for OTP byte_3x: row 0x%03x\n", row);
        return false;
    }

    // If any bits are already voted upon as set, there's no way to unset them.
    if (old_voted_bits & (~new_value)) {
        PRINT_ERROR("Whitelabel Error: Fail: Old voted-upon byte_3x value 0x%02x has bits set that are not in the new value 0x%02x ---> 0x%02x\n",
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
        PRINT_ERROR("Whitelabel Error: Warning: unable to read old bits for OTP byte_3x: row 0x%03x\n", row);
        return false;
    }
    OTP_RAW_READ_RESULT new_value_3x = { .as_bytes = { new_value, new_value, new_value } };
    OTP_RAW_READ_RESULT to_write = { .as_uint32 = old_raw_data.as_uint32 | new_value_3x.as_uint32 };
    if (old_raw_data.as_uint32 == to_write.as_uint32) {
        // no change needed
        PRINT_WARNING("Whitelabel Warning: skipping update to row 0x%03x: old value 0x%06x already has bits 0x%06x\n", row, old_raw_data.as_uint32, new_value_3x.as_uint32);
        return true;
    }

    PRINT_DEBUG("Whitelabel Debug: Write OTP byte_3x: updating row 0x%03x: 0x%06x --> 0x%06x\n", row, old_raw_data.as_uint32, to_write.as_uint32); WAIT_FOR_KEY();

    r = write_raw_wrapper(row, &to_write, sizeof(to_write));
    if (BOOTROM_OK != r) {
        PRINT_ERROR("Whitelabel Error: Failed to write new bits for byte_3x: row 0x%03x: 0x%06x --> 0x%06x\n", row, old_raw_data.as_uint32, to_write.as_uint32);
        return false;
    }

    // 3. Can we read the data as now voted upon?
    uint8_t new_voted_bits;
    if (!read_otp_byte_3x(row, &new_voted_bits)) {
        PRINT_ERROR("Whitelabel Error: Failed to read agreed-upon new bits for OTP byte_3x: row 0x%03x\n", row);
        return false;
    }

    // 4. Verify the new data matches the requested value
    if (new_voted_bits != new_value) {
        PRINT_ERROR("Whitelabel Error: OTP byte_3x: row 0x%03x: 0x%02x -> 0x%02x, but got 0x%02x\n",
            row, old_voted_bits, new_value, new_voted_bits
        );
        return false;
    }

    return true;
}

/// All code above this point are the static helper functions / implementation details.
/// Only the below are the public API functions.


#if defined(BP_USE_VIRTUALIZED_OTP)

// Enable "virtualized" OTP ... useful for testing.
// 8k of OTP is a lot to virtualize...
// maybe only track written sections up to a fixed maximum,
// and pretend all other sections are filled with 0xFFFFFFu?
typedef struct _BP_OTP_VIRTUALIZED_PAGE {
    uint16_t start_row;    // If zero, this page hasn't been written to yet.
    uint16_t rfu_padding;
    BP_OTP_RAW_READ_RESULT data[0x40];   // each page stores 0x40 (64) rows of OTP data
} BP_OTP_VIRTUALIZED_PAGE;
static BP_OTP_VIRTUALIZED_PAGE virtualized_otp[40] = NULL;

// probably want a helper function to map from row to virtualized page (nullptr if not exists)
// Set allocate_if_needed only when next action is to write to the page.
static inline uint16_t ROW_TO_PAGE_OFFSET(uint16_t row) { return row & 0x3Fu; }
static inline BP_OTP_VIRTUALIZED_PAGE* ROW_TO_VIRTUALIZED_PAGE(uint16_t row, bool allocate_if_needed) {
    if (row >= 0x1000u) { return NULL; } // invalid OTP Row ..  range is 0x000..0xFFF

    BP_OTP_VIRTUALIZED_PAGE* result = NULL;
    uint16_t page_start_row = row & (~0x3Fu);
    // find matching start_row in array?
    for (size_t i = 0; (result == NULL) && (i < ARRAY_SIZE(virtualized_otp)); ++i) {
        if (virtualized_otp[i].start_row == page_start_row) {
            result = &virtualized_otp[i];
        }
    }
    if ((result == NULL) && allocate_if_needed) {
        for (size_t i = 0; (result == NULL) && (i < ARRAY_SIZE(virtualized_otp)); ++i) {
            if (virtualized_otp[i].start_row == 0u) {
                result = &virtualized_otp[i];
                result->start_row = page_start_row;
            }
        }    
    }
    return result;
}

// virtualize the access to the underlying OTP...
bool bp_otp_write_single_row_raw(uint16_t row, uint32_t new_value);
bool bp_otp_read_single_row_raw(uint16_t row, uint32_t* out_data);
bool bp_otp_write_single_row_ecc(uint16_t row, uint16_t new_value);
bool bp_otp_read_single_row_ecc(uint16_t row, uint16_t* out_data);
bool bp_otp_read_ecc_data(uint16_t start_row, void* out_data, size_t count_of_bytes);
bool bp_otp_write_single_row_byte3x(uint16_t row, uint8_t new_value);
bool bp_otp_read_single_row_byte3x(uint16_t row, uint8_t* out_data);
bool bp_otp_write_redundant_rows_2_of_3(uint16_t start_row, uint32_t new_value);
bool bp_otp_read_redundant_rows_2_of_3(uint16_t start_row, uint32_t* out_data);
bool bp_otp_read_ecc_data(uint16_t start_row, void* out_data, size_t count_of_bytes);

#else // !defined(BP_USE_VIRTUALIZED_OTP)


bool bp_otp_write_single_row_raw(uint16_t row, uint32_t new_value) {
    return write_single_otp_raw_row(row, new_value);
}
bool bp_otp_read_single_row_raw(uint16_t row, uint32_t* out_data) {
    return read_raw_wrapper(row, out_data, sizeof(uint32_t)) == BOOTROM_OK;
}
bool bp_otp_write_single_row_ecc(uint16_t row, uint16_t new_value) {
    // Use ROM function for ECC writing ... due to BRBP selection logic not yet implemented
    // May eventually do this in future ourselves... (not difficult)
    return write_single_otp_ecc_row(row, new_value);
}
bool bp_otp_read_single_row_ecc(uint16_t row, uint16_t* out_data) {
    uint32_t raw_data = 0xFFFFFFFFu;
    if (read_raw_wrapper(row, &raw_data, sizeof(raw_data)) != BOOTROM_OK) {
        return false;
    }
    uint32_t result = bp_otp_decode_raw(raw_data);
    if (result & 0xFF000000u) {
        return false;
    }
    *out_data = result & 0xFFFFu;
    return true;
}
bool bp_otp_read_ecc_data(uint16_t start_row, void* out_data, size_t count_of_bytes) {
    if (count_of_bytes >= (0x1000*2)) { // OTP rows from 0x000u to 0xFFFu, so max 0x1000*2 bytes
        return false;
    }

    uint8_t * buffer = out_data;
    for (size_t i = 0; i < (count_of_bytes/2u); i++) {
        uint16_t row = start_row + i;
        uint16_t tmpData;
        if (!bp_otp_read_single_row_ecc(row, &tmpData)) {
            return false;
        }
        buffer[(2*i)+0] = (tmpData >> (8u*0u)) & 0xFFu;
        buffer[(2*i)+1] = (tmpData >> (8u*1u)) & 0xFFu;
    }
    if (count_of_bytes & 1u) {
        uint16_t row = start_row + (count_of_bytes/2u);
        uint16_t tmpData;
        if (!bp_otp_read_single_row_ecc(row, &tmpData)) {
            return false;
        }
        buffer[count_of_bytes-1] = (tmpData >> (8u*0u)) & 0xFFu;
    }
    return true;
}
bool bp_otp_write_single_row_byte3x(uint16_t row, uint8_t new_value) {
    return write_otp_byte_3x(row, new_value);
}
bool bp_otp_read_single_row_byte3x(uint16_t row, uint8_t* out_data) {
    return read_otp_byte_3x(row, out_data);
}
bool bp_otp_write_redundant_rows_2_of_3(uint16_t start_row, uint32_t new_value) {
    return write_otp_2_of_3(start_row, new_value);
}
bool bp_otp_read_redundant_rows_2_of_3(uint16_t start_row, uint32_t* out_data) {
    return read_otp_2_of_3(start_row, out_data);
}

#endif // defined(BP_USE_VIRTUALIZED_OTP)





                 