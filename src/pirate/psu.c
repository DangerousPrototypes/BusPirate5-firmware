#include <stdio.h>
#include "pico/stdlib.h"
#include "hardware/pwm.h"
#include "hardware/clocks.h"
#include "pirate.h"
#include "pirate/shift.h"
#include "pirate/psu.h"
#include "pirate/amux.h"

#define PWM_TOP 14000 // 0x30D3

// voltage settings
#define PSU_V_LOW 800   // millivolts
#define PSU_V_HIGH 5000 // millivolts
#define PSU_V_RANGE ((PSU_V_HIGH * 10) - (PSU_V_LOW * 10))

// current settings
#define PSU_I_LOW 0    // mA
#define PSU_I_HIGH 500 // mA
#define PSU_I_RANGE ((PSU_I_HIGH * 10000) - (PSU_I_LOW * 10000))

#define BP_HW_PSU_USE_SHIFT_REGISTERS (BP_VER == 5 || BP_VER == XL5)

struct psu_status_t psu_status;

static void psu_fuse_reset(void) {
    // reset current trigger
    #if (BP_VER == 5 || BP_VER == XL5)
        shift_clear_set_wait(CURRENT_RESET, 0); // low to activate the pnp
        busy_wait_ms(1);
        shift_clear_set_wait(0, CURRENT_RESET); // high to disable    
    #elif (BP_VER == 6)
        gpio_put(CURRENT_RESET, 0);
        busy_wait_ms(1);
        gpio_put(CURRENT_RESET, 1);
    #elif (BP_VER == 7 && BP_REV == 0)
        gpio_put(CURRENT_RESET, 0);
        busy_wait_ms(1);
        gpio_put(CURRENT_RESET, 1);
    #else
        #error "Platform not speficied in psu.c"
    #endif
}

// TODO: rename this function, it actually controls if the current limit circuit is connected to the VREG
void psu_vreg_enable(bool enable) {
    #if (BP_VER == 5 || BP_VER == XL5)
        if (enable) {
            shift_clear_set_wait(CURRENT_EN, 0); // low is on (PNP)
        } else {
            shift_clear_set_wait(0, CURRENT_EN); // high is off
        }    
    #elif (BP_VER == 6)
        gpio_put(CURRENT_EN, !enable);
    #elif (BP_VER == 7 && BP_REV == 0)
        gpio_put(CURRENT_EN, !enable);
    #else
        #error "Platform not speficied in psu.c"
    #endif
}

void psu_current_limit_override(bool enable) {
    #if (BP_VER == 5 || BP_VER == XL5)
        if (enable) {
            shift_clear_set_wait(0, CURRENT_EN_OVERRIDE);
        } else {
            shift_clear_set_wait(CURRENT_EN_OVERRIDE, 0);
        }
    #elif (BP_VER == 6)
        gpio_put(CURRENT_EN_OVERRIDE, enable);
    #elif (BP_VER == 7 && BP_REV == 0)
        gpio_put(CURRENT_EN_OVERRIDE, enable);
    #else
        #error "Platform not speficied in psu.c"
    #endif
}

void psu_set_v(float volts, struct psu_status_t* psu) {
    uint32_t psu_v_per_bit = ((PSU_V_RANGE) / PWM_TOP);
    uint32_t vset = (uint32_t)((float)volts * 10000);
    vset = vset - (PSU_V_LOW * 10);
    vset = vset / psu_v_per_bit;
    if (vset > PWM_TOP) {
        vset = PWM_TOP + 1;
    }

    psu->voltage_requested = volts;
    psu->voltage_actual_i = ((vset * psu_v_per_bit) + (PSU_V_LOW * 10));
    psu->voltage_actual = (float)((float)((vset * psu_v_per_bit) + (PSU_V_LOW * 10)) / (float)10000);
    // inverse for VREG margining
    psu->voltage_dac_value = (PWM_TOP - vset);
}

void psu_set_i(float current, struct psu_status_t* psu) {
    float psu_i_per_bit = ((float)(PSU_I_RANGE) / (float)PWM_TOP);
    float iset = (float)((float)current * 10000);
    iset /= psu_i_per_bit;
    float iact = (float)((float)iset * psu_i_per_bit) / 10000;

    psu->current_dac_value = (uint32_t)iset;
    psu->current_requested = current;
    psu->current_actual_i = (uint32_t)((float)iset * (float)psu_i_per_bit);
    psu->current_actual = iact;
}

void psu_dac_set(uint16_t v_dac, uint16_t i_dac) {
    #if (BP_VER == 5 || BP_VER == XL5 || BP_VER == 6 || (BP_VER == 7 && BP_REV > 0))
        uint slice_num = pwm_gpio_to_slice_num(PSU_PWM_VREG_ADJ);
        uint v_chan_num = pwm_gpio_to_channel(PSU_PWM_VREG_ADJ);
        uint i_chan_num = pwm_gpio_to_channel(PSU_PWM_CURRENT_ADJ);
        pwm_set_chan_level(slice_num, v_chan_num, v_dac);
        pwm_set_chan_level(slice_num, i_chan_num, i_dac);
    #elif (BP_VER == 7 && BP_REV == 0)
        //I2C dac
    #else
        #error "Platform not speficied in psu.c"
    #endif
    // printf("GPIO: %d, slice: %d, v_chan: %d, i_chan: %d",PSU_PWM_VREG_ADJ,slice_num,v_chan_num,i_chan_num);
}

bool psu_fuse_ok(void) {
    uint32_t fuse = amux_read(HW_ADC_MUX_CURRENT_DETECT);
    // printf("Fuse: %d\r\n",fuse);
    return (fuse > 300);
}

bool psu_vout_ok(struct psu_status_t* psu) {
    return (amux_read(HW_ADC_MUX_VREF_VOUT) > 100); // todo calculate an actual voltage to 10%
}

bool psu_backflow_ok(struct psu_status_t* psu) {
    return (amux_read(HW_ADC_MUX_VREF_VOUT) < (amux_read(HW_ADC_MUX_VREG_OUT) + 100)); //+100? TODO: fine tuning
}

uint32_t psu_measure_current(void) {
    return (amux_read_current() * ((500 * 1000) / 4095));
}

uint32_t psu_measure_vreg(void) {
    return hw_adc_to_volts_x2(amux_read(HW_ADC_MUX_VREG_OUT));
}

uint32_t psu_measure_vout(void) {
    return hw_adc_to_volts_x2(amux_read(HW_ADC_MUX_VREF_VOUT));
}

void psu_measure(uint32_t* vout, uint32_t* isense, uint32_t* vreg, bool* fuse) {
    amux_sweep();
    *isense = ((hw_adc_raw[HW_ADC_CURRENT_SENSE]) * ((500 * 1000) / 4095));
    *vreg = ((hw_adc_voltage[HW_ADC_MUX_VREG_OUT]));
    *vout = ((hw_adc_voltage[HW_ADC_MUX_VREF_VOUT]));
    *fuse = (hw_adc_raw[HW_ADC_MUX_CURRENT_DETECT] > 300);
}

void psu_disable(void) {
    psu_vreg_enable(false);
    psu_dac_set(PWM_TOP, 0);
    psu_current_limit_override(false);
    busy_wait_ms(1);
    psu_fuse_reset(); // reset fuse so it isn't draining current from the opamp
}

uint32_t psu_enable(float volts, float current, bool current_limit_override) {

    psu_set_v(volts, &psu_status);
    if (!current_limit_override) {
        psu_set_i(current, &psu_status);
    } else {
        psu_status.current_dac_value = PWM_TOP; // override, set to 100% current
    }

    // printf("V dac: 0x%04X I dac: 0x%04X\r\n",psu_status.voltage_dac_value, psu_status.current_dac_value);
    // start with override engaged because the inrush will often trip the fuse on low limits
    psu_current_limit_override(true);
    psu_dac_set(psu_status.voltage_dac_value, PWM_TOP);
    psu_fuse_reset();
    psu_vreg_enable(true);
    busy_wait_ms(10);

    // after some settling time, engage the current limit system
    if (!current_limit_override) {
        psu_current_limit_override(false);
        psu_dac_set(psu_status.voltage_dac_value, psu_status.current_dac_value);
        busy_wait_ms(200);
        if (!psu_fuse_ok()) { // did the fuse blow?
            psu_disable();
            return PSU_ERROR_FUSE_TRIPPED;
        }
    }

    if (!psu_vout_ok(&psu_status)) { // did the vout voltage drop?
        psu_disable();
        return PSU_ERROR_VOUT_LOW;
    }

    if (!psu_backflow_ok(&psu_status)) { // is vreg_out < vref_vout?
        psu_disable();
        return PSU_ERROR_BACKFLOW;
    }

    return PSU_OK;
}

void psu_init(void) {
    psu_vreg_enable(false);

    #if (BP_VER == 5 || BP_VER == XL5 || BP_VER == 6 || (BP_VER == 7 && BP_REV > 0))
        // pin and PWM setup
        gpio_set_function(PSU_PWM_CURRENT_ADJ, GPIO_FUNC_SIO);
        gpio_set_dir(PSU_PWM_CURRENT_ADJ, GPIO_OUT);
        gpio_put(PSU_PWM_CURRENT_ADJ, 0);
        gpio_set_function(PSU_PWM_VREG_ADJ, GPIO_FUNC_SIO);
        gpio_set_dir(PSU_PWM_VREG_ADJ, GPIO_OUT);
        gpio_put(PSU_PWM_VREG_ADJ, 1);

        uint slice_num = pwm_gpio_to_slice_num(PSU_PWM_VREG_ADJ);
        uint v_chan_num = pwm_gpio_to_channel(PSU_PWM_VREG_ADJ);
        uint i_chan_num = pwm_gpio_to_channel(PSU_PWM_CURRENT_ADJ);

        // 10kHz clock, into our 1K + 0.1uF filter
        pwm_set_clkdiv_int_frac(slice_num, 16 >> 4, 16 & 0b1111);
        pwm_set_wrap(slice_num, PWM_TOP);

        // start with v adjust high (lowest voltage output)
        pwm_set_chan_level(slice_num, v_chan_num, PWM_TOP);

        // start with i adjust low (lowest current output)
        pwm_set_chan_level(slice_num, i_chan_num, 0);

        // enable output
        gpio_set_function(PSU_PWM_VREG_ADJ, GPIO_FUNC_PWM);
        gpio_set_function(PSU_PWM_CURRENT_ADJ, GPIO_FUNC_PWM);
        pwm_set_enabled(slice_num, true);

        // too early, spi not setup for shift register method of IO control
        // psu_current_limit_override(false);
        // psu_fuse_reset();
    #elif (BP_VER == 7 && BP_REV == 0)
        //I2C dac
    #else
        #error "Platform not speficied in psu.c"
    #endif
}
