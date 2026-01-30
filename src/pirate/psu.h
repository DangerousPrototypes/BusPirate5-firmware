enum psu_errors {
    PSU_OK = 0,
    PSU_ERROR_FUSE_TRIPPED,
    PSU_ERROR_VOUT_LOW,
    PSU_ERROR_BACKFLOW
};

    //uint8_t psu; // psu (0=off, 1=on)
    //uint32_t psu_voltage;   // psu voltage output setting in decimal * 10000
    //bool psu_current_limit_en;
    //uint32_t psu_current_limit; // psu current limit in decimal * 10000
    //bool psu_current_error;     // psu over current limit fuse tripped
    //bool psu_undervoltage_error;
    //bool psu_error;             // error, usually with the dac
    //bool psu_irq_en;

struct psu_status_t {
    float voltage_requested_float;
    float voltage_actual_float;
    uint32_t voltage_actual_int;
    uint16_t voltage_dac_value;
    float current_requested_float;
    float current_actual_float;
    uint32_t current_actual_int;
    uint16_t current_dac_value;
    uint8_t undervoltage_percent;
    uint32_t undervoltage_limit_int;  // mV value for display
    uint16_t undervoltage_limit_adc;  // raw ADC counts for comparison
    bool current_limit_override;
    bool undervoltage_limit_override;
    bool enabled;
    bool error_overcurrent;
    bool error_undervoltage;
    bool error_pending;
};

extern struct psu_status_t psu_status;

void psu_init(void);
uint32_t psu_enable(float volts, float current, bool current_limit_enabled, uint8_t voltage_lag_percent);
void psu_disable(void);
void psu_measure(uint32_t* vout, uint32_t* isense, uint32_t* vreg, bool* fuse);
uint32_t psu_measure_vout(void);
uint32_t psu_measure_vreg(void);
uint32_t psu_measure_current(void);
bool psu_fuse_ok(void);
bool psu_vout_ok(void);
void psu_vreg_enable(bool enable);
void psu_current_limit_override(bool enable);
bool psu_poll_fuse_vout_error(void);
void psu_clear_error_flag(void) ;
