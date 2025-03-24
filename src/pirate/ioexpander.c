#include <stdio.h>
#include "pico/stdlib.h"
#include "pirate.h"

struct ioexp_func_t {
    void (*init)(void);
    void (*output_enable)(bool enable);
    void (*clear_set)(uint8_t *level, uint8_t *direction);
};

#if BP_HW_IOEXP_595
    #include "pirate/shift.h"
    static const struct ioexp_func_t ioexp_func = {
        .init = shift_init,
        .output_enable = shift_output_enable,
        .clear_set = shift_write_wait
    };
#elif BP_HW_IOEXP_I2C
    #include "pirate/xl9555.h"
    static const struct ioexp_func_t ioexp_func = {
        .init = xl9555_init,
        .output_enable = xl9555_output_enable,
        .clear_set = xl9555_write_wait
    };
#else
    #error "Platform not speficied in ioexpander.c"
#endif  


void ioexp_init(void){
    ioexp_func.init();
}

void ioexp_output_enable(bool enable) {
    ioexp_func.output_enable(enable);
}

void ioexp_clear_set(uint16_t clear_bits, uint16_t set_bits) {
    static uint8_t ioexp_dir[2] = { 0, 0 };
    static uint8_t ioexp_level[2] = { 0, 0 };

    ioexp_level[1] &= ~((uint8_t)clear_bits);
    ioexp_level[0] &= ~((uint8_t)(clear_bits >> 8));
    ioexp_level[1] |= (uint8_t)set_bits;
    ioexp_level[0] |= (uint8_t)(set_bits >> 8);
    ioexp_func.clear_set(ioexp_level, ioexp_dir);
}
