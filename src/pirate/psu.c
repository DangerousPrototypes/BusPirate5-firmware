#include <stdio.h>
#include "pico/stdlib.h"
#include "hardware/pwm.h"
#include "hardware/clocks.h"
#include "pirate.h"
#include "pirate/shift.h"
#include "pirate/psu.h"
#include "pirate/amux.h"
#if BP_HW_PSU_DAC
    #include "hardware/i2c.h"
#endif

#if BP_HW_PSU_DAC
    #define PWM_TOP 0x0fff //12 bit DAC
#else
    #define PWM_TOP 14000 // 0x30D3
#endif

// voltage settings
#define PSU_V_LOW 800   // millivolts
#define PSU_V_HIGH 5000 // millivolts
#define PSU_V_RANGE ((PSU_V_HIGH * 10) - (PSU_V_LOW * 10))

// current settings
#define PSU_I_LOW 0    // mA
#define PSU_I_HIGH 500 // mA
#define PSU_I_RANGE ((PSU_I_HIGH * 10000) - (PSU_I_LOW * 10000))

struct psu_status_t psu_status;

static void psu_fuse_reset(void) {
    // reset current trigger
    #if BP_HW_IOEXP_595
        shift_clear_set_wait(CURRENT_RESET, 0); // low to activate the pnp
        busy_wait_ms(1);
        shift_clear_set_wait(0, CURRENT_RESET); // high to disable    
    #elif BP_HW_IOEXP_NONE
        gpio_put(CURRENT_RESET, 0);
        busy_wait_ms(1);
        gpio_put(CURRENT_RESET, 1);
    #else
        #error "Platform not speficied in psu.c"
    #endif
}

// TODO: rename this function, it actually controls if the current limit circuit is connected to the VREG
void psu_vreg_enable(bool enable) {
    #if BP_HW_IOEXP_595
        if (enable) {
            shift_clear_set_wait(CURRENT_EN, 0); // low is on (PNP)
        } else {
            shift_clear_set_wait(0, CURRENT_EN); // high is off
        }    
    #elif BP_HW_IOEXP_NONE
        gpio_put(CURRENT_EN, !enable);
    #else
        #error "Platform not speficied in psu.c"
    #endif
}

void psu_current_limit_override(bool enable) {
    #if BP_HW_IOEXP_595
        if (enable) {
            shift_clear_set_wait(0, CURRENT_EN_OVERRIDE);
        } else {
            shift_clear_set_wait(CURRENT_EN_OVERRIDE, 0);
        }
    #elif BP_HW_IOEXP_NONE
        gpio_put(CURRENT_EN_OVERRIDE, enable);
    #else
        #error "Platform not speficied in psu.c"
    #endif
    psu_status.current_limit_override = enable;
}

void psu_set_v(float volts, struct psu_status_t* psu) {
    uint32_t psu_v_per_bit = ((PSU_V_RANGE) / PWM_TOP);
    uint32_t vset = (uint32_t)((float)volts * 10000);
    vset = vset - (PSU_V_LOW * 10);
    vset = vset / psu_v_per_bit;
    if (vset > PWM_TOP) {
        vset = PWM_TOP + 1;
    }

    psu->voltage_requested_float = volts;
    psu->voltage_actual_int = ((vset * psu_v_per_bit) + (PSU_V_LOW * 10));
    psu->voltage_actual_float = (float)((float)((vset * psu_v_per_bit) + (PSU_V_LOW * 10)) / (float)10000);
    // inverse for VREG margining
    psu->voltage_dac_value = (PWM_TOP - vset);
}

void psu_set_i(float current, struct psu_status_t* psu) {
    float psu_i_per_bit = ((float)(PSU_I_RANGE) / (float)PWM_TOP);
    float iset = (float)((float)current * 10000);
    iset /= psu_i_per_bit;
    float iact = (float)((float)iset * psu_i_per_bit) / 10000;

    psu->current_dac_value = (uint32_t)iset;
    psu->current_requested_float = current;
    psu->current_actual_int = (uint32_t)((float)iset * (float)psu_i_per_bit);
    psu->current_actual_float = iact;
}

void psu_dac_set(uint16_t v_dac, uint16_t i_dac) {
    #if BP_HW_PSU_PWM
        uint slice_num = pwm_gpio_to_slice_num(PSU_PWM_VREG_ADJ);
        uint v_chan_num = pwm_gpio_to_channel(PSU_PWM_VREG_ADJ);
        uint i_chan_num = pwm_gpio_to_channel(PSU_PWM_CURRENT_ADJ);
        pwm_set_chan_level(slice_num, v_chan_num, v_dac);
        pwm_set_chan_level(slice_num, i_chan_num, i_dac);
    #elif BP_HW_PSU_DAC
        //I2C dac
        //voltage dac
        const uint8_t v_dac_address = 0xc2;
        uint8_t dac[2];
        dac[0] = (v_dac >> 8) & 0xF;
        dac[1] = v_dac & 0xFF;
        if(i2c_write_blocking(BP_I2C_PORT, v_dac_address, dac, 2, false) == PICO_ERROR_GENERIC){
            printf("I2C write error\n");
        } 
        //current dac
        const uint8_t i_dac_address = 0xc0;
        dac[0] == (i_dac >> 8) & 0xF;
        dac[1] == i_dac & 0xFF;
        if(i2c_write_blocking(BP_I2C_PORT, i_dac_address, dac, 2, false) == PICO_ERROR_GENERIC){
            printf("I2C write error\n");
        }
    #else
        #error "Platform not speficied in psu.c"
    #endif
    // printf("GPIO: %d, slice: %d, v_chan: %d, i_chan: %d",PSU_PWM_VREG_ADJ,slice_num,v_chan_num,i_chan_num);
}

// deal with floating limit better (eg 0.7)
void psu_set_undervoltage_limit(struct psu_status_t* psu, uint8_t undervoltage_percent) {
    if (undervoltage_percent == 100) {
        psu->undervoltage_limit_override = true;
    }else{
        psu->undervoltage_limit_override = false;
    }
    psu->undervoltage_percent = undervoltage_percent;
    // calculate limit in mV (for display)
    psu->undervoltage_limit_int = (psu_status.voltage_actual_int - ((float)psu_status.voltage_actual_int * ((float)undervoltage_percent/100.f)));
    // calculate raw ADC counts for fast comparison: adc = (mV * 4096) / 66000
    psu->undervoltage_limit_adc = (uint16_t)((psu->undervoltage_limit_int * 4096UL) / 66000UL);
}

bool psu_fuse_ok(void) {
    if(psu_status.error_overcurrent) {
        return false;
    }
    uint32_t fuse = amux_read(HW_ADC_MUX_CURRENT_DETECT);
    //printf("Fuse: %d\r\n",fuse);
    return (fuse > 300);
}

bool psu_vout_ok(void) {
    if(psu_status.undervoltage_limit_override) {
        return true;
    }
    if(psu_status.error_undervoltage) {
        return false;
    }
    // compare raw ADC counts directly - no runtime math needed
    return (amux_read(HW_ADC_MUX_VREF_VOUT) > psu_status.undervoltage_limit_adc);
}

bool psu_poll_fuse_vout_error(void) {
    if(!psu_status.enabled) {
        return false;
    }
    bool error = false;
    if (!psu_fuse_ok()) {
        psu_status.error_overcurrent = true;
        psu_status.error_pending = true;
        error = true;
    }
    if (!psu_vout_ok()) {
        psu_status.error_undervoltage = true;
        psu_status.error_pending = true;
        error = true;
    }
    if(error) {
        psu_disable();
    }   
    return error;
}

void psu_clear_error_flag(void) {
    psu_status.error_pending = false;
}

bool psu_backflow_ok(void) {
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
    psu_status.enabled = false;
    psu_vreg_enable(false);
    psu_dac_set(PWM_TOP, 0);
    psu_current_limit_override(false);
    busy_wait_ms(1);
    psu_fuse_reset(); // reset fuse so it isn't draining current from the opamp    
}

uint32_t psu_enable(float volts, float current, bool current_limit_override, uint8_t undervoltage_percent) {

    // setup the config struct
    psu_disable();
    psu_status.error_overcurrent = false;
    psu_status.error_undervoltage = false;
    psu_status.error_pending = false;

    psu_set_v(volts, &psu_status);
    if (!current_limit_override) {
        psu_set_i(current, &psu_status);
    } else {
        psu_status.current_dac_value = PWM_TOP; // override, set to 100% current
    }

    psu_set_undervoltage_limit(&psu_status, undervoltage_percent);

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
        busy_wait_ms(500);
        if (!psu_fuse_ok()) { // did the fuse blow?
            psu_disable();
            return PSU_ERROR_FUSE_TRIPPED;
        }
    }

    if (!psu_vout_ok()) { // did the vout voltage drop?
        psu_disable();
        return PSU_ERROR_VOUT_LOW;
    }

    if (!psu_backflow_ok()) { // is vreg_out < vref_vout?
        psu_disable();
        return PSU_ERROR_BACKFLOW;
    }

    psu_status.enabled = true;

    return PSU_OK;
}

void psu_init(void) {
    psu_vreg_enable(false);
    psu_status.enabled = false;
    psu_status.error_overcurrent = false;
    psu_status.error_undervoltage = false;
    psu_status.error_pending = false;

    #if BP_HW_PSU_PWM
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
    #elif BP_HW_PSU_DAC
        //I2C dac
        i2c_init(BP_I2C_PORT, 400 * 1000);
        gpio_set_function(BP_I2C_SDA, GPIO_FUNC_I2C);
        gpio_set_function(BP_I2C_SCL, GPIO_FUNC_I2C);    
        //init dac
        psu_dac_set(0xffff, 0x0000);  
    #else
        #error "Platform not speficied in psu.c"
    #endif
}
