#include <stdio.h>
#include "pico/stdlib.h"
#include "pirate.h"

struct ioexp_func_t {
    void (*init)(void);
    void (*output_enable)(bool enable);
    void (*clear_set)(uint8_t *level, uint8_t *direction);
    void (*interrupt_enable)(uint16_t mask);
    bool (*read_bit)(uint8_t pin);
};

#if BP_HW_IOEXP_SPI
    #include "pirate/shift.h"
    static const struct ioexp_func_t ioexp_func = {
        .init = shift_init,
        .output_enable = shift_output_enable,
        .clear_set = shift_write_wait,
        .interrupt_enable = 0x00,
        .read_bit = 0x00,
    };
#elif BP_HW_IOEXP_I2C
    #include "pirate/xl9555.h"
    static const struct ioexp_func_t ioexp_func = {
        .init = xl9555_init,
        .output_enable = xl9555_output_enable,
        .clear_set = xl9555_write_wait,
        .interrupt_enable = xl9555_interrupt_enable,
        .read_bit = xl9555_read_bit,
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

//#if BP_HW_IOEXP_SPI
static uint8_t ioexp_dir[2] = { 0xff, 0xff };
/*#elif BP_HW_IOEXP_I2C
static uint8_t ioexp_dir[2] = { 0x00, 0xff &~(IOEXP_DISPLAY_BACKLIGHT | IOEXP_DISPLAY_RESET | IOEXP_CURRENT_RESET | IOEXP_CURRENT_EN | IOEXP_CURRENT_EN_OVERRIDE) };
#else
  #error "Platform not speficied in ioexpander.c"
#endif*/  
static uint8_t ioexp_level[2] = { 0, 0 };  

void ioexp_in_out(uint16_t input, uint16_t output) {
    ioexp_dir[0] &= ~((uint8_t)output);
    ioexp_dir[1] &= ~((uint8_t)(output >> 8));
    ioexp_dir[0] |= (uint8_t)input;
    ioexp_dir[1] |= (uint8_t)(input >> 8);
    ioexp_func.clear_set(ioexp_level, ioexp_dir);
}

void ioexp_clear_set(uint16_t clear_bits, uint16_t set_bits) {
    ioexp_level[0] &= ~((uint8_t)clear_bits);
    ioexp_level[1] &= ~((uint8_t)(clear_bits >> 8));
    ioexp_level[0] |= (uint8_t)set_bits;
    ioexp_level[1] |= (uint8_t)(set_bits >> 8);
    ioexp_func.clear_set(ioexp_level, ioexp_dir);
}

void ioexp_interrupt_enable(uint16_t mask) {
    ioexp_func.interrupt_enable(mask);
}

bool ioexp_read_bit(uint8_t pin) {
    return ioexp_func.read_bit(pin);
}
