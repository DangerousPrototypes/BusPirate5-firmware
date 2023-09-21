#include <stdint.h>

enum PIN_TYPE {
    BP_IO_PIN_TYPE,
    BP_GND_PIN_TYPE,
    BP_VOUT_IN_TYPE,
    BP_IO_PIN_BANK
};

typedef struct io_pin_bank{
    PIN_TYPE type;
    uint8_t num_pins;
    io_pin pins[BP_MAX_PINS];
} ;

typedef struct io_pin{
    PIN_TYPE type;
    bool has_adc;
    void *adc_update;
    uint16_t *adc_raw;
    uint32_t *adc_voltage;
    bool has_pwm;
    void *pwm_update;
    uint32_t pwm_period;
    uint32_t pwm_duty;
    bool has_freq;
    void *freq_update;
    uint32_t freq_value;
    const char *name;
    const char *label;
    void *set_input;
    void *set_output;
    
    bool has_vreg_adjust;
    void *vreg_set_voltage;
    uint32_t vreg_adj_value;
    bool has_current_adjust;
    void *vreg_set_current;
    uint32_t vreg_current_value;
    bool has_current_sense;
    void *vreg_get_current;
    bool has_over_current_detect;
    void *vreg_get_over_current;
    
};

typedef struct gnd_pin{
    PIN_TYPE type;
    const char *name;
    const char *label;
};

typedef struct vout_pin{
    bool has_adc;
    uint16_t *adc_raw;
    uint32_t *adc_voltage;
    bool has_vreg_adjust;
    void *vreg_set_voltage;
    uint32_t vreg_adj_value;
    bool has_current_adjust;
    void *vreg_set_current;
    uint32_t vreg_current_value;
    bool has_current_sense;
    void *vreg_get_current;
    bool has_over_current_detect;
    void *vreg_get_over_current;
};

struct pins{
    vout_pin *vout0;
    io_pins[BP_MAX_PINS];
    gnd_pin *gnd0;
};