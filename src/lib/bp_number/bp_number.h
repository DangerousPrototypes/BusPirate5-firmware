/**
 * @file bp_number.h
 * @brief Unified number parsing for Bus Pirate.
 * @details Parses integers and floats from strings with format detection.
 *          Supports hex (0x), binary (0b), decimal, and float formats.
 *          No dynamic allocation - uses simple pointer advancement.
 *
 * Usage:
 *   const char *p = "0xFF 123";
 *   uint32_t val;
 *   bp_num_format_t fmt;
 *   
 *   if (bp_num_u32(&p, &val, &fmt)) {
 *       // val = 255, fmt = BP_NUM_HEX, p points to " 123"
 *   }
 *   
 *   bp_num_skip_whitespace(&p);  // p now points to "123"
 *   
 *   if (bp_num_u32(&p, &val, &fmt)) {
 *       // val = 123, fmt = BP_NUM_DEC
 *   }
 */

#ifndef BP_NUMBER_H
#define BP_NUMBER_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

/**
 * @brief Number format detected during parsing.
 */
typedef enum {
    BP_NUM_NONE = 0,    ///< No number found
    BP_NUM_DEC,         ///< Decimal (123)
    BP_NUM_HEX,         ///< Hexadecimal (0x7B or 0X7B)
    BP_NUM_BIN,         ///< Binary (0b1111011 or 0B1111011)
    BP_NUM_FLOAT,       ///< Float (3.14)
} bp_num_format_t;

/*
 * =============================================================================
 * Core parsing functions (pointer-to-pointer, advances past consumed input)
 * =============================================================================
 */

/**
 * @brief Parse uint32 with auto-detect format (0x=hex, 0b=bin, else dec).
 * @param str    Pointer to string pointer (advanced past number)
 * @param value  Output value
 * @param fmt    Optional output format (can be NULL)
 * @return true if valid number parsed, false if no number at position
 */
bool bp_num_u32(const char **str, uint32_t *value, bp_num_format_t *fmt);

/**
 * @brief Parse int32 with auto-detect format (supports leading -).
 * @param str    Pointer to string pointer (advanced past number)
 * @param value  Output value
 * @param fmt    Optional output format (can be NULL)
 * @return true if valid number parsed
 */
bool bp_num_i32(const char **str, int32_t *value, bp_num_format_t *fmt);

/**
 * @brief Parse float (integer part, optional decimal point and fraction).
 * @param str    Pointer to string pointer (advanced past number)
 * @param value  Output value
 * @return true if valid number parsed
 */
bool bp_num_float(const char **str, float *value);

/*
 * =============================================================================
 * Format-specific parsing (no prefix detection)
 * =============================================================================
 */

/**
 * @brief Parse hex digits (no 0x prefix expected).
 */
bool bp_num_hex(const char **str, uint32_t *value);

/**
 * @brief Parse binary digits (no 0b prefix expected).
 */
bool bp_num_bin(const char **str, uint32_t *value);

/**
 * @brief Parse decimal digits.
 */
bool bp_num_dec(const char **str, uint32_t *value);

/*
 * =============================================================================
 * Utility functions
 * =============================================================================
 */

/**
 * @brief Skip whitespace (space, tab, CR, LF).
 * @param str  Pointer to string pointer (advanced past whitespace)
 */
static inline void bp_num_skip_whitespace(const char **str) {
    while (**str == ' ' || **str == '\t' || **str == '\r' || **str == '\n') {
        (*str)++;
    }
}

/**
 * @brief Check if at end of string (null or whitespace).
 */
static inline bool bp_num_at_end(const char *str) {
    return *str == '\0' || *str == ' ' || *str == '\t';
}

/**
 * @brief Check if character is hex digit.
 */
static inline bool bp_num_is_hex(char c) {
    return (c >= '0' && c <= '9') || 
           ((c | 0x20) >= 'a' && (c | 0x20) <= 'f');
}

/**
 * @brief Convert hex character to value (0-15).
 */
static inline uint8_t bp_num_hex_val(char c) {
    if (c >= '0' && c <= '9') return c - '0';
    return (c | 0x20) - 'a' + 10;
}

/**
 * @brief Check if character is decimal digit.
 */
static inline bool bp_num_is_dec(char c) {
    return c >= '0' && c <= '9';
}

/**
 * @brief Check if character is binary digit.
 */
static inline bool bp_num_is_bin(char c) {
    return c == '0' || c == '1';
}

#endif // BP_NUMBER_H

