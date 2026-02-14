#include <stdio.h>
#include "pico/stdlib.h"
#include <stdint.h>
#include "pirate.h"
#include "system_config.h"
#include "command_struct.h"
#include "bytecode.h"
#include "modes.h"
#include "ui/ui_const.h"
#include "ui/ui_term.h"

// order bits according to lsb/msb setting
uint32_t ui_format_bitorder(uint32_t d) {
    uint32_t result, mask;
    int i;

    if (!system_config.bit_order) { // 0=MSB
        return d;
    } else {
        mask = 0x80000000;
        result = 0;

        for (i = 0; i < 32; i++) {
            if (d & mask) {
                result |= (1 << (i));
            }
            mask >>= 1;
        }

        return result >> (32 - system_config.num_bits);
    }
}
// customize bitorder and bits
uint32_t ui_format_lsb(uint32_t d, uint8_t num_bits) {
    uint32_t result, mask;

    mask = 0x80000000;
    result = 0;
    for (uint32_t i = 0; i < 32; i++) {
        if ((d)&mask) {
            result |= (1 << (i));
        }
        mask >>= 1;
    }
    return (result >> (32 - num_bits));
}
void ui_format_bitorder_manual(uint32_t* d, uint8_t num_bits, bool bit_order) {
    if (!bit_order) {
        return; // 0=MSB
    }
    (*d) = ui_format_lsb(*d, num_bits);
}

//-----------------------------------------------------------------------------
// Number formatting helpers
//-----------------------------------------------------------------------------

/**
 * @brief Get alternating color for digit grouping.
 * @param state  Current color state (toggled after call)
 * @return ANSI color escape sequence
 */
static inline const char* format_get_color(bool* state) {
    const char* color = (*state) ? ui_term_color_num_float() : ui_term_color_reset();
    *state = !(*state);
    return color;
}

/**
 * @brief Calculate number of hex nibbles needed (rounded up to even).
 */
static inline uint8_t format_calc_nibbles(uint8_t num_bits) {
    uint8_t nibbles = (num_bits + 3) / 4;  // Round up
    return (nibbles + 1) & ~1;              // Round to even (byte-aligned)
}

/**
 * @brief Mask value to specified bit width.
 */
static inline uint32_t format_mask_value(uint32_t value, uint8_t num_bits) {
    return (num_bits < 32) ? (value & ((1u << num_bits) - 1)) : value;
}

/**
 * @brief Print value in hexadecimal with byte-wise color alternation.
 */
static void format_print_hex(uint32_t value, uint8_t num_bits) {
    uint8_t total_bits = format_calc_nibbles(num_bits) * 4;
    bool color_state = true;
    
    printf("0x");
    for (uint8_t pos = total_bits; pos > 0; pos -= 8) {
        printf("%s", format_get_color(&color_state));
        printf("%c%c", 
            ascii_hex[(value >> (pos - 4)) & 0x0F],
            ascii_hex[(value >> (pos - 8)) & 0x0F]);
    }
    printf("%s", ui_term_color_reset());
}

/**
 * @brief Print value in binary with nibble-wise color alternation.
 */
static void format_print_bin(uint32_t value, uint8_t num_bits) {
    uint8_t nibble_pos = num_bits % 4;
    if (nibble_pos == 0) nibble_pos = 4;
    bool color_state = false;
    
    printf("0b%s", ui_term_color_num_float());
    for (uint8_t i = 0; i < num_bits; i++) {
        // Switch color at nibble boundaries
        if (nibble_pos == 0) {
            printf("%s", format_get_color(&color_state));
            nibble_pos = 4;
        }
        nibble_pos--;
        
        uint32_t bit = (value >> (num_bits - 1 - i)) & 1;
        printf("%c", '0' + bit);
    }
    printf("%s", ui_term_color_reset());
}

/**
 * @brief Print formatted number with specified bit width and format.
 */
void ui_format_print_number_3(uint32_t value, uint32_t num_bits, uint32_t display_format) {
    value = format_mask_value(value, num_bits);

    switch (display_format) {
        case df_ascii:
        case df_hex:
            format_print_hex(value, num_bits);
            break;
        case df_dec:
            printf("%u", value);
            break;
        case df_bin:
            format_print_bin(value, num_bits);
            break;
    }

    if (num_bits != 8) {
        printf(".%d", num_bits);
    }
}

// represent d in the current display mode. If numbits=8 also display the ascii representation
void ui_format_print_number_2(struct command_attributes* attributes, uint32_t* value) {
    uint32_t d = (*value);
    uint8_t num_bits = attributes->has_dot ? attributes->dot : system_config.num_bits;
    uint8_t display_format = (system_config.display_format == df_auto) 
                             ? attributes->number_format 
                             : system_config.display_format;

    // Mask value to num_bits
    uint32_t mask = (num_bits < 32) ? ((1 << num_bits) - 1) : 0xFFFFFFFF;
    d &= mask;

    // Print ASCII prefix if applicable
    if (display_format == df_ascii || attributes->has_string) {
        if ((char)d >= ' ' && (char)d <= '~') {
            printf("'%c' ", (char)d);
        } else {
            printf("''  ");
        }
    }

    // Delegate to core formatting function
    ui_format_print_number_3(d, num_bits, display_format);
}

