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
#include "commands/global/freq.h"
#include "ui/ui_const.h"
#include "ui/ui_info.h"
#include "pirate/mcu.h"
#include "pirate/amux.h"
#include "pirate/mem.h" //big buffer owner defines

// show voltages/pinstates
void ui_info_print_pin_names(void) {
    // pin list
    for (int i = 0; i < HW_PINS; i++) {
        ui_term_color_text_background(hw_pin_label_ordered_color[i][0], hw_pin_label_ordered_color[i][1]);
        printf("\e[8X%d.%s\t", i + 1, hw_pin_label_ordered[i]);
    }
    printf("%s\r\n", ui_term_color_reset());
}

void ui_info_print_pin_labels(void) {
    uint8_t j = 0;
    // pin function

    // TODO: combine this with the version above in separate function
    if (system_config.psu) {
        uint32_t isense = ((hw_adc_raw[HW_ADC_CURRENT_SENSE]) * ((500 * 1000) / 4095));
        printf("%s%d.%d%smA\t",
               ui_term_color_num_float(),
               (isense / 1000),
               ((isense % 1000) / 100),
               ui_term_color_reset());
        j = 1;
    }

    // pin function
    for (int i = j; i < HW_PINS; i++) {
        printf("%s\t", system_config.pin_labels[i] == 0 ? "-" : (char*)system_config.pin_labels[i]);
    }
    printf("\r\n");
}

void ui_info_print_pin_voltage(bool refresh) {
    // pin voltage
    // take a reading from all adc channels
    // updates the values available at hw_pin_voltage_ordered[] pointer array
    amux_sweep();

    // the Vout pin
    // HACK: way too hardware dependent. This is getting to be a mess
    printf("%s%d.%d%sV\t",
           ui_term_color_num_float(),
           (*hw_pin_voltage_ordered[0]) / 1000,
           ((*hw_pin_voltage_ordered[0]) % 1000) / 100,
           ui_term_color_reset());

    // show state of IO pins
    for (uint i = 1; i < HW_PINS - 1; i++) {
        // TODO: global function for integer division
        // TODO: struct with pin type, label, units, and all the info buffered, just write it out
        // feature specific values (freq/pwm/V/?)
        if (system_config.freq_active & (0x01 << ((uint8_t)(i - 1))) ||
            system_config.pwm_active & (0x01 << ((uint8_t)(i - 1)))) {
            float freq_friendly_value;
            uint8_t freq_friendly_units;
            freq_display_hz(&system_config.freq_config[i - 1].period, &freq_friendly_value, &freq_friendly_units);
            printf("%s%3.1f%s%c\t",
                   ui_term_color_num_float(),
                   freq_friendly_value,
                   ui_term_color_reset(),
                   *ui_const_freq_labels_short[freq_friendly_units]);
        } else {
            printf("%s%d.%d%sV\t",
                   ui_term_color_num_float(),
                   (*hw_pin_voltage_ordered[i]) / 1000,
                   ((*hw_pin_voltage_ordered[i]) % 1000) / 100,
                   ui_term_color_reset());
        }
    }
    // ground pin
    printf("%s\r%s",
           GET_T(T_GND),
           !refresh ? "\n" : ""); // TODO: pin type struct and handle things like this automatically
}
