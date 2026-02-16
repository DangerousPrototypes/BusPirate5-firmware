#include <stdint.h>
#include "pico/stdlib.h"
#include "pirate.h"
#include "system_config.h"
#include "command_struct.h"
#include "ui/ui_term.h"
#include "ui/ui_prompt.h"
#include "ui/ui_parse.h"
#include "lib/bp_args/bp_cmd.h"
#include "ui/ui_help.h"
#include "pico/multicore.h"
#include "pirate/storage.h"
#include "pirate/pullup.h"
#include "pirate/button.h"
#include "pirate/psu.h"
#include "pirate/bio.h"
#include "pirate/amux.h"
#include "display/scope.h"
#include "pirate/intercore_helpers.h"
#include "commands/global/bug.h"
#include "usb_rx.h"

#define SELF_TEST_LOW_LIMIT 300

bool selftest_rp2350_e9_fix(void){
    bool fail = false;
    printf("TEST RP2350 E9 BUG FIX\r\n");
    for (uint8_t i = 0; i < BIO_MAX_PINS; i++) {
        if(!bug_e9_seems_fixed(true, i, false)){
            printf("ERROR: BIO%d shows E9 behavior\r\n", i);
            fail = true;
        }
    }
    bio_init();
    return fail;
}

bool selftest_format_nand(void) {
    struct prompt_result presult;
    bool confirm;
    if (!system_config.storage_available) {
        printf("No file system!\r\nFormat the Bus Pirate NAND flash?\r\nALL DATA WILL BE DESTROYED.");
        ui_prompt_bool(&presult, false, false, false, &confirm);
        printf("\r\n");
        if (confirm) {
            uint8_t fr = storage_format();
            if (fr) {
                storage_file_error(fr);
                printf("FORMAT NAND FLASH: ERROR! 不好\r\n\r\n");
                return true;
            } else {
                printf("FORMAT NAND FLASH: OK\r\n\r\n");
                return false;
            }
        }
    }
    return false;
}

bool selftest_adc(void) {
    // USB/VOLTAGE/ADC/AMUX test: read the (USB) power supply
    amux_sweep();
    printf("ADC SUBSYSTEM: VUSB ");
    if (hw_adc_voltage[HW_ADC_MUX_VUSB] < (4.5 * 1000)) {
        printf("NOT DETECTED (%1.2fV). ERROR!\r\n", (float)(hw_adc_voltage[HW_ADC_MUX_VUSB] / (float)1000));
        return true;
    } else {
        printf(" %1.2fV OK\r\n", (float)(hw_adc_voltage[HW_ADC_MUX_VUSB] / (float)1000));
    }
    return false;
}

bool selftest_flash(void) {
    // TF flash card check: was TF flash card detected?
    printf("FLASH STORAGE: ");
    if (storage_mount() == 0) {
        printf("OK\r\n");
    } else {
        printf("NOT DETECTED. ERROR!\r\n");
        return true;
    }
    return false;
}

bool selftest_psu(float volts, float current, bool current_limit_override) {
    // psu test
    // psu to 1.8, 2.5, 3.3, 5.0 volt test
    printf("PSU ENABLE: ");
    uint32_t result = psu_enable(volts, current, current_limit_override, 100);
    if (result) {
        printf("PSU ERROR CODE %d\r\n", result);
        return true;
    } else {
        printf("OK\r\n");
    }
    return false;
}

bool selftest_psu_opamp(void) {
    // detect REV10 failure of op-amp
    printf("VREG==VOUT: ");
    amux_sweep();
    // at 3.3v, the VREGout should be close  to VOUT/VREF
    if (hw_adc_voltage[HW_ADC_MUX_VREG_OUT] > hw_adc_voltage[HW_ADC_MUX_VREF_VOUT]) {
        if (hw_adc_voltage[HW_ADC_MUX_VREG_OUT] - hw_adc_voltage[HW_ADC_MUX_VREF_VOUT] > 100) {
            printf(" %d > %d ERROR!!\r\n", hw_adc_voltage[HW_ADC_MUX_VREG_OUT], hw_adc_voltage[HW_ADC_MUX_VREF_VOUT]);
            return true;
        } else {
            printf(" %d = %d OK\r\n", hw_adc_voltage[HW_ADC_MUX_VREG_OUT], hw_adc_voltage[HW_ADC_MUX_VREF_VOUT]);
        }
    } else {
        if (hw_adc_voltage[HW_ADC_MUX_VREF_VOUT] - hw_adc_voltage[HW_ADC_MUX_VREG_OUT] > 100) {
            printf(" %d < %d ERROR!!\r\n", hw_adc_voltage[HW_ADC_MUX_VREG_OUT], hw_adc_voltage[HW_ADC_MUX_VREF_VOUT]);
            return true;
        } else {
            printf(" %d = %d OK\r\n", hw_adc_voltage[HW_ADC_MUX_VREG_OUT], hw_adc_voltage[HW_ADC_MUX_VREF_VOUT]);
        }
    }
    return false;
}

bool selftest_bio_float(void) {
    uint32_t temp1;
    uint32_t fails = 0;
    printf("BIO FLOAT TEST (SHOULD BE 0/<0.%dV)\r\n", SELF_TEST_LOW_LIMIT / 10);
    for (uint8_t pin = 0; pin < BIO_MAX_PINS; pin++) {
        // read pin input (should be low)
        amux_sweep();
        temp1 = bio_get(pin);
        printf("BIO%d FLOAT: %d/%1.2fV ", pin, temp1, (float)(*hw_pin_voltage_ordered[pin + 1] / (float)1000));
        if (temp1 || ((*hw_pin_voltage_ordered[pin + 1]) > SELF_TEST_LOW_LIMIT)) {
            printf("ERROR!\r\n");
            fails++;
        } else {
            printf("OK\r\n");
        }
    }
    if (fails) {
        return true;
    }
    return false;
}

bool selftest_bio_high(void) {
    uint32_t temp1;
    uint32_t fails = 0;
    printf("BIO HIGH TEST (SHOULD BE >3.00V)\r\n");
    for (uint8_t pin = 0; pin < BIO_MAX_PINS; pin++) {
        bio_output(pin);
        bio_put(pin, 1);
        busy_wait_ms(1); // give it some time
        // read pin ADC, should be ~3.3v
        amux_sweep();
        printf("BIO%d HIGH: %1.2fV ", pin, (float)(*hw_pin_voltage_ordered[pin + 1] / (float)1000));
        if (*hw_pin_voltage_ordered[pin + 1] < 3 * 1000) {
            printf("ERROR!\r\n");
            fails++;
        } else {
            printf("OK\r\n");
        }
        // check other pins for possible shorts
        for (uint8_t i = 0; i < BIO_MAX_PINS; i++) {
            if (pin == i) {
                continue;
            }
            temp1 = bio_get(i);
            if (temp1 || *hw_pin_voltage_ordered[i + 1] > SELF_TEST_LOW_LIMIT) {
                printf("BIO%d SHORT->BIO%d (%d/%1.2fV): ERROR!\r\n",
                       pin,
                       i,
                       temp1,
                       (float)(*hw_pin_voltage_ordered[i + 1] / (float)1000));
                fails++;
            }
        }
        bio_input(pin);
    }
    if (fails) {
        return true;
    }
    return false;
}

bool selftest_bio_low(void) {
    uint32_t fails = 0;
    printf("BIO LOW TEST (SHOULD BE <0.%dV)\r\n", SELF_TEST_LOW_LIMIT / 10);
    // start with all pins high, ground one by one
    for (uint8_t i = 0; i < BIO_MAX_PINS; i++) {
        bio_output(i);
        bio_put(i, 1);
    }
    for (uint8_t pin = 0; pin < BIO_MAX_PINS; pin++) {
        bio_put(pin, 0);
        busy_wait_ms(1); // give it some time
        // read pin ADC, should be ~3.3v
        amux_sweep();
        printf("BIO%d LOW: %1.2fV ", pin, (float)(*hw_pin_voltage_ordered[pin + 1] / (float)1000));
        if (*hw_pin_voltage_ordered[pin + 1] > SELF_TEST_LOW_LIMIT) {
            printf("ERROR!\r\n");
            fails++;
        } else {
            printf("OK\r\n");
        }
        // check other pins for possible shorts
        for (uint8_t i = 0; i < BIO_MAX_PINS; i++) {
            if (pin == i) {
                continue;
            }
            if (*hw_pin_voltage_ordered[i + 1] < 3 * 1000) {
                printf("BIO%d SHORT->BIO%d (%1.2fV): ERROR!\r\n",
                       pin,
                       i,
                       (float)(*hw_pin_voltage_ordered[i + 1] / (float)1000));
                fails++;
            }
        }
        bio_put(pin, 1);
    }
    bio_init();
    if (fails) {
        return true;
    }
    return false;
}

bool selftest_pullup_high(void) {
    uint32_t temp1;
    uint32_t fails = 0;
    printf("BIO PULL-UP HIGH TEST (SHOULD BE 1/>3.00V)\r\n");
    pullup_enable();
    // start with all pins grounded, then float one by one
    for (uint8_t i = 0; i < BIO_MAX_PINS; i++) {
        bio_output(i);
        bio_put(i, 0);
    }
    for (uint8_t pin = 0; pin < BIO_MAX_PINS; pin++) {
        bio_input(pin);  // let float high
        busy_wait_ms(5); // give it some time
        // read pin input (should be low)
        amux_sweep();
        temp1 = bio_get(pin);
        printf("BIO%d PU-HIGH: %d/%1.2fV ", pin, temp1, (float)(*hw_pin_voltage_ordered[pin + 1] / (float)1000));
        if (!temp1 || ((*hw_pin_voltage_ordered[pin + 1]) < 3 * 1000)) {
            printf("ERROR!\r\n");
            fails++;
        } else {
            printf("OK\r\n");
        }
        // check other pins for possible shorts
        for (uint8_t i = 0; i < BIO_MAX_PINS; i++) {
            if (pin == i) {
                continue;
            }
            if (*hw_pin_voltage_ordered[i + 1] > SELF_TEST_LOW_LIMIT) {
                printf("BIO%d SHORT->BIO%d (%1.2fV): ERROR!\r\n",
                       pin,
                       i,
                       (float)(*hw_pin_voltage_ordered[i + 1] / (float)1000));
                fails++;
            }
        }
        bio_output(pin);
        bio_put(pin, 0);
    }
    bio_init();
    if (fails) {
        return true;
    }
    return false;
}

bool selftest_pullup_low(void) {
    uint32_t temp1;
    uint32_t fails = 0;
    printf("BIO PULL-UP LOW TEST (SHOULD BE <0.%dV)\r\n", SELF_TEST_LOW_LIMIT / 10);
    pullup_enable();
    // start with all pins floating, then ground one by one
    for (uint8_t pin = 0; pin < BIO_MAX_PINS; pin++) {
        bio_output(pin); // let float high
        bio_put(pin, 0);
        busy_wait_ms(5); // give it some time
        // read pin input (should be high)
        amux_sweep();
        printf("BIO%d PU-LOW: %1.2fV ", pin, (float)(*hw_pin_voltage_ordered[pin + 1] / (float)1000));
        if (((*hw_pin_voltage_ordered[pin + 1]) > SELF_TEST_LOW_LIMIT)) {
            printf("ERROR!\r\n");
            fails++;
        } else {
            printf("OK\r\n");
        }
        // check other pins for possible shorts
        for (uint8_t i = 0; i < BIO_MAX_PINS; i++) {
            if (pin == i) {
                continue;
            }
            temp1 = bio_get(i);
            if (temp1 == 0 || *hw_pin_voltage_ordered[i + 1] < 3 * 1000) {
                printf("BIO%d SHORT->BIO%d (%d/%1.2fV): ERROR!\r\n",
                       pin,
                       i,
                       temp1,
                       (float)(*hw_pin_voltage_ordered[i + 1] / (float)1000));
                fails++;
            }
        }
        bio_input(pin);
    }
    if (fails) {
        return true;
    }
    return false;
}

bool selftest_current_override(void) {
    // 1. Test with current override
    pullup_enable();
    printf("CURRENT OVERRIDE: ");
    uint32_t result = psu_enable(3.3, 0, true, 100);
    if (result == 0) {
        printf("OK\r\n");
    } else {
        printf("PPSU CODE %d, ERROR!\r\n", result);
        return true;
    }
    psu_disable();
    return false;
}

bool selftest_current_limit(void) {
    // use pullups to trigger current limit, set and reset
    printf("CURRENT LIMIT TEST: ");
    for (uint8_t pin = 0; pin < BIO_MAX_PINS; pin++) {
        bio_output(pin);
        bio_put(pin, 0);
    }
    // 2. Enable wth 0 limit and check the return error code of the PSU
    uint32_t result = psu_enable(3.3, 0, false, 100);
    if (result == PSU_ERROR_FUSE_TRIPPED) {
        printf("OK\r\n");
    } else {
        uint i;
        for (i = 0; i < 5; i++) {
            amux_sweep();
            printf("PPSU CODE %d, ADC: %d, ERROR!\r\n", result, hw_adc_raw[HW_ADC_MUX_CURRENT_DETECT]);
            busy_wait_ms(200);
        }
        return true;
    }
    return false;
}

bool selftest_button(void) {
    // debounce value selected somewhat arbitrarily
    static const uint32_t DEBOUNCE_DELAY_MS = 100;
    // prompt to push button
    printf("PUSH BUTTON (SPACE TO SKIP): ");
    // wait for button to be pressed
    while (true){
        if(button_get(0)){
            break;
        }   
        //or press key to exit
        char c;
        if (rx_fifo_try_get(&c) && (c==' ')) {
            printf("SKIPPED\r\n");
            return true;
        }
    }

    busy_wait_ms(DEBOUNCE_DELAY_MS);
    printf("PUSH BUTTON (SPACE TO SKIP): ");
    // then wait for button to be released
    while (true){
        if(!button_get(0)){
            break;
        }   
        //or press key to exit
        char c;
        if (rx_fifo_try_get(&c) && (c==' ')) {
            printf("SKIPPED\r\n");
            return true;
        }
    }
    printf("OK\r\n");
    return false;
}

// test that the logic analyzer chip is mounted and with no shorts
#if BP_HW_FALA_BUFFER
bool selftest_la_bpio(void) {
    uint32_t temp1, fails = 0, iopin = 0;
    printf("LA_BPIO TEST (SHOULD BE 1)\r\n");
    for (uint8_t lapin = LA_BPIO0; lapin < (LA_BPIO7 + 1); lapin++) {
        bio_output(iopin);
        bio_put(iopin, 1);
        busy_wait_ms(1); // give it some time
        temp1 = gpio_get(lapin);
        printf("LA_BPIO%d HIGH: %d ", iopin, temp1);
        if (!temp1) {
            printf("ERROR!\r\n");
            fails++;
        } else {
            printf("OK\r\n");
        }
        printf("LA_BPIO: 0:%d 1:%d 2:%d 3:%d 4:%d 5:%d 6:%d 7:%d\r\n",
               gpio_get(LA_BPIO0),
               gpio_get(LA_BPIO1),
               gpio_get(LA_BPIO2),
               gpio_get(LA_BPIO3),
               gpio_get(LA_BPIO4),
               gpio_get(LA_BPIO5),
               gpio_get(LA_BPIO6),
               gpio_get(LA_BPIO7));

        // check other pins for possible shorts
        for (uint8_t i = LA_BPIO0; i < (LA_BPIO7 + 1); i++) {
            if (lapin == i) {
                continue;
            }
            temp1 = gpio_get(i);
            if (temp1) {
                printf("LA_BBIO%d SHORT->BIO%d (%d): ERROR!\r\n", lapin, i - LA_BPIO0, temp1);
                fails++;
            }
        }
        bio_input(iopin);
        iopin++;

        /*
        while(true){
            pullup_enable();
            busy_wait_ms(1);
            printf("LA_BPIO: 0:%d 1:%d 2:%d 3:%d 4:%d 5:%d 6:%d 7:%d\r\n", gpio_get(LA_BPIO0), gpio_get(LA_BPIO1),
        gpio_get(LA_BPIO2), gpio_get(LA_BPIO3), gpio_get(LA_BPIO4), gpio_get(LA_BPIO5), gpio_get(LA_BPIO6),
        gpio_get(LA_BPIO7));
        }
        */
    }
    if (fails) {
        return true;
    }
    return false;
}
#endif

void cmd_selftest(void) {
    uint32_t fails = 0;

    if (scope_running) { // scope is using the analog subsystem
        printf("Can't self test when the scope is using the analog subsystem - use the 'd 1' command to switch to the "
               "default display\r\n");
        return;
    }

    // only allow selftest in HiZ mode
    if (system_config.mode != 0) {
        printf("Selftest only available in HiZ mode\r\n");
        return;
    }

    // REV10 + check status of NAND flash
    #ifdef BP_HW_STORAGE_NAND
        if (selftest_format_nand()) {
            fails++;
        }
    #endif

    printf("SELF TEST STARTING\r\nDISABLE IRQ: ");
    icm_core0_send_message_synchronous(BP_ICM_DISABLE_LCD_UPDATES);
    busy_wait_ms(500);
    printf("OK\r\n");

    // init pins
    bio_init();

    // USB/VOLTAGE/ADC/AMUX test: read the (USB) power supply
    if (selftest_adc()) {
        fails++;
    }

    // TF flash card check: was TF flash card detected?
    if (selftest_flash()) {
        fails++;
    }

    // psu test
    if (selftest_psu(3.3, 100, true)) {
        fails++;
    }

    // detect REV10 failure of op-amp
    if (selftest_psu_opamp()) {
        fails++;
    }

    // BIO float test
    if (selftest_bio_float()) {
        fails++;
    }

    // BIO high test
    if (selftest_bio_high()) {
        fails++;
    }

    // BIO low test
    if (selftest_bio_low()) {
        fails++;
    }

    // LA_BPIO test
    #if BP_HW_FALA_BUFFER
        if (selftest_la_bpio()) {
            fails++;
        }
    #endif
    #if (RPI_PLATFORM == RP2350)
        if(selftest_rp2350_e9_fix()){
            printf("RP2350 E9 silicon bug detected, try \"bug e9 -a\" to confirm\r\n");
            fails++;
        }
    #endif

    // BIO pull-up high test
    if (selftest_pullup_high()) {
        fails++;
    }

    // BIO pull-up low test
    if (selftest_pullup_low()) {
        fails++;
    }

    // PSU test with current override
    if (selftest_current_override()) {
        fails++;
    }

    // PSU test with current limit
    if (selftest_current_limit()) {
        fails++;
    }

    pullup_disable();
    bio_init();
    psu_disable();

    // prompt to push button
    if (selftest_button()) {
        fails++;
    }

    if (fails) {
        printf("\r\nERRORS: %d\r\nFAIL! :(\r\n", fails);
    } else {
        printf("\r\n\r\nPASS :)\r\n");
    }

    psu_status.enabled = false;
    psu_status.error_overcurrent = false;
    psu_status.error_undervoltage = false;
    psu_status.error_pending = false;
    system_config.error = false;

    // enable system interrupts
    icm_core0_send_message_synchronous(BP_ICM_ENABLE_LCD_UPDATES);
}

static const char* const usage[] = {
    "~\t[-h(elp)]",
    "Run self-test:%s ~",
    "Warning:%s Disconnect any devices before running self-test",
    "Warning:%s Self-test is only available in HiZ mode",
};

const bp_command_def_t cmd_selftest_def = {
    .name         = "~",
    .description  = T_HELP_GCMD_SELFTEST,
    .actions      = NULL,
    .action_count = 0,
    .opts         = NULL,
    .usage        = usage,
    .usage_count  = count_of(usage),
};

void cmd_selftest_handler(struct command_result* res) {
    // check help
    if (bp_cmd_help_check(&cmd_selftest_def, res->help_flag)) {
        return;
    }

    cmd_selftest();
}