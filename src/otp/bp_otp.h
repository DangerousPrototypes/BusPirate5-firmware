#pragma once
#include <stdint.h>
#include <stdbool.h>
#include <assert.h>


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
} OTP_RAW_READ_RESULT;
static_assert(sizeof(OTP_RAW_READ_RESULT) == sizeof(uint32_t));

uint32_t bp_otp_calculate_ecc(uint16_t x);
uint32_t bp_otp_decode_raw(const OTP_RAW_READ_RESULT data);

#if BP_VER == 5
    // // ?? mark these as deprecated for compilation warnings if used?
    // inline void bp_otp_apply_whitelabel_data(void) __attribute__((deprecated)) { }
    // inline bool bp_otp_apply_manufacturing_string(const char* manufacturing_data_string) __attribute__((deprecated)) { return false; }
    inline void bp_otp_apply_whitelabel_data(void) { }
    inline bool bp_otp_apply_manufacturing_string(const char* manufacturing_data_string) { return false; }
#else
    void bp_otp_apply_whitelabel_data(void);
    bool bp_otp_apply_manufacturing_string(const char* manufacturing_data_string);
#endif


#ifdef __cplusplus
}
#endif

