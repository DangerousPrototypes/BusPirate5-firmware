/**
 * @file SW2W.h
 * @brief Software 2-wire mode interface.
 * @details Provides bit-banged 2-wire protocol mode.
 */

void SW2W_start(void);
void SW2W_startr(void);
void SW2W_stop(void);
void SW2W_stopr(void);
uint32_t SW2W_send(uint32_t d);
uint32_t SW2W_read(void);
void SW2W_clkh(void);
void SW2W_clkl(void);
void SW2W_dath(void);
void SW2W_datl(void);
uint32_t SW2W_dats(void);
void SW2W_clk(void);
uint32_t SW2W_bitr(void);
uint32_t SW2W_period(void);
void SW2W_macro(uint32_t macro);
void SW2W_setup(void);
void SW2W_setup_exc(void);
void SW2W_cleanup(void);
void SW2W_pins(void);
void SW2W_settings(void);
void SW2W_setDATAmode(uint8_t input);

#define SW2W_INPUT 1
#define SW2W_OUTPUT 0

#define SW2W_CLOCK_HIGH() gpio_set(BP_SW2W_CLK_PORT, BP_SW2W_CLK_PIN)
#define SW2W_CLOCK_LOW() gpio_clear(BP_SW2W_CLK_PORT, BP_SW2W_CLK_PIN)

#define SW2W_DATA_HIGH() gpio_set(BP_SW2W_SDA_PORT, BP_SW2W_SDA_PIN)
#define SW2W_DATA_LOW() gpio_clear(BP_SW2W_SDA_PORT, BP_SW2W_SDA_PIN)

#define SW2W_DATA_OPENDRAIN()                                                                                          \
    gpio_set_mode(BP_SW2W_SDA_PORT, GPIO_MODE_OUTPUT_10_MHZ, GPIO_CNF_OUTPUT_OPENDRAIN, BP_SW2W_SDA_PIN)
#define SW2W_DATA_PUSHPULL()                                                                                           \
    gpio_set_mode(BP_SW2W_SDA_PORT, GPIO_MODE_OUTPUT_10_MHZ, GPIO_CNF_OUTPUT_PUSHPULL, BP_SW2W_SDA_PIN)
#define SW2W_DATA_INPUT() gpio_set_mode(BP_SW2W_SDA_PORT, GPIO_MODE_INPUT, GPIO_CNF_INPUT_FLOAT, BP_SW2W_SDA_PIN)
#define SW2W_DATA_READ() gpio_get(BP_SW2W_SDA_PORT, BP_SW2W_SDA_PIN)

#define SW2W_SETUP_OPENDRAIN()                                                                                         \
    gpio_set_mode(BP_SW2W_SDA_PORT, GPIO_MODE_OUTPUT_10_MHZ, GPIO_CNF_OUTPUT_OPENDRAIN, BP_SW2W_SDA_PIN);              \
    gpio_set_mode(BP_SW2W_CLK_PORT, GPIO_MODE_OUTPUT_10_MHZ, GPIO_CNF_OUTPUT_OPENDRAIN, BP_SW2W_CLK_PIN)
#define SW2W_SETUP_PUSHPULL()                                                                                          \
    gpio_set_mode(BP_SW2W_SDA_PORT, GPIO_MODE_OUTPUT_10_MHZ, GPIO_CNF_OUTPUT_PUSHPULL, BP_SW2W_SDA_PIN);               \
    gpio_set_mode(BP_SW2W_CLK_PORT, GPIO_MODE_OUTPUT_10_MHZ, GPIO_CNF_OUTPUT_PUSHPULL, BP_SW2W_CLK_PIN)
#define SW2W_SETUP_HIZ()                                                                                               \
    gpio_set_mode(BP_SW2W_SDA_PORT, GPIO_MODE_INPUT, GPIO_CNF_INPUT_FLOAT, BP_SW2W_SDA_PIN);                           \
    gpio_set_mode(BP_SW2W_CLK_PORT, GPIO_MODE_INPUT, GPIO_CNF_INPUT_FLOAT, BP_SW2W_CLK_PIN)
