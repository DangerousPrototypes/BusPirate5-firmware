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

// format can be set
void ui_format_print_number_3(uint32_t value, uint32_t num_bits, uint32_t display_format) {
    uint32_t mask, i, d, j;
    uint8_t num_nibbles;
    bool color_flip;

    if (num_bits < 32) {
        mask = ((1 << num_bits) - 1);
    } else {
        mask = 0xFFFFFFFF;
    }
    value &= mask;

    switch (display_format) {
        case df_ascii: // drop through and show hex

        case df_hex:
            num_nibbles = num_bits / 4;
            if (num_bits % 4) {
                num_nibbles++;
            }
            if (num_nibbles & 0b1) {
                num_nibbles++;
            }
            color_flip = true;
            printf("%s0x%s", "", "");
            for (i = num_nibbles * 4; i > 0; i -= 8) {
                printf("%s", (color_flip ? ui_term_color_num_float() : ui_term_color_reset()));
                color_flip = !color_flip;
                printf("%c", ascii_hex[((value >> (i - 4)) & 0x0F)]);
                printf("%c", ascii_hex[((value >> (i - 8)) & 0x0F)]);
            }
            printf("%s", ui_term_color_reset());
            break;
        case df_dec:
            printf("%d", value);
            break;
        case df_bin:
            j = num_bits % 4;
            if (j == 0) {
                j = 4;
            }
            color_flip = false;
            printf("%s0b%s", "", ui_term_color_num_float());
            for (i = 0; i < num_bits; i++) {
                if (!j) {
                    if (color_flip) {
                        color_flip = !color_flip;
                        printf("%s", ui_term_color_num_float());
                    } else {
                        color_flip = !color_flip;
                        printf("%s", ui_term_color_reset());
                    }
                    j = 4;
                }
                j--;
                mask = 1 << (num_bits - i - 1);
                if (value & mask) {
                    printf("1");
                } else {
                    printf("0");
                }
            }
            printf("%s", ui_term_color_reset());
            break;
    }

    if (num_bits != 8) {
        printf(".%d", num_bits);
    }
}

// represent d in the current display mode. If numbits=8 also display the ascii representation
void ui_format_print_number_2(struct command_attributes* attributes, uint32_t* value) {
    uint32_t mask, i, d, j;
    uint8_t num_bits, num_nibbles, display_format;
    bool color_flip;

    d = (*value);
    if (attributes->has_dot) {
        num_bits = attributes->dot;
    } else {
        num_bits = system_config.num_bits;
    }

    if (system_config.display_format == df_auto) // AUTO setting!
    {
        display_format = attributes->number_format;
    } else {
        display_format = system_config.display_format;
    }

    if (num_bits < 32) {
        mask = ((1 << num_bits) - 1);
    } else {
        mask = 0xFFFFFFFF;
    }
    d &= mask;

    if (display_format == df_ascii || attributes->has_string) {
        if ((char)d >= ' ' && (char)d <= '~') {
            printf("'%c' ", (char)d);
        } else {
            printf("''  ");
        }
    }

    // TODO: move this part to second function/third functions so we can reuse it from other number print places with
    // custom specs that aren't in attributes
    switch (display_format) {
        case df_ascii: // drop through and show hex

        case df_hex:
            num_nibbles = num_bits / 4;
            if (num_bits % 4) {
                num_nibbles++;
            }
            if (num_nibbles & 0b1) {
                num_nibbles++;
            }
            color_flip = true;
            printf("%s0x%s", "", "");
            for (i = num_nibbles * 4; i > 0; i -= 8) {
                printf("%s", (color_flip ? ui_term_color_num_float() : ui_term_color_reset()));
                color_flip = !color_flip;
                printf("%c", ascii_hex[((d >> (i - 4)) & 0x0F)]);
                printf("%c", ascii_hex[((d >> (i - 8)) & 0x0F)]);
            }
            printf("%s", ui_term_color_reset());
            break;
        case df_dec:
            printf("%d", d);
            break;
        case df_bin:
            j = num_bits % 4;
            if (j == 0) {
                j = 4;
            }
            color_flip = false;
            printf("%s0b%s", "", ui_term_color_num_float());
            for (i = 0; i < num_bits; i++) {
                if (!j) {
                    if (color_flip) {
                        color_flip = !color_flip;
                        printf("%s", ui_term_color_num_float());
                    } else {
                        color_flip = !color_flip;
                        printf("%s", ui_term_color_reset());
                    }
                    j = 4;
                }
                j--;

                mask = 1 << (num_bits - i - 1);
                if (d & mask) {
                    printf("1");
                } else {
                    printf("0");
                }
            }
            printf("%s", ui_term_color_reset());
            break;
    }

    if (num_bits != 8) {
        printf(".%d", num_bits);
    }

    // if( attributes->has_string)
    //{
    // printf(" %s\'%c\'", ui_term_color_reset(), d);
    //}
}

// represent d in the current display mode. If numbits=8 also display the ascii representation
void ui_format_print_number(uint32_t d) {
    uint32_t mask, i;

    if (system_config.num_bits < 32) {
        mask = ((1 << system_config.num_bits) - 1);
    } else {
        mask = 0xFFFFFFFF;
    }
    d &= mask;

    switch (system_config.display_format) {
        case 2:
            if (system_config.num_bits <= 8) { // DEC
                printf("%3d", d);
            } else if (system_config.num_bits <= 16) {
                printf("%5d", d);
            } else if (system_config.num_bits <= 24) {
                printf("%8d", d);
            } else if (system_config.num_bits <= 32) {
                printf("%10d", d);
            }
            break;
        /*case 2:	if(system_config.num_bits<=6)
                printf("0%02o", d);
            else if(system_config.num_bits<=12)
                printf("0%04o", d);
            else if(system_config.num_bits<=18)
                printf("0%06o", d);
            else if(system_config.num_bits<=24)
                printf("0%08o", d);
            else if(system_config.num_bits<=30)
                printf("0%010o", d);
            else if(system_config.num_bits<=32)
                printf("0%012o", d);
            break;*/
        case 3:
            printf("0b"); // binary
            for (i = 0; i < system_config.num_bits; i++) {
                mask = 1 << (system_config.num_bits - i - 1);
                if (d & mask) {
                    printf("1");
                } else {
                    printf("0");
                }
            }
            break;
        case 1: // hex
        default:
            if (system_config.num_bits <= 8) {
                printf("0x%02X", d);
            } else if (system_config.num_bits <= 16) {
                printf("0x%04X", d);
            } else if (system_config.num_bits <= 24) {
                printf("0x%06X", d);
            } else if (system_config.num_bits <= 32) {
                printf("0x%08X", d);
            }
            break;
    }
    if (system_config.num_bits != 8) {
        printf(".%d", system_config.num_bits);
    }
    if (system_config.num_bits == 8 && d >= ' ' && d <= '~') { // ASCII
        printf(" (\'%c\')", ((d >= 0x20) && (d < 0x7E) ? d : 0x20));
    }
}