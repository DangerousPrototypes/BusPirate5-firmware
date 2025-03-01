#include <stdint.h>
#include <assert.h>
#include "bp_otp.h"


static const uint32_t SUCCESS_MASK = 0x0000FFFFu;

// Syndrome to bitflip table
// Syndrome is 5 bits, but not all values are used.
static const uint16_t sdk_otp_syndrome_to_bitflip[32] = {
    // [ 3] = 0x0001,
    // [ 5] = 0x0002,
    // [ 6] = 0x0004,
    // [ 7] = 0x0008,
    // [ 9] = 0x0010,
    // [10] = 0x0020,
    // [11] = 0x0040,
    // [12] = 0x0080,
    // [13] = 0x0100,
    // [14] = 0x0200,
    // [15] = 0x0400,
    // [17] = 0x0800,
    // [18] = 0x1000,
    // [19] = 0x2000,
    // [20] = 0x4000,
    // [21] = 0x8000,
    0x0000u, 0x0000u, 0x0000u, 0x0001u,  // [ 0.. 3]
    0x0000u, 0x0002u, 0x0004u, 0x0008u,  // [ 4.. 7]
    0x0000u, 0x0010u, 0x0020u, 0x0040u,  // [ 8..11]
    0x0080u, 0x0100u, 0x0200u, 0x0400u,  // [12..15]
    0x0000u, 0x0800u, 0x1000u, 0x2000u,  // [16..19]
    0x4000u, 0x8000u, 0x0000u, 0x0000u,  // [20..23] // 22..31 are unused for error correction
    0x0000u, 0x0000u, 0x0000u, 0x0000u,  // [24..27]
    0x0000u, 0x0000u, 0x0000u, 0x0000u,  // [28..31]
};
static const uint32_t _otp_ecc_parity_table[6] = {
    0b0000001010110101011011,
    0b0000000011011001101101,
    0b0000001100011110001110,
    0b0000000000011111110000,
    0b0000001111100000000000,
    0b0111111111111111111111,
};
static uint32_t _even_parity(uint32_t input) {
    uint32_t rc = 0;
    while (input) {
        rc ^= input & 1;
        input >>= 1;
    }
    return rc;
}
static uint32_t decode_raw_data_with_correction_impl(const OTP_RAW_READ_RESULT data) {
    // If this decodes correctly, only the lower 16-bits will be set.
    // Else, at least the top eight bits will be set, to indicate
    // an error condition.  (FFxxxxxx)

    // Initially based on trying to emulate what reading via
    // the ECC memory-mapped alias does, as described in datasheet:
    // See section 13.6.1 Bit Repair By Polarity @ page 1264
    // See section 13.6.2 Modified Hamming ECC @ page 1265

    // Input must be limited to 24-bit values
    if (data.as_uint32 & 0xFF000000u) {
        return BP_OTP_ECC_ERROR_INVALID_INPUT;
    }
    // 0. if only one BRBP bit is set, then check if exact match...
    if ((data.bit_repair_by_polarity == 0x1u) || (data.bit_repair_by_polarity == 0x2u)) {
        const uint16_t decoded_wo_brbp =  data.as_uint32;
        const uint16_t decoded_w__brbp = ~data.as_uint32;
        const OTP_RAW_READ_RESULT src_wo_brbp = { .as_uint32 = bp_otp_calculate_ecc( decoded_wo_brbp )              };
        const OTP_RAW_READ_RESULT src_w__brbp = { .as_uint32 = bp_otp_calculate_ecc( decoded_w__brbp ) ^ 0x00FFFFFF };

        uint32_t diff_wo_brbp = data.as_uint32 ^ src_wo_brbp.as_uint32;
        uint32_t diff_w__brbp = data.as_uint32 ^ src_w__brbp.as_uint32;
        bool match_wo_brbp = (diff_wo_brbp == 0x00800000u) || (diff_wo_brbp == 0x00400000u);
        bool match_w__brbp = (diff_w__brbp == 0x00800000u) || (diff_w__brbp == 0x00400000u);

        // Are both of those decodings an exact match (except for the BRBP bits?  If so, that's 
        if (match_wo_brbp && match_w__brbp) {
            // NOTE: This is expected to be impossible, but only way to know is via exhaustively checking all 16-bit values.
            return BP_OTP_ECC_ERROR_BRBP_DUAL_DECODINGS_POSSIBLE;
        } else if (match_wo_brbp) {
            // There was a single-bit error in the BRBP bits; True value was 0b00  (no BRBP used to store the ECC encoded data).
            return decoded_wo_brbp;
        } else if (match_w__brbp) {
            // There was a single-bit error in the BRBP bits; True value was 0b11  (BRBP used to store the ECC encoded data).
            return decoded_w__brbp;
        } else {
            // Either multiple bits in error, or this data was not encoded with the RP2350 ECC encoding scheme.
            return BP_OTP_ECC_ERROR_BRBP_NEITHER_DECODING_VALID;
        }
    }

    // 1. if BRBP bits are both set, then invert all bits before further processing
    // See section 13.6.1 Bit Repair By Polarity @ page 1264:
    //     When you read an OTP value through an ECC alias,
    //     BRBP checks for two ones in bits 23:22.  When both
    //     bits 23 and 22 are set, BRBP inverts the entire row
    //     before passing it to the modified Hamming code stage.
    const OTP_RAW_READ_RESULT src = {
        .as_uint32 =
            (data.bit_repair_by_polarity == 0x3u) ?
            (data.as_uint32 ^ 0x00FFFFFFu) :
            (data.as_uint32)
    };
    
    // 2. re-calculate the six parity bits using lower 16-bits
    // See section 13.6.2 Modified Hamming ECC @ page 1265
    //     When you read an OTP value through an ECC alias,
    //     ECC recalculates the six parity bits based on the
    //     value read from the OTP row.
    //     ...
    OTP_RAW_READ_RESULT tmp = { .as_uint32 = bp_otp_calculate_ecc(src.as_uint32) };

    // 3. Simplest case: do the values match exactly? If so, done!
    if (tmp.as_uint32 == src.as_uint32) {
        return src.as_uint32 & SUCCESS_MASK; // SUCCESS!
    }
    
    // 4. XOR the recalculated ECC vs. src bits
    // See section 13.6.2 Modified Hamming ECC @ page 1265
    //     ...
    //     Then, ECC XORs the original six (6) parity bits with
    //     the newly-calculated parity bits.
    //
    //     This generates six (6) new bits:
    //     • the five (5) LSBs are the syndrome, a unique
    //       bit pattern that corresponds to each possible
    //       bit flip in the data value
    //     • the MSB distinguishes between odd and even
    //       numbers of bit flips
    //     ...
    tmp.as_uint32 ^= src.as_uint32;

    // NEW: if syndrome has exactly one bit set,
    //      then bit flip was outside the 16 data bits.
    //      Done!
    if ((tmp.as_uint32 & (tmp.as_uint32 - 1u)) == 0u) {
        return src.as_uint32 & SUCCESS_MASK; // SUCCESS!
    }

    // NEW: Not specified in datasheet ...
    //      AFTER XOR ... the parity bit needs to flip
    //      if the low 5 bits have an odd number of set bits?
    if (_even_parity(tmp.hamming_ecc)) {
        tmp.parity_bit ^= 1;
    }
    // NEW: If syndrome is 0b00001 the single-bit error was in one of the BRBP bits?
    if (tmp.hamming_ecc == 0x01u) {
        // NOTE: This is expected to be impossible to reach this code,
        //       because should have found the one-bit error earlier (above).
        //       After all, a one-bit error in BRBP is either 0b01 or 0b10,
        //       both of which checked above.  Belt-and-suspenders....
        return BP_OTP_ECC_ERROR_INTERNAL_ERROR_BRBP_BIT;
    }

    // 4. Decide result based on the parity & syndrome
    // See section 13.6.2 Modified Hamming ECC @ page 1265
    //     ...
    //     If all 6 bits in this value are zero,
    //         ECC did not detect an error.
    //     If the MSB is 0, but the syndrome contains a value other than 0,
    //         the ECC detected an unrecoverable multi-bit error.
    //     ...
    if (tmp.parity_bit == 0u && tmp.hamming_ecc == 0u) {
        // NOTE: This is expected to be impossible to reach this code,
        //       because should have discovered no error existed earlier,
        //       when calculated ECC from just the low 16 bits.
        //       Even so, belt-and-suspenders... Can this occur any other time?
        //       would need to check all 2^24 input values to be sure.
        return BP_OTP_ECC_ERROR_INTERNAL_ERROR_PERFECT_MATCH;
    }
    if (tmp.parity_bit == 0u && tmp.hamming_ecc != 0u) {
        return BP_OTP_ECC_ERROR_DETECTED_MULTI_BIT_ERROR;
    }


    // 5. Else correct the single-bit error ... the syndrome is the index into the bitflip array.
    // See section 13.6.2 Modified Hamming ECC @ page 1265
    //     ...
    //     If the MSB is 1,
    //         the syndrome should indicate a
    //         single-bit error.
    //         ECC flips the corresponding data bit
    //         to recover from the error.
    //     ...
    uint16_t bitflip = sdk_otp_syndrome_to_bitflip[tmp.hamming_ecc];

    if (bitflip == 0u) {
        // The table has entries for all **VALID** single-bit flip syndromes.
        // All other values in the table are 0u, which would not flip a bit,
        // and thus would not correct an error.
        // It's unknown if this code path could be reached.
        return BP_OTP_ECC_ERROR_NOT_VALID_SINGLE_BIT_FLIP;
    }

    return (src.as_uint32 ^ bitflip) & SUCCESS_MASK;
}



// ======================================================================
// The following are the only two non-static functions in this file.
// everything above are just the implementation details.
// ======================================================================


uint32_t bp_otp_calculate_ecc(uint16_t x) {
    uint32_t p = x;
    for (uint_fast8_t i = 0; i < 6; ++i) {
        p |= _even_parity(p & _otp_ecc_parity_table[i]) << (16 + i);
    }
    return p;
}

uint32_t bp_otp_decode_raw(uint32_t raw_data) {
    const BP_OTP_RAW_READ_RESULT data = { .as_uint32 = raw_data };
    // This function DISABLES raw data that the bootrom MIGHT accept as
    // being validly encoded ECC data.  This can occur when the count
    // of bitflips is 3, 5 (also 19, 21 for BRBP variants) vs. the correct encoding.
    uint32_t result = decode_raw_data_with_correction_impl(data);

    // if use of syndrome calculates matching low 16 bits,
    // then final check that original data has <2 bit flips vs.
    // the ECC encoded value.
    // This excludes erroneous acceptance of encodings with 3 or 5 bit flips.
    // (also excludes erroneous encodings with 19 or 21 bitflips ... see BRBP)
    // As a result, this function provides heightened reliability for the ECC decoding,
    // vs. a function that skips this step.

    if ((result & 0xFFFF0000u) == 0) {
        // result could be encoded two ways: with or without use of BRBP
        uint32_t chk  = bp_otp_calculate_ecc(result);
        uint32_t brbp = chk ^ 0xFFFFFFu;
        uint32_t chk_bits  = chk  ^ data.as_uint32;
        uint32_t brbp_bits = brbp ^ data.as_uint32;
        if ((__builtin_popcount(chk_bits) <= 1) && (data.bit_repair_by_polarity != 0x3u)) {
            // this is OK
        } else if ((__builtin_popcount(brbp_bits) <= 1) && (data.bit_repair_by_polarity != 0x0u)) {
            // this is OK
        } else {
            // this is NOT a valid ECC decoding ... but maybe the bootrom will think it is.  :-)
            return BP_OTP_ECC_ERROR_POTENTIALLY_READABLE_BY_BOOTROM;
        }
    }
    return result;
}





#ifdef __cplusplus
}
#endif
