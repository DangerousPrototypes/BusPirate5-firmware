
#include <pico/stdlib.h>
#include <string.h>
#include "hardware/clocks.h"
#include "pirate.h"
#include "queue.h"
#include "usb_rx.h"
#include "usb_tx.h"
#include "system_config.h"
// #include "bytecode.h" //needed because modes.h has some functions that use it TODO: move all the opt args and
// bytecode stuff to a single helper file #include "opt_args.h" //needed for same reason as bytecode and needs same fix
// #include "modes.h"
#include "binmode/binmodes.h"
#include "binmode/logicanalyzer.h"
#include "binmode/binio.h"
#include "binmode/fala.h"
#include "tusb.h"

#define MAX_UART_PKT 64
#define CDC_INTF 1

FalaConfig fala_config = {
    .base_frequency = 1000000,
    .oversample = 8
};

// binmode name to display
const char fala_name[] = "Follow along logic analyzer";

// binmode setup on mode start
void fala_setup(void) {
    system_config.binmode_usb_rx_queue_enable = false;
    system_config.binmode_usb_tx_queue_enable = false;
    if (!logicanalyzer_setup()) {
        printf("Logic analyzer setup error, out of memory?\r\n");
    }
}
// binmode cleanup on exit
void fala_cleanup(void) {
    logic_analyzer_cleanup();
    system_config.binmode_usb_rx_queue_enable = true;
    system_config.binmode_usb_tx_queue_enable = true;
}

// start the logic analyzer
void fala_start(void) {
    // configure and arm the logic analyzer
    logic_analyzer_configure(fala_config.base_frequency * fala_config.oversample, DMA_BYTES_PER_CHUNK * LA_DMA_COUNT, 0x00, 0x00, false);
    logic_analyzer_arm(false);
}

// stop the logic analyzer
// but keeps data available for dump
void fala_stop(void) {
    logic_analyser_done();
}

void fala_reset(void) {
    // reset the logic analyzer
    // logic_analyzer_cleanup();
}

// set the sampling rate
void fala_set_freq(uint32_t freq) {
    // store in fala struct for easy oversample adjustment
    fala_config.base_frequency = freq;
    printf("\r\nFollow Along Logic Analyzer capture: %dHz (%dx oversampling)\r\n",
           fala_config.base_frequency * fala_config.oversample,
           fala_config.oversample);
    printf("Use the 'logic' command to change capture settings.\r\n");
}

// set oversampling rate
void fala_set_oversample(uint32_t oversample_rate) {
    // set the sampling rate
    fala_config.oversample = oversample_rate;
}

// send notification packet at end of capture
void fala_notify(void) {
    logic_analyzer_reset_ptr(); // put pointer back to end of data buffer (last sample first)
    uint32_t fala_samples = logic_analyzer_get_ptr();
    // send notification packet
    //$FALADATA;{pins};{trigger pins};{trigger mask};{edge trigger (bool)}; {capture speed in hz};{samples};{pre-samples
    //(for trigger line)};
    if (tud_cdc_n_connected(CDC_INTF)) {
        uint8_t buf[MAX_UART_PKT];
        uint8_t len = snprintf(buf,
                               sizeof(buf),
                               "$FALADATA;%d;%d;%d;%c;%d;%d;%d;\n",
                               8,
                               0,
                               0,
                               'N',
                               fala_config.base_frequency * fala_config.oversample,
                               fala_samples,
                               0);
        if (tud_cdc_n_write_available(CDC_INTF) >= sizeof(buf)) {
            tud_cdc_n_write(CDC_INTF, buf, len);
            tud_cdc_n_write_flush(CDC_INTF);
        }
    }
}

// output printed to user terminal
void fala_print_result(void) {
    // send notification packet
    fala_notify();
    // get samples count
    logic_analyzer_reset_ptr(); // put pointer back to end of data buffer (last sample first)
    uint32_t fala_samples = logic_analyzer_get_ptr();
    // show some info about the logic capture
    printf("Logic Analyzer: %d samples captured\r\n", fala_samples);

    // DEBUG: print an 8 line logic analyzer graph of the last 80 samples
    if(fala_config.debug_level>1){
        printf("[DEBUG] Logic Analyzer Graph:\r\n");
        fala_samples = fala_samples < 80 ? fala_samples : 80;
        for (int bits = 0; bits < 8; bits++) {
            logic_analyzer_reset_ptr();
            uint8_t val;
            for (int i = 0; i < fala_samples; i++) {
                logicanalyzer_dump(&val);
                if (val & (1 << bits)) {
                    printf("-"); // high
                } else {
                    printf("_"); // low
                }
            }
            printf("\r\n");
        }
    }
}

static uint32_t fala_dump_count;

static uint fala_tx8(uint8_t* buf, uint len) {
    uint32_t i, count;
    count = fala_dump_count;
    for (i = 0; i < len && count > 0; i++, count--) {
        logicanalyzer_dump(&buf[i]);
    }
    fala_dump_count -= i;
    return i;
}

enum fala_statemachine { FALA_IDLE = 0, FALA_DUMP };

void fala_service(void) {
    static enum fala_statemachine state = FALA_IDLE;

    if (!tud_cdc_n_connected(CDC_INTF)) {
        return;
    }

    switch (state) {
        case FALA_IDLE:

            if (!tud_cdc_n_available(CDC_INTF)) {
                break;
            }

            uint8_t buf[64];
            uint8_t len = tud_cdc_n_read(CDC_INTF, buf, sizeof(buf));

            if (len) {
                for (uint8_t i = 0; i < len; i++) {
                    switch (buf[i]) {
                        case '?':
                            fala_notify();
                            break;
                        case '+':
                            // dump the buffer
                            logic_analyzer_reset_ptr(); // put pointer back to end of data buffer (last sample first)
                            fala_dump_count = logic_analyzer_get_ptr();
                            state = FALA_DUMP;
                            break;
                    }
                }
            }
            break;
        case FALA_DUMP:
            if (tud_cdc_n_write_available(CDC_INTF) >= sizeof(buf)) {
                uint8_t len = fala_tx8(buf, sizeof(buf));
                tud_cdc_n_write(CDC_INTF, buf, len);
                tud_cdc_n_write_flush(CDC_INTF);
            }
            if (fala_dump_count == 0) {
                state = FALA_IDLE;
            }
            break;
    }
}
