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

// show voltages/pinstates
void ui_info_print_pin_names(void) {
    ui_pin_render_names(NULL, 0);
}

void ui_info_print_pin_labels(void) {
    ui_pin_render_labels(NULL, 0);
}

void ui_info_print_pin_voltage(bool refresh) {
    // take a reading from all adc channels
    // updates the values available at hw_pin_voltage_ordered[] pointer array
    amux_sweep();
    ui_pin_render_values(NULL, 0, refresh);
}
