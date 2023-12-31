void psu_init(void);
void psu_enable(opt_args (*args), struct command_result *res);
void psu_disable(opt_args (*args), struct command_result *res);
bool psu_setup(void);
bool psu_reset(void);
void psu_cleanup(void);
void psu_irq_callback(void);

uint32_t psu_set(float volts, float current, bool fuse_en);
