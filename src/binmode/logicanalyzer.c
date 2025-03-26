#include <stdio.h>
#include <math.h>
#include <string.h>
#include "pico/stdlib.h"
#include "hardware/dma.h"
#include "hardware/clocks.h"
#include "pirate.h"
#include "system_config.h"
#include "pirate/bio.h"
#include "command_struct.h"
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
#include "pio_config.h"

static struct _pio_config pio_config;

enum logicanalyzer_status {
    LA_IDLE = 0,
    LA_ARMED_INIT,
    LA_ARMED,
    LA_CAPTURE
};

static void restart_dma();
int la_dma_data_channel;
int la_dma_control_channel;
volatile uint8_t* la_buf;
uint32_t la_ptr = 0;
uint32_t la_ptr_reset = 0;
uint8_t la_base_pin = LA_BPIO0;
volatile uint8_t la_status = LA_IDLE;
volatile uint32_t la_sm_done = false;
bool irq_handler_installed = true;
bool status_leds_enabled = false;
//this is a sample count variable for FALA and triggerless captures
// for triggers, it is the number of samples after 0 
uint32_t samples_from_zero = 0;

// PIO pio = pio0;
// uint sm = 0;
// static uint offset = 0;
// static const struct pio_program* pio_program_active=0;

void logic_analyzer_set_base_pin(uint8_t base_pin) {
    la_base_pin = base_pin;
}

void logic_analyzer_enable_status_leds(bool enable) {
    status_leds_enabled = enable;
}

void logicanalyzer_reset_led(void) {
    icm_core0_send_message_synchronous(BP_ICM_DISABLE_RGB_UPDATES);
}

//returns the number of samples in the buffer counting from the 0 sample
//is equivalent to the number of samples when no trigger is set
uint32_t logic_analyzer_get_samples_from_zero(void) {
    return samples_from_zero;
}

uint32_t logic_analyzer_get_start_ptr(uint32_t sample_count) {
    return ((la_ptr_reset - sample_count) % LA_BUFFER_SIZE);
}

uint32_t logic_analyzer_get_end_ptr(void) {
    return la_ptr_reset;
}

uint32_t logic_analyzer_get_current_ptr(void) {
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

// this will probably need a mutex
void logic_analyser_done(void) {
    // turn off stuff!
    pio_interrupt_clear(pio_config.pio, 0);
    irq_set_enabled(PIO0_IRQ_0 + (PIO_NUM(pio_config.pio) * 2), false);
    // irq_set_enabled(pio_get_dreq(pio_config.pio, pio_config.sm, false), false);
    if(irq_handler_installed){
        irq_remove_handler(PIO0_IRQ_0 + (PIO_NUM(pio_config.pio) * 2), logic_analyser_done);
    }
    pio_sm_set_enabled(pio_config.pio, pio_config.sm, false);

    if (pio_config.program) {
        // pio_remove_program_and_unclaim_sm(pio_config.program, pio_config.pio, pio_config.sm, pio_config.offset);
        pio_remove_program(pio_config.pio, pio_config.program, pio_config.offset);
        pio_config.program = 0;
    }

    busy_wait_ms(1);

 
    // transfer count is the words remaining in the stalled transfer, dma deincrements on start (-1)
    int32_t tail = LA_BUFFER_SIZE - dma_channel_hw_addr(la_dma_data_channel)->transfer_count - 1;

    // add the preceding chunks of DMA to find the location in the array
    // ready to dump
    samples_from_zero = la_ptr_reset = la_ptr = tail;

    if(tail==-1){
        samples_from_zero = LA_BUFFER_SIZE;
    }
    if(status_leds_enabled){
        rgb_set_all(0x00, 0xff, 0); //,0x00FF00 green for dump
    }
    la_sm_done = true;
}

uint32_t logic_analyzer_get_dma_tail(void) {
    return dma_channel_hw_addr(la_dma_data_channel)->transfer_count;
}

bool logic_analyzer_is_done(void) {
    static int32_t tail;
    if (la_status == LA_ARMED_INIT) {
        tail = logic_analyzer_get_dma_tail();
        la_status = LA_ARMED;
    }

    if (la_status == LA_ARMED && tail != logic_analyzer_get_dma_tail()) {
        la_status = LA_CAPTURE;
        if(status_leds_enabled){
            rgb_set_all(0xab, 0x7f, 0); // 0xAB7F00 yellow for capture in progress.
        }
    }
    if (la_sm_done) {
        la_status = LA_IDLE;
    }
    return (la_status == LA_IDLE);
}

const uint32_t la_buff_size[1] = {LA_BUFFER_SIZE};

void restart_dma() {
    dma_channel_config la_dma_data_config;
    dma_channel_config la_dma_control_config;
    dma_channel_abort(la_dma_control_channel);
    dma_channel_abort(la_dma_data_channel);
    la_dma_data_config = dma_channel_get_default_config(la_dma_data_channel);
    la_dma_control_config = dma_channel_get_default_config(la_dma_control_channel);

    channel_config_set_transfer_data_size(&la_dma_control_config, DMA_SIZE_32);
    channel_config_set_read_increment(&la_dma_control_config, false);
    channel_config_set_write_increment(&la_dma_control_config, false);
    dma_channel_configure(la_dma_control_channel,
                          &la_dma_control_config,
                          &dma_hw->ch[la_dma_data_channel].al1_transfer_count_trig, // write address
                          &la_buff_size[0],                          // read address
                          1,                                         // Halt after each control block
                          false                                      // Don't start yet
    );
    channel_config_set_transfer_data_size(&la_dma_data_config, DMA_SIZE_8);
    channel_config_set_read_increment(&la_dma_data_config, false);
    channel_config_set_write_increment(&la_dma_data_config, true);
    channel_config_set_dreq(
        &la_dma_data_config,
        pio_get_dreq(pio_config.pio, pio_config.sm, false)); // &pio0_hw->rxf[sm] paces the rate of transfer

    channel_config_set_chain_to(&la_dma_data_config,
                                la_dma_control_channel); // Trigger ctrl_chan when data_chan completes
    dma_channel_configure(la_dma_data_channel,
                          &la_dma_data_config,
                          &la_buf[0],                             // write address
                          &pio_config.pio->rxf[pio_config.sm], // read address
                          0,                      // filled by the control channel
                          false                                // Don't start yet
    );

    // start the control channel, the data channel will pause for data from PIO
    dma_channel_start(la_dma_control_channel);
}

uint32_t logic_analyzer_configure(
    float freq, uint32_t samples, uint32_t trigger_mask, uint32_t trigger_direction, bool edge, bool interrupt) {
    uint32_t actual_frequency = 0;
    la_sm_done = false;
    memset((uint8_t*)la_buf, 0, LA_BUFFER_SIZE);

    irq_handler_installed=interrupt;

    // This can be useful for debugging. The position of sampling always start at the beginning of the buffer
    // restart_dma(); //this moved to below because the PIO isn't yet assigned

    if (pio_config.program) {
        // pio_remove_program_and_unclaim_sm(pio_config.program, pio_config.pio, pio_config.sm, pio_config.offset);
        pio_remove_program(pio_config.pio, pio_config.program, pio_config.offset);
        pio_config.program = 0;
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

    pio_config.pio = PIO_LOGIC_ANALYZER_PIO;
    pio_config.sm = PIO_LOGIC_ANALYZER_SM;

    if (trigger_ok) {
        if (trigger_direction & 1u << trigger_pin) // high level trigger program
        {
            // bool success = pio_claim_free_sm_and_add_program_for_gpio_range(&logicanalyzer_high_trigger_program,
            // &pio_config.pio, &pio_config.sm, &pio_config.offset, LA_BASE_PIN, 8, true); hard_assert(success);
            pio_config.program = &logicanalyzer_high_trigger_program;
            pio_config.offset = pio_add_program(pio_config.pio, pio_config.program);
            actual_frequency = logicanalyzer_high_trigger_program_init(
                pio_config.pio, pio_config.sm, pio_config.offset, la_base_pin, la_base_pin + trigger_pin, freq, edge);
        } else // low level trigger program
        {
            // bool success = pio_claim_free_sm_and_add_program_for_gpio_range(&logicanalyzer_low_trigger_program,
            // &pio_config.pio, &pio_config.sm, &pio_config.offset, LA_BASE_PIN, 8, true); hard_assert(success);
            pio_config.program = &logicanalyzer_low_trigger_program;
            pio_config.offset = pio_add_program(pio_config.pio, pio_config.program);
            actual_frequency = logicanalyzer_low_trigger_program_init(
                pio_config.pio, pio_config.sm, pio_config.offset, la_base_pin, la_base_pin + trigger_pin, freq, edge);
        }
    } else { // else no trigger program
        // bool success = pio_claim_free_sm_and_add_program_for_gpio_range(&logicanalyzer_no_trigger_program,
        // &pio_config.pio, &pio_config.sm, &pio_config.offset, LA_BASE_PIN, 8, true); hard_assert(success);
        pio_config.program = &logicanalyzer_no_trigger_program; // move this before to simplify add program
        pio_config.offset = pio_add_program(pio_config.pio, pio_config.program);
        actual_frequency =
            logicanalyzer_no_trigger_program_init(pio_config.pio, pio_config.sm, pio_config.offset, la_base_pin, freq);
    }
#ifdef BP_PIO_SHOW_ASSIGNMENT
    printf("pio %d, sm %d, offset %d\n", PIO_NUM(pio_config.pio), pio_config.sm, pio_config.offset);
#endif

    restart_dma(); // do after PIO and SM are assigned

    // interrupt on done notification
    pio_interrupt_clear(pio_config.pio, 0);
    pio_set_irq0_source_enabled(pio_config.pio, pis_interrupt0, true);
    if(irq_handler_installed) {
        irq_set_exclusive_handler(PIO0_IRQ_0 + (PIO_NUM(pio_config.pio) * 2), logic_analyser_done);
        irq_set_enabled(PIO0_IRQ_0 + (PIO_NUM(pio_config.pio) * 2), true);
        irq_set_enabled(pio_get_dreq(pio_config.pio, pio_config.sm, false), true);
    }
    // write sample count and enable sampling
    pio_sm_put_blocking(pio_config.pio, pio_config.sm, samples - 1);
    return actual_frequency;
}

void logic_analyzer_arm(bool led_indicator_enable) {
    la_status = LA_ARMED_INIT;
    status_leds_enabled = led_indicator_enable;
    if (led_indicator_enable) {
        icm_core0_send_message_synchronous(BP_ICM_ENABLE_RGB_UPDATES);
        busy_wait_ms(5);
        rgb_set_all(0xff, 0, 0); // RED LEDs for armed
    }
    pio_sm_set_enabled(pio_config.pio, pio_config.sm, true);
}

bool logic_analyzer_cleanup(void) {
    dma_channel_cleanup(la_dma_control_channel);
    dma_channel_cleanup(la_dma_data_channel);
    dma_channel_unclaim(la_dma_data_channel);
    dma_channel_unclaim(la_dma_control_channel);

    // pio_clear_instruction_memory(pio);
    if (pio_config.program) {
        // pio_remove_program_and_unclaim_sm(pio_config.program, pio_config.pio, pio_config.sm, pio_config.offset);
        pio_remove_program(pio_config.pio, pio_config.program, pio_config.offset);
        pio_config.program = 0;
    }

    mem_free((uint8_t*)la_buf);

    logicanalyzer_reset_led();
    return true;
}

bool logicanalyzer_setup(void) {
    //note that there isn't any alignment constraint for this buffer
    la_buf = mem_alloc(LA_BUFFER_SIZE, 0);


    // high bus priority to the DMA
    bus_ctrl_hw->priority = BUSCTRL_BUS_PRIORITY_DMA_W_BITS | BUSCTRL_BUS_PRIORITY_DMA_R_BITS;

    la_dma_data_channel = dma_claim_unused_channel(true);
    la_dma_control_channel = dma_claim_unused_channel(true);

    pio_config.program = 0;

    // restart_dma();
    return true;
}

uint32_t logic_analyzer_compute_actual_sample_frequency(float desired_frequency, float* div_out)
{
    float div = clock_get_hz(clk_sys) / (desired_frequency * 2); // 2 instructions per sample, run twice as fast as requested sampling rate
    div = (div >= 1.0) ? ((div < 10.0) ? floorf(div) : div) : 1.0;
    if (div_out) {
        *div_out = div;
    }
    return clock_get_hz(clk_sys) / (2 * div);
}