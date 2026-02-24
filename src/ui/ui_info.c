#include <stdio.h>
#include "pico/stdlib.h"
#include <stdint.h>
#include "pirate.h"
#include "system_config.h"
#include "command_struct.h"
#include "bytecode.h"
#include "modes.h"
#include "ui/ui_term.h"
#include "pirate/storage.h"
#include "ui/ui_const.h"
#include "ui/ui_info.h"
#include "pirate/mcu.h"
#include "pirate/amux.h"
#include "pirate/mem.h" //big buffer owner defines
#include "pirate/psu.h"
#include "ui/ui_pin_render.h"
#include "usb_tx.h"

// show voltages/pinstates — build into a local buffer, then push through tx_fifo
void ui_info_print_pin_names(void) {
    char tmp[512];
    uint32_t len = ui_pin_render_names(tmp, sizeof(tmp),
                                        PIN_RENDER_NEWLINE | PIN_RENDER_CLEAR_CELLS);
    tx_fifo_write(tmp, len);
}

void ui_info_print_pin_labels(void) {
    char tmp[512];
    uint32_t len = ui_pin_render_labels(tmp, sizeof(tmp),
                                         PIN_RENDER_NEWLINE | PIN_RENDER_CLEAR_CELLS);
    tx_fifo_write(tmp, len);
}

void ui_info_print_pin_voltage(bool refresh) {
    // take a reading from all adc channels
    // updates the values available at hw_pin_voltage_ordered[] pointer array
    amux_sweep();
    char tmp[512];
    pin_render_flags_t flags = PIN_RENDER_CLEAR_CELLS;
    if (!refresh) flags |= PIN_RENDER_NEWLINE;
    uint32_t len = ui_pin_render_values(tmp, sizeof(tmp), flags);
    tx_fifo_write(tmp, len);
}
