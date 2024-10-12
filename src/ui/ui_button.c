#include <stdio.h>
#include "pico/stdlib.h"
#include <stdint.h>
#include "pirate.h"
#include "system_config.h"
#include "pirate/button.h"
#include "opt_args.h"       // File system related
#include "fatfs/ff.h"       // File system related
#include "pirate/storage.h" // File system related
#include "ui/ui_cmdln.h"    // This file is needed for the command line parsing functions
#include "ui/ui_term.h"     // Terminal functions
#include "ui/ui_process.h"
#include "usb_rx.h"
#include "usb_tx.h"
#include "bytecode.h"
#include "modes.h"

// non zero return value indicates error
bool ui_button_exec(void) {

    FIL fil;    /* File object needed for each open file */
    FRESULT fr; /* FatFs return code */
    char file[512];
    fr = f_open(&fil, "button.scr", FA_READ);
    if (fr != FR_OK) {
        printf("button.scr not found\r\n");
        return true;
    }

    printf("\r\n");
    while (f_gets(file, sizeof(file), &fil)) {
        if (file[0] == '#') { // comment line TODO: make a little more robust against leading whitespace
            printf("%s%s%s\r", ui_term_color_info(), file, ui_term_color_reset());
        } else {
            cmdln_next_buf_pos();
            if (system_config.subprotocol_name) {
                printf("%s%s-(%s)>%s ",
                       ui_term_color_prompt(),
                       modes[system_config.mode].protocol_name,
                       system_config.subprotocol_name,
                       ui_term_color_reset());
            } else {
                printf(
                    "%s%s>%s ", ui_term_color_prompt(), modes[system_config.mode].protocol_name, ui_term_color_reset());
            }
            for (uint32_t i = 0; i < sizeof(file); i++) {
                if (file[i] == '\r' || file[i] == '\n' || file[i] == '\0') {
                    break;
                }
                rx_fifo_add(&file[i]);
            }
            while (ui_term_get_user_input() != 0)
                ; // nothing left to shove in the command prompt
            printf("\r\n");
            bool error = ui_process_commands();
        }
    }
    printf("\r\n");
    /* Close the file */
    f_close(&fil);
    return false;
}