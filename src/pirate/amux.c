#include <stdio.h>
#include "pico/stdlib.h"
#include "hardware/adc.h"
#include "pirate.h"
#include "pirate/shift.h"
#include "pirate/amux.h"
#include "command_struct.h"
#include "display/scope.h"
#include "hardware/sync.h"
#include "pico/lock_core.h"

// lock_core_t core;
spin_lock_t* adc_spin_lock;
uint adc_spin_lock_num;

// is executed on the next average calculation
bool reset_adc_average = false;

// gives protected access to adc (core safe)
void adc_busy_wait(bool enable) {
    static bool busy = false;

    if (!enable) {
        busy = false;
        return;
    }
    do {
        spin_lock_unsafe_blocking(adc_spin_lock);
        if (busy) {
            spin_unlock_unsafe(adc_spin_lock);
        } else {
            busy = true;
            spin_unlock_unsafe(adc_spin_lock);
            return;
        }
    } while (true);
}

void amux_init(void) {
    if (scope_running) { // scope is using the analog subsystem
        return;
    }

    adc_init();
    adc_gpio_init(AMUX_OUT);
    adc_gpio_init(CURRENT_SENSE);
    // setup the spinlock for adc arbitration
    adc_spin_lock_num = spin_lock_claim_unused(true);
    adc_spin_lock = spin_lock_init(adc_spin_lock_num);
    reset_adc_average = true;
}

// select AMUX input source, use the channel defines from the platform header
// only effects the 4067CD analog mux, you cannot get the current measurement from here
bool amux_select_input(uint16_t channel) {
    if (scope_running) {
        return false; // scope is using the analog subsystem
    }
    // clear the amux control bits, set the amux channel bits
    #if BP_HW_IOEXP_595
        shift_clear_set((0b1111 << 1), (channel << 1) & 0b11110, true);
    #elif (BP_VER == 6 || BP_VER == 7)
        // uint64_t value=(uint64_t)(channel<<AMUX_S0);
        // uint64_t mask=(uint64_t)(0b1111<<AMUX_S0);
        // gpio_put_masked(mask, value);
        gpio_put(AMUX_S0, (channel >> 0) & 1);
        gpio_put(AMUX_S1, (channel >> 1) & 1);
        gpio_put(AMUX_S2, (channel >> 2) & 1);
        gpio_put(AMUX_S3, (channel >> 3) & 1);
    #elif (BP_VER == 8)
       #error "AMUX not implemented for BP8"
    #else
        #error "Platform not speficied in amux.c"
    #endif
    return true;
}

bool amux_select_bio(uint8_t bio) {
    if (scope_running) {
        return false; // scope is using the analog subsystem
    }
    amux_select_input(bufio2amux(bio));
    return true;
}

// read from AMUX using channel list in platform header file
uint32_t amux_read(uint8_t channel) {
    if (scope_running) { // scope is using the analog subsystem
        return 0;
    }
    adc_busy_wait(true);
    adc_select_input(AMUX_OUT_ADC);
    amux_select_input(HW_ADC_MUX_GND); // to clear any charge from a floating pin
    amux_select_input((uint16_t)channel);
    busy_wait_us(60);
    uint32_t ret = adc_read();
    adc_busy_wait(false);
    return ret;
}

uint32_t amux_read_present_channel(void) {
    if (scope_running) { // scope is using the analog subsystem
        return 0;
    }
    adc_busy_wait(true);
    uint32_t ret = adc_read();
    adc_busy_wait(false);
    return ret;
}

// read from AMUX using BIO pin number
uint32_t amux_read_bio(uint8_t bio) {
    if (scope_running) {
        return 0; // scope is using the analog subsystem
    }
    return amux_read(bufio2amux(bio));
}

// this is actually on a different ADC and not the AMUX
// but this is the best place for it I think
// voltage is not /2 so we can use the full range of the ADC
uint32_t amux_read_current(void) {
    if (scope_running) {
        return 0; // scope is using the analog subsystem
    }
    adc_busy_wait(true);
    adc_select_input(CURRENT_SENSE_ADC);
    uint32_t ret = adc_read();
    adc_busy_wait(false);
    return ret;
}

// read all the AMUX channels and the current sense
// place into the global arrays hw_adc_raw and hw_adc_voltage
void amux_sweep(void) {
    if (scope_running) { // scope is using the analog subsystem
        return;
    }
    adc_busy_wait(true);
    adc_select_input(AMUX_OUT_ADC);
    for (int i = 0; i < HW_ADC_MUX_COUNT; i++) {
        amux_select_input(HW_ADC_MUX_GND); // to clear any charge from a floating pin
        busy_wait_us(10);
        amux_select_input(i);
        busy_wait_us(60);
        hw_adc_raw[i] = adc_read();
        // hw_adc_voltage[i]=hw_adc_to_volts_x2(i); //these are X2 because a resistor divider /2
    }
    amux_select_input(HW_ADC_MUX_GND); // to clear any charge from a floating pin
    adc_select_input(CURRENT_SENSE_ADC);
    busy_wait_us(60);
    hw_adc_raw[HW_ADC_CURRENT_SENSE] = (adc_read() + hw_adc_raw[HW_ADC_CURRENT_SENSE]) / 2;
    adc_busy_wait(false);
    // do these outside the ADC spin lock
    for (int i = 0; i < HW_ADC_MUX_COUNT; i++) {
        hw_adc_voltage[i] = hw_adc_to_volts_x2(i); // these are X2 because a resistor divider /2
        
        if (reset_adc_average)
            hw_adc_avgsum_voltage[i] = hw_adc_voltage[i]*ADC_AVG_TIMES;
        else
        {
            // calculate the rolling average
            hw_adc_avgsum_voltage[i]-=get_adc_average(hw_adc_avgsum_voltage[i]);
            hw_adc_avgsum_voltage[i]+=hw_adc_voltage[i];
        }
    }
    if (reset_adc_average)
        reset_adc_average = false;
    hw_adc_voltage[HW_ADC_CURRENT_SENSE] = hw_adc_to_volts_x1(HW_ADC_CURRENT_SENSE);
}
