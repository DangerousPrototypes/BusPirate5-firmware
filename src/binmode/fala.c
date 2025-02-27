
#include <pico/stdlib.h>
#include <string.h>
#include "hardware/clocks.h"
#include "pirate.h"
#include "queue.h"
#include "usb_rx.h"
#include "usb_tx.h"
#include "system_config.h"
// #include "bytecode.h" //needed because modes.h has some functions that use it TODO: move all the opt args and
// bytecode stuff to a single helper file #include "command_struct.h" //needed for same reason as bytecode and needs same fix
// #include "modes.h"
#include "binmode/binmodes.h"
#include "binmode/logicanalyzer.h"
#include "binmode/binio.h"
#include "binmode/fala.h"
#include "tusb.h"
#include "ui/ui_term.h"
#include "command_struct.h"
#include "bytecode.h"
#include "modes.h"

FalaConfig fala_config = { .base_frequency = 1000000, .oversample = 8 };

// set the sampling rate
void fala_set_freq(uint32_t freq) {
    // store in fala struct for easy oversample adjustment
    fala_config.base_frequency = freq;
}

// set oversampling rate
void fala_set_oversample(uint32_t oversample_rate) {
    // set the sampling rate
    fala_config.oversample = oversample_rate;
}

// TODO: implement
void fala_set_triggers(uint8_t trigger_pin, uint8_t trigger_level) {
    // set the trigger pin and level
}

// start the logic analyzer
void fala_start(void) {
    // configure and arm the logic analyzer
    fala_config.actual_sample_frequency = logic_analyzer_configure(
        fala_config.base_frequency * fala_config.oversample, DMA_BYTES_PER_CHUNK * LA_DMA_COUNT, 0x00, 0x00, false, false);
    logic_analyzer_arm(false);
}

// stop the logic analyzer
// but keeps data available for dump
void fala_stop(void) {
    logic_analyser_done();
}

// output printed to user terminal
void fala_print_result(void) {
    // get samples count
    //uint32_t fala_samples = logic_analyzer_get_end_ptr();
    uint32_t fala_samples = logic_analyzer_get_samples_from_zero();

    if(fala_samples > (DMA_BYTES_PER_CHUNK * LA_DMA_COUNT)){
        printf(
        "\r\n%sLogic analyzer:%s invalid sample count\r\n", ui_term_color_info(), ui_term_color_reset());
    }else{
        // show some info about the logic capture
        printf(
        "\r\n%sLogic analyzer:%s %d samples captured\r\n", ui_term_color_info(), ui_term_color_reset(), fala_samples);
    }

    // DEBUG: print an 8 line logic analyzer graph of the last 80 samples
    if (fala_config.debug_level > 1) {
        printf("%s[DEBUG] Logic Analyzer Graph\r\n", ui_term_color_info());
        fala_samples = fala_samples < 80 ? fala_samples : 80;
        for (int bits = 0; bits < 8; bits++) {
            logic_analyzer_reset_ptr();
            uint8_t val;
            for (int i = 0; i < fala_samples; i++) {
                logic_analyzer_dump(&val);
                if (val & (1 << bits)) {
                    printf("-"); // high
                } else {
                    printf("_"); // low
                }
            }
            printf("\r\n");
        }
        printf("%s[DEBUG] End of Logic Analyzer Graph%s\r\n", ui_term_color_info(), ui_term_color_reset());
    }
}

/****************************************************/
// Hooks for notifying applications of new captures

void (*fala_notify_hooks[2])();

// test if anything is registered
bool fala_has_hook(void) {
    for (int i = 0; i < 2; i++) {
        if (fala_notify_hooks[i] != NULL) {
            return true;
        }
    }
    return false;
}
// register a hook to be called when the logic analyzer is done
// set up the logic analyzer if it hasn't been already
bool fala_notify_register(void (*hook)()) {
    if (!fala_has_hook()) {
        if (!logicanalyzer_setup()) {
            printf("\r\nLogic analyzer setup error, out of memory?\r\n");
            return false;
        }
    }

    for (int i = 0; i < 2; i++) {
        if (fala_notify_hooks[i] == NULL) {
            fala_notify_hooks[i] = hook;
            return true;
        }
    }
    return false;
}

// remove a hook from the list
// if no hooks are left, clean up the logic analyzer
void fala_notify_unregister(void (*hook)()) {
    for (int i = 0; i < 2; i++) {
        if (fala_notify_hooks[i] == hook) {
            fala_notify_hooks[i] = NULL;
        }
    }

    if (!fala_has_hook()) {
        logic_analyzer_cleanup();
    }
}

// call all the hooks
void fala_notify_hook(void) {
    if (fala_has_hook()) {
        fala_print_result();
    }

    for (int i = 0; i < 2; i++) {
        if (fala_notify_hooks[i] != NULL) {
            fala_notify_hooks[i]();
        }
    }
}

// start the logic analyzer if a hook is registered
void fala_start_hook(void) {
    for (int i = 0; i < 2; i++) {
        if (fala_notify_hooks[i] != NULL) {
            fala_start();
            return;
        }
    }
}

// stop the logic analyzer if a hook is registered
void fala_stop_hook(void) {
    for (int i = 0; i < 2; i++) {
        if (fala_notify_hooks[i] != NULL) {
            fala_stop();
            return;
        }
    }
}

// mode change, configure speed with mode function
void fala_mode_change_hook(void) {
    fala_set_freq(modes[system_config.mode].protocol_get_speed());
    fala_set_oversample(8);
    fala_config.actual_sample_frequency = logic_analyzer_compute_actual_sample_frequency(fala_config.base_frequency * fala_config.oversample, NULL);
    if (fala_has_hook()) {
        printf("\r\n%sLogic analyzer speed:%s %dHz (%dx oversampling)\r\n",
               ui_term_color_info(),
               ui_term_color_reset(),
               fala_config.actual_sample_frequency,
               fala_config.oversample);
        printf(
            "%sUse the 'logic' command to change capture settings%s\r\n", ui_term_color_info(), ui_term_color_reset());
    }
}
