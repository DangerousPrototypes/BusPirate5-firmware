/**
 * @file amux.c
 * @brief Analog multiplexer (AMUX) control and ADC management
 * 
 * This file implements the analog multiplexer system for Bus Pirate, which
 * allows reading voltages from multiple sources through a single ADC channel.
 * 
 * The AMUX system provides:
 * - Voltage measurement on all 8 BIO pins
 * - Power supply voltage monitoring
 * - Current sense measurement
 * - Multi-core safe ADC access with spinlocks
 * - Averaging for noise reduction
 * 
 * Hardware:
 * - CD4067 16-channel analog multiplexer
 * - RP2040 ADC (12-bit)
 * - Voltage dividers for different ranges
 * 
 * @author Bus Pirate Project
 * @date 2024-2026
 */

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

spin_lock_t* adc_spin_lock;    /**< Spinlock for multi-core ADC access protection */
uint adc_spin_lock_num;        /**< Spinlock number */

bool reset_adc_average = false; /**< Flag to reset ADC averaging */

/**
 * @brief Provide protected access to ADC (core-safe)
 * 
 * Uses spinlock to ensure only one core accesses the ADC at a time.
 * When enable=true, blocks until ADC is available.
 * 
 * @param enable true to acquire lock, false to release
 * 
 * @note Must call with enable=false when done to release lock
 * @warning Blocking call when enable=true
 */
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

/**
 * @brief Initialize analog multiplexer and ADC subsystem
 * 
 * Sets up:
 * - ADC peripheral
 * - GPIO pins for ADC input
 * - Spinlock for multi-core protection
 * - Averaging reset flag
 * 
 * @note Does nothing if scope is running (scope owns ADC)
 */
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

/**
 * @brief Select AMUX input channel
 * 
 * Configures the CD4067 analog multiplexer to route the specified
 * input channel to the ADC.
 * 
 * @param channel Channel number (0-15, platform-specific defines)
 * @return true if successful, false if scope is running
 * 
 * @note Does not affect current sense input (separate ADC channel)
 * @note Channel definitions are platform-specific (see platform headers)
 */
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

/**
 * @brief Select AMUX input using BIO pin number
 * 
 * Convenience function that maps BIO pin numbers to AMUX channels.
 * 
 * @param bio BIO pin number (0-7)
 * @return true if successful, false if scope is running
 */
bool amux_select_bio(uint8_t bio) {
    if (scope_running) {
        return false; // scope is using the analog subsystem
    }
    amux_select_input(bufio2amux(bio));
    return true;
}

/**
 * @brief Read voltage from specified AMUX channel
 * 
 * Selects the channel, waits for settling, and performs ADC conversion.
 * Uses spinlock for multi-core safety.
 * 
 * @param channel AMUX channel number
 * @return 12-bit ADC reading (0-4095), or 0 if scope is running
 * 
 * @note Briefly grounds the channel before measurement to clear charge
 * @note Includes 60μs settling time
 */
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
