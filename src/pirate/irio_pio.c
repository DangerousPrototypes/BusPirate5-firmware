#include <stdio.h>
#include "pico/stdlib.h"
#include "pirate.h"
#include "hardware/pio.h"
#include "pio_config.h"
#include "irio.pio.h"

static struct _pio_config pio_config_low;
static struct _pio_config pio_config_high;

void pio_irio_init(uint pin_demod, uint pin_pio2pio, uint freq) {
    // bool success = pio_claim_free_sm_and_add_program_for_gpio_range(&i2c_program, &pio_config.pio, &pio_config.sm,
    // &pio_config.offset, dir_sda, 10, true); hard_assert(success);
    pio_config_low.pio = PIO_MODE_PIO;
    pio_config_low.sm = 0;
    pio_config_low.program = &ir_in_low_counter_program;
    pio_config_low.offset = pio_add_program(pio_config_low.pio, pio_config_low.program);
    ir_in_low_counter_program_init(pio_config_low.pio, pio_config_low.sm, pio_config_low.offset, pin_demod, pin_pio2pio, freq);

    pio_config_high.pio = PIO_MODE_PIO;
    pio_config_high.sm = 1;
    pio_config_high.program = &ir_in_high_counter_program;
    pio_config_high.offset = pio_add_program(pio_config_high.pio, pio_config_high.program);
    ir_in_high_counter_program_init(pio_config_high.pio, pio_config_high.sm, pio_config_high.offset, pin_pio2pio, freq);


    #ifdef BP_PIO_SHOW_ASSIGNMENT
        printf("PIO: pio=%d, sm=%d, offset=%d\r\n", PIO_NUM(pio_config_high.pio), pio_config_high.sm, pio_config_high.offset);
    #endif
}

void pio_irio_cleanup(void) {
    // pio_remove_program_and_unclaim_sm(&i2c_program, pio_config.pio, pio_config.sm, pio_config.offset);
    pio_remove_program(pio_config_low.pio, pio_config_low.program, pio_config_low.offset);
    pio_remove_program(pio_config_high.pio, pio_config_high.program, pio_config_high.offset);
}

void pio_irio_get(void){
    uint32_t low = 0;
    uint32_t high = 0;
    if(!pio_sm_is_rx_fifo_empty(pio_config_low.pio, pio_config_low.sm)){
        low = (uint32_t)pio_sm_get(pio_config_low.pio, pio_config_low.sm);
        printf("L:%u--", low);
    }
    if(!pio_sm_is_rx_fifo_empty(pio_config_high.pio, pio_config_high.sm)){
        high = (uint32_t)pio_sm_get(pio_config_high.pio, pio_config_high.sm);
        printf("H:%u--", high);
    }

}
