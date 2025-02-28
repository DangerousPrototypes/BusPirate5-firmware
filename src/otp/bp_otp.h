#pragma once
#include <stdint.h>
#include <stdbool.h>
#include <assert.h>

#if BP_VER !=5
    #include "hardware/structs/otp.h"
    #include "pico/bootrom.h"
    #include "hardware/regs/addressmap.h"
    #include "hardware/regs/otp.h"
#endif


// Use prefix `bp_otp_` for functions in this file
// Use prefix `BP_OTP_` for enums and typedef'd structs in this file

#ifdef __cplusplus
extern "C" {
#endif

// Note: looking at these results is only for debugging purposes.
//       Code can simply check for error by checking if the top 8 bits are non-zero.
//       (any bit set in the top 8 bits indicates error)
typedef enum _BP_OTP_ECC_ERROR {
    // only low 24 bits can contain data
    // Thus, all values from 0x00000000u .. 0x00FFFFFFu are successful results
    BP_OTP_ECC_ERROR_INVALID_INPUT                 = 0xFF010000u,
    BP_OTP_ECC_ERROR_DETECTED_MULTI_BIT_ERROR      = 0xFF020000u,
    BP_OTP_ECC_ERROR_BRBP_NEITHER_DECODING_VALID   = 0xFF030000u, // BRBP = 0b10 or 0b01, but neither decodes precisely

    BP_OTP_ECC_ERROR_INVALID_ENCODING              = 0xFF040000u, // Syndrome alone generates data, but too many bit flips...
    BP_OTP_ECC_ERROR_BRBP_DUAL_DECODINGS_POSSIBLE  = 0x80000000u, // TODO: prove code makes this impossible to hit
    BP_OTP_ECC_ERROR_INTERNAL_ERROR_BRBP_BIT       = 0x80010000u, // TODO: prove code makes this impossible to hit
    BP_OTP_ECC_ERROR_INTERNAL_ERROR_PERFECT_MATCH  = 0x80020000u, // TODO: prove code makes this impossible to hit
} BP_OTP_ECC_ERROR;

// When reading OTP data, the result is a 32-bit value.
// Sometimes it's convenient to view as a uint32_t,
// other times it's convenient to view the individual fields.
// (ab)use anonymous unions to allow whichever use is convenient.
typedef struct _OTP_RAW_READ_RESULT {
    // anonymous structs are supported in C11
    union {
        uint32_t as_uint32;
        uint8_t  as_bytes[4];
        struct {
            uint8_t lsb;
            uint8_t msb;
            union {
                struct {
                    uint8_t hamming_ecc : 5; // aka syndrome during correction
                    uint8_t parity_bit : 1;
                    uint8_t bit_repair_by_polarity : 2;
                };
                uint8_t correction;
            };
            uint8_t is_error; // 0: ok
        };
    };
} BP_OTP_RAW_READ_RESULT;
typedef BP_OTP_RAW_READ_RESULT OTP_RAW_READ_RESULT; // TODO -- remove this old name for the struct
static_assert(sizeof(BP_OTP_RAW_READ_RESULT) == sizeof(uint32_t));

uint32_t bp_otp_calculate_ecc(uint16_t x);                     // [[unsequenced]]
uint32_t bp_otp_decode_raw(const BP_OTP_RAW_READ_RESULT data); // [[unsequenced]]

// RP2350 OTP can encode data in multiple ways:
// * 24 bits of raw data (no error correction / detection)
// * 16 bits of ECC protected data (using the RP2350's standard ECC to correct 1-bit, detect 2-bit errors)
// *  8 bits of triple-redundant data (using a majority vote to correct each bit, aka 2-of-3 voting in a single OTP ROW)
// * Using multiple rows for N-of-M voting (e.g., boot critical fields might be recorded in eight rows...)
// There are many edge cases when reading or writing an OTP row,

// NOT RECOMMENDED DUE TO LIKELIHOOD OF UNDETECTED ERRORS:
// Writes a single OTP row with 24-bits of data.  No ECC / BRBP is used.
// Returns false if the data could not be written (e.g., if any bit is
// already set to 1, and the new value sets that bit to zero).
bool bp_otp_write_single_row_raw(uint16_t row, uint32_t new_value);
// NOT RECOMMENDED DUE TO LIKELIHOOD OF UNDETECTED ERRORS:
// Reads a single OTP row raw, returning the 24-bits of data without interpretation.
// Not recommended for general use.
bool bp_otp_read_single_row_raw(uint16_t row, uint32_t* out_data);

// Writes a single OTP row with 16-bits of data, protected by ECC.
// Writes will not fail due to a single bit already being set to one.
// Returns false if the data could not be written (e.g., if more than
// one bit was already written to 1).
bool bp_otp_write_single_row_ecc(uint16_t row, uint16_t new_value);
// Reads a single OTP row, applies bit recovery by polarity,
// and corrects any single-bit errors using ECC.  Returns the
// corrected 16-bits of data.
// Returns false if the data could not be successfully read (e.g.,
// uncorrectable errors detected, etc.)
bool bp_otp_read_single_row_ecc(uint16_t row, uint16_t* out_data);

// Writes a single OTP row with 8-bits of data stored with 3x redundancy.
// For each bit of the new value that is zero:
//   the existing OTP row is permitted to have that bit set to one in
//   at most one of the three locations, without causing a failure.
// After writing the new value, the function reads the value back to
// ensure that (with voting applied) the new value was correctly stored.
// This style of storage is mostly used for flags that are independently
// updated over multiple OTP writes. 
bool bp_otp_write_single_row_2_of_3(uint16_t row, uint8_t new_value);
// Reads a single OTP row with 8-bits of data stored with 3x redundancy.
// Returns the 8-bits of data, after applying 2-of-3 voting.
bool bp_otp_read_single_row_2_of_3(uint16_t row, uint8_t* out_data);

// Writes three consecutive rows of OTP data with same 24-bit data.
// For each bit with a new value of zero:
//   the existing OTP rows are permitted to have that bit set to one
//   in one of the three rows, without this function failing.
// After writing the new values, the function reads the value back
// and will return false if the value (with voting applied) is not
// the expected new value.
bool bp_otp_write_redundant_rows_2_of_3(uint16_t start_row, uint32_t new_value);
// Reads three consecutive rows of raw OTP data (24-bits), and applies
// 2-of-3 voting for each bit independently.
// Returns the 24-bits of voted-upon data.
bool bp_otp_read_redundant_rows_2_of_3(uint16_t start_row, uint32_t* out_data);



#if BP_VER == 5
    // These functions actually access the hardware,
    // so code calling it on RP2040 is probably in error?
    // This is a nicer error message than a linker error....
    inline void bp_otp_apply_whitelabel_data(void) [[deprecated]] { }
    inline bool bp_otp_apply_manufacturing_string(const char* manufacturing_data_string) [[deprecated]] { return false; }
#else
    void bp_otp_apply_whitelabel_data(void);
    bool bp_otp_apply_manufacturing_string(const char* manufacturing_data_string);
#endif


#ifdef __cplusplus
}
#endif

