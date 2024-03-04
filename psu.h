void psu_init(void);
void psu_enable(struct command_result *res);
void psu_disable(struct command_result *res);
bool psu_setup(void);
bool psu_reset(void);
void psu_cleanup(void);
void psu_irq_callback(void);

uint32_t psu_set(float volts, float current, bool fuse_en);
