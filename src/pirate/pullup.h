enum {
    PULLX_OFF=0,
    PULLX_1K3,
    PULLX_1K5,
    PULLX_1K8,
    PULLX_2K2,
    PULLX_3K2,
    PULLX_4K7,
    PULLX_10K,
    PULLX_1M,
};

struct pullx_options_t {
    uint8_t pull;
    const char name[5];
    uint8_t resistors; // Use a uint8_t to store the boolean values as bitfields
};

extern const struct pullx_options_t pullx_options[9];
extern uint8_t pullx_value[8];
extern uint8_t pullx_direction;

void pullup_init(void);
void pullup_enable(void);
void pullup_disable(void);

void pullx_set_all_test(uint16_t resistor_mask, uint16_t direction_mask);
bool pullx_set_all_update(uint8_t pull, bool pull_up);
void pullx_set_pin(uint8_t pin, uint8_t pull, bool pull_up);
bool pullx_update(void);
void pullx_get_pin(uint8_t pin, uint8_t *pull, bool *pull_up);