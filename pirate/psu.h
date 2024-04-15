enum psu_errors {
    PSU_OK=0,
    PSU_ERROR_FUSE_TRIPPED,
    PSU_ERROR_VOUT_LOW,
    PSU_ERROR_BACKFLOW
};

struct psu_status_t {
    float voltage_requested;
    float voltage_actual;
    uint32_t voltage_actual_i;
    uint16_t voltage_dac_value;
    float current_requested;
    float current_actual;
    uint32_t current_actual_i;
    uint16_t current_dac_value;
    bool current_limit_override;
};

extern struct psu_status_t psu_status;

void psu_init(void);
uint32_t psu_enable(float volts, float current, bool current_limit_override);
void psu_disable(void);
void psu_measure(uint32_t *vout, uint32_t *isense, uint32_t *vreg, bool *fuse);
uint32_t psu_measure_vout(void);
uint32_t psu_measure_vreg(void);
uint32_t psu_measure_current(void);
bool psu_fuse_ok(void);
void psu_vreg_enable(bool enable);
void psu_current_limit_override(bool enable);
