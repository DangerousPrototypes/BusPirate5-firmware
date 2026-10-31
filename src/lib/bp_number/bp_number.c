/**
 * @file bp_number.c
 * @brief Unified number parsing implementation.
 * @details Simple pointer-advancement parsing for linear buffers.
 */

#include "bp_number.h"

/*
 * =============================================================================
 * Format-specific parsing
 * =============================================================================
 */

bool bp_num_hex(const char **str, uint32_t *value) {
    const char *p = *str;
    uint32_t v = 0;
    int digits = 0;
    
    while (bp_num_is_hex(*p)) {
        v = (v << 4) | bp_num_hex_val(*p);
        p++;
        digits++;
        if (digits > 8) {
            // Overflow - too many hex digits for uint32
            // Continue parsing to advance pointer, but value is truncated
        }
    }
    
    if (digits == 0) {
        return false;
    }
    
    *value = v;
    *str = p;
    return true;
}

bool bp_num_bin(const char **str, uint32_t *value) {
    const char *p = *str;
    uint32_t v = 0;
    int digits = 0;
    
    while (bp_num_is_bin(*p)) {
        v = (v << 1) | (*p - '0');
        p++;
        digits++;
        if (digits > 32) {
            // Overflow
        }
    }
    
    if (digits == 0) {
        return false;
    }
    
    *value = v;
    *str = p;
    return true;
}

bool bp_num_dec(const char **str, uint32_t *value) {
    const char *p = *str;
    uint32_t v = 0;
    int digits = 0;
    
    while (bp_num_is_dec(*p)) {
        // Overflow check: 4294967295 is max uint32
        if (v > 429496729 || (v == 429496729 && (*p - '0') > 5)) {
            // Would overflow - continue parsing but cap value
            v = 0xFFFFFFFF;
        } else {
            v = v * 10 + (*p - '0');
        }
        p++;
        digits++;
    }
    
    if (digits == 0) {
        return false;
    }
    
    *value = v;
    *str = p;
    return true;
}

/*
 * =============================================================================
 * Auto-detect parsing
 * =============================================================================
 */

bool bp_num_u32(const char **str, uint32_t *value, bp_num_format_t *fmt) {
    const char *p = *str;
    bp_num_format_t f = BP_NUM_NONE;
    bool ok = false;
    
    *value = 0;
    
    // Check for prefix: 0x or 0b
    if (p[0] == '0' && (p[1] | 0x20) == 'x') {
        // Hex
        p += 2;
        ok = bp_num_hex(&p, value);
        f = BP_NUM_HEX;
    } else if (p[0] == '0' && (p[1] | 0x20) == 'b') {
        // Binary
        p += 2;
        ok = bp_num_bin(&p, value);
        f = BP_NUM_BIN;
    } else if (bp_num_is_dec(*p)) {
        // Decimal
        ok = bp_num_dec(&p, value);
        f = BP_NUM_DEC;
    }
    
    if (ok) {
        *str = p;
        if (fmt) *fmt = f;
    }
    return ok;
}

bool bp_num_i32(const char **str, int32_t *value, bp_num_format_t *fmt) {
    const char *p = *str;
    bool negative = false;
    
    // Check for negative sign
    if (*p == '-') {
        negative = true;
        p++;
    }
    
    uint32_t uval;
    bp_num_format_t f;
    if (!bp_num_u32(&p, &uval, &f)) {
        return false;
    }
    
    // Range check
    if (negative) {
        if (uval > 2147483648U) {
            return false;  // Overflow
        }
        *value = -(int32_t)uval;
    } else {
        if (uval > 2147483647U) {
            return false;  // Overflow
        }
        *value = (int32_t)uval;
    }
    
    *str = p;
    if (fmt) *fmt = f;
    return true;
}

bool bp_num_float(const char **str, float *value) {
    const char *p = *str;
    bool negative = false;
    int digits = 0;
    
    *value = 0.0f;
    
    // Check for negative sign
    if (*p == '-') {
        negative = true;
        p++;
    }
    
    // Integer part
    uint32_t ipart = 0;
    while (bp_num_is_dec(*p)) {
        ipart = ipart * 10 + (*p - '0');
        p++;
        digits++;
    }
    
    float v = (float)ipart;
    
    // Decimal point and fractional part
    if (*p == '.' || *p == ',') {
        p++;
        
        float frac = 0.0f;
        float divisor = 10.0f;
        
        while (bp_num_is_dec(*p)) {
            frac += (float)(*p - '0') / divisor;
            divisor *= 10.0f;
            p++;
            digits++;
        }
        
        v += frac;
    }
    
    if (digits == 0) {
        return false;
    }
    
    *value = negative ? -v : v;
    *str = p;
    return true;
}

