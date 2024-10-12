// #include <stdio.h>
#include <string.h>
#include "pico/stdlib.h"
#include "pirate.h"
#include "queue.h"
#include "usb_rx.h"
#include "usb_tx.h"
#include "pirate/bio.h"
#include "system_config.h"
#include "bytecode.h" //needed because modes.h has some functions that use it TODO: move all the opt args and bytecode stuff to a single helper file
#include "opt_args.h" //needed for same reason as bytecode and needs same fix
#include "modes.h"

void script_send(const char* c, uint32_t len) {
    for (uint8_t i = 0; i < len; i++) {
        bin_tx_fifo_put(c[i]);
    }
}

void script_reset(void) {
    modes[system_config.mode].protocol_cleanup(); // switch to HiZ
    modes[0].protocol_setup_exc();                // disables power supply etc.

    // POWER|PULLUP|AUX|MOSI|CLK|MISO|CS
    static const char pin_labels[][5] = { "BIO0", "BIO1", "BIO2", "BIO3", "BIO4", "BIO5", "BIO6", "BIO7" };
    system_bio_claim(true, BIO0, BP_PIN_MODE, pin_labels[0]);
    system_bio_claim(true, BIO1, BP_PIN_MODE, pin_labels[1]);
    system_bio_claim(true, BIO2, BP_PIN_MODE, pin_labels[2]);
    system_bio_claim(true, BIO3, BP_PIN_MODE, pin_labels[3]);
    system_bio_claim(true, BIO4, BP_PIN_MODE, pin_labels[4]);
    system_bio_claim(true, BIO5, BP_PIN_MODE, pin_labels[5]);
    system_bio_claim(true, BIO6, BP_PIN_MODE, pin_labels[6]);
    system_bio_claim(true, BIO7, BP_PIN_MODE, pin_labels[7]);
}