void psu_init(void);
void psu_enable(struct command_attributes *attributes, struct command_response *response);
void psu_disable(struct command_attributes *attributes, struct command_response *response);
bool psu_setup(void);
bool psu_reset(void);
void psu_cleanup(void);

uint32_t psu_set(float volts, float current);
