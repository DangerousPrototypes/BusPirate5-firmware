#include <stdio.h>
#include <math.h>
#include <string.h>
#include "pico/stdlib.h"
#include "hardware/dma.h"
#include "hardware/clocks.h"
#include "pirate.h"
#include "system_config.h"
#include "pirate/bio.h"
#include "opt_args.h"
#include "logicanalyzer.h"
#include "hardware/pio.h"
#include "logicanalyzer.pio.h"
#include "pirate/mem.h"
#include "hardware/structs/bus_ctrl.h"
#include "ui/ui_term.h"
#include "usb_rx.h"
#include "pirate/storage.h"
#include "pirate/rgb.h"
#include "pico/multicore.h"
#include "pirate/amux.h"
#include "ui/ui_cmdln.h"
#include "pirate/intercore_helpers.h"

enum logicanalyzer_status { LA_IDLE = 0, LA_ARMED_INIT, LA_ARMED, LA_CAPTURE };

static void restart_dma();

int la_dma[LA_DMA_COUNT];
uint8_t* la_buf;
uint32_t la_ptr = 0;
uint32_t la_ptr_reset = 0;
volatile uint8_t la_status = LA_IDLE;
volatile uint32_t la_sm_done = false;

PIO pio = pio0;
uint sm = 0;
static uint offset = 0;
static const struct pio_program* pio_program_active;

void logicanalyzer_reset_led(void) {
    icm_core0_send_message_synchronous(BP_ICM_DISABLE_RGB_UPDATES);
}

uint32_t logic_analyzer_get_start_ptr(uint32_t sample_count) {
    return ((la_ptr_reset - sample_count) & 0x1ffff);
}

uint32_t logic_analyzer_get_end_ptr(void) {
    return la_ptr_reset;
}

uint32_t logic_analyzer_get_current_ptr(void){
    return la_ptr;
}

void logic_analyzer_reset_ptr(void) {
    la_ptr = la_ptr_reset;
}

void logic_analyzer_dump(uint8_t* txbuf) {
    *txbuf = la_buf[la_ptr];
    la_ptr--;
    la_ptr &= 0x1ffff;
}

uint8_t logic_analyzer_read_ptr(uint32_t read_pointer) {
    return la_buf[read_pointer];
}

//this will probably need a mutex
void logic_analyser_done(void) {
    // turn off stuff!
    pio_interrupt_clear(pio, 0);
    irq_set_enabled(PIO0_IRQ_0, false);
    // irq_set_enabled(pio_get_dreq(pio, sm, false), false);
    irq_remove_handler(PIO0_IRQ_0, logic_analyser_done);
    pio_sm_set_enabled(pio, sm, false);
    // pio_clear_instruction_memory(pio);
    if (pio_program_active) {
        pio_remove_program(pio, pio_program_active, offset);
        pio_program_active = 0;
    }

    busy_wait_ms(1);

    int tail_dma = -1;
    uint32_t tail_offset;

    // find the final sample
    for (uint8_t i = 0; i < count_of(la_dma); i++) {
        if (dma_channel_is_busy(la_dma[i])) {
            tail_dma = i;
            break;
        }
    }
    // error, return
    if (tail_dma == -1) {
        return;
    }

    // transfer count is the words remaining in the stalled transfer, dma deincrements on start (-1)
    int32_t tail = DMA_BYTES_PER_CHUNK - dma_channel_hw_addr(la_dma[tail_dma])->transfer_count - 1;

    // add the preceding chunks of DMA to find the location in the array
    // ready to dump
    la_ptr_reset=la_ptr = ((DMA_BYTES_PER_CHUNK * tail_dma) + tail);

    rgb_set_all(0x00, 0xff, 0); //,0x00FF00 green for dump
    la_sm_done = true;
}

uint32_t logic_analyzer_get_dma_tail(void) {
    uint8_t tail_dma = 0xff;
    for (uint8_t i = 0; i < count_of(la_dma); i++) {
        if (dma_channel_is_busy(la_dma[i])) {
            tail_dma = i;
            break;
        }
    }

    if (tail_dma > count_of(la_dma)) {
        // hum
        return 0;
    }

    return dma_channel_hw_addr(la_dma[tail_dma])->transfer_count;
}

bool logic_analyzer_is_done(void) {
    static int32_t tail;
    uint8_t tail_dma = 0xff;

    if (la_status == LA_ARMED_INIT) {
        tail = logic_analyzer_get_dma_tail();
        la_status = LA_ARMED;
    }

    if (la_status == LA_ARMED && tail != logic_analyzer_get_dma_tail()) {
        la_status = LA_CAPTURE;
        rgb_set_all(0xab, 0x7f, 0); // 0xAB7F00 yellow for capture in progress.
    }
    if (la_sm_done) {
        la_status = LA_IDLE;
    }
    return (la_status == LA_IDLE);
}

void restart_dma() {
    dma_channel_config la_dma_config;
    for (uint8_t i = 0; i < count_of(la_dma); i++) {
        dma_channel_abort(la_dma[i]);
        la_dma_config = dma_channel_get_default_config(la_dma[i]);
        channel_config_set_read_increment(&la_dma_config, false);          // read fixed PIO address
        channel_config_set_write_increment(&la_dma_config, true);          // write to circular buffer
        channel_config_set_transfer_data_size(&la_dma_config, DMA_SIZE_8); // we have 8 IO pins
        channel_config_set_dreq(&la_dma_config,
                                pio_get_dreq(pio, sm, false)); // &pio0_hw->rxf[sm] paces the rate of transfer
        channel_config_set_ring(&la_dma_config, true, 15);     // loop at 2 * 8 bytes

        int la_dma_next = (i + 1 < count_of(la_dma)) ? la_dma[i + 1] : la_dma[0];
        channel_config_set_chain_to(&la_dma_config, la_dma_next); // chain to next DMA

        dma_channel_configure(la_dma[i],
                              &la_dma_config,
                              (volatile uint8_t*)&la_buf[DMA_BYTES_PER_CHUNK * i],
                              &pio->rxf[sm],
                              DMA_BYTES_PER_CHUNK,
                              false);
    }

    // start the first channel, will pause for data from PIO
    dma_channel_start(la_dma[0]);
}

bool logic_analyzer_configure(
    float freq, uint32_t samples, uint32_t trigger_mask, uint32_t trigger_direction, bool edge) {
    la_sm_done = false;
    memset(la_buf, 0, DMA_BYTES_PER_CHUNK * LA_DMA_COUNT);

    // This can be useful for debugging. The position of sampling always start at the beginning of the buffer
    restart_dma();

    if (pio_program_active) {
        pio_remove_program(pio, pio_program_active, offset);
        pio_program_active = 0;
    }

    uint8_t trigger_pin = 0;
    bool trigger_ok = false;
    if (trigger_mask) {
        for (uint8_t i = 0; i < 8; i++) {
            if (trigger_mask & 1u << i) {
                trigger_pin = i;
                trigger_ok = true;
                break; // use first masked pin
            }
        }
    }

    if (trigger_ok) {
        if (trigger_direction & 1u << trigger_pin) // high level trigger program
        {
            offset = pio_add_program(pio, &logicanalyzer_high_trigger_program);
            pio_program_active = &logicanalyzer_high_trigger_program;
            logicanalyzer_high_trigger_program_init(pio, sm, offset, LA_BPIO0, LA_BPIO0 + trigger_pin, freq, edge);
        } else // low level trigger program
        {
            offset = pio_add_program(pio, &logicanalyzer_low_trigger_program);
            pio_program_active = &logicanalyzer_low_trigger_program;
            logicanalyzer_low_trigger_program_init(pio, sm, offset, LA_BPIO0, LA_BPIO0 + trigger_pin, freq, edge);
        }
    } else { // else no trigger program
        offset = pio_add_program(pio, &logicanalyzer_no_trigger_program);
        pio_program_active = &logicanalyzer_no_trigger_program;
        logicanalyzer_no_trigger_program_init(pio, sm, offset, LA_BPIO0, freq);
    }

    // interrupt on done notification
    pio_interrupt_clear(pio, 0);
    pio_set_irq0_source_enabled(pio, pis_interrupt0, true);
    irq_set_exclusive_handler(PIO0_IRQ_0, logic_analyser_done);
    irq_set_enabled(PIO0_IRQ_0, true);
    irq_set_enabled(pio_get_dreq(pio, sm, false), true);
    // write sample count and enable sampling
    pio_sm_put_blocking(pio, sm, samples - 1);
    return true;
}

void logic_analyzer_arm(bool led_indicator_enable) {
    la_status = LA_ARMED_INIT;
    if (led_indicator_enable) {
        icm_core0_send_message_synchronous(BP_ICM_ENABLE_RGB_UPDATES);
        busy_wait_ms(5);
        rgb_set_all(0xff, 0, 0); // RED LEDs for armed
    }
    pio_sm_set_enabled(pio, sm, true);
}

bool logic_analyzer_cleanup(void) {

    for (uint8_t i = 0; i < count_of(la_dma); i++) {
        dma_channel_cleanup(la_dma[i]);
        dma_channel_unclaim(la_dma[i]);
    }

    // pio_clear_instruction_memory(pio);
    if (pio_program_active) {
        pio_remove_program(pio, pio_program_active, offset);
        pio_program_active = 0;
    }

    mem_free(la_buf);

    logicanalyzer_reset_led();
    return true;
}

bool logicanalyzer_setup(void) {

    la_buf = mem_alloc(DMA_BYTES_PER_CHUNK * LA_DMA_COUNT, 0);
    if (!la_buf || ((uint)la_buf != ((uint)la_buf & ~((1 << 15) - 1)))) {
        // printf("Failed to allocate buffer. Is the scope running?\r\n");
        return false;
    }

    // high bus priority to the DMA
    bus_ctrl_hw->priority = BUSCTRL_BUS_PRIORITY_DMA_W_BITS | BUSCTRL_BUS_PRIORITY_DMA_R_BITS;

    for (uint8_t i = 0; i < count_of(la_dma); i++) {
        la_dma[i] = dma_claim_unused_channel(true);
    }

    restart_dma();
    return true;
}