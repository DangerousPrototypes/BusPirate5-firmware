// initialize all buttons
void button_init(void);
// example irq callback handler, copy for your own uses
void button_irq_enable(uint8_t button_id, void *callback);
// enable the irq for button button_id
void button_irq_disable(uint8_t button_id);
// example irq callback handler, copy for your own uses
void button_irq_callback(uint gpio, uint32_t events);
// poll the value of button button_id
bool button_get(uint8_t button_id);
//check for interrupt and clear interrupt flag
// bool button_check_irq(uint8_t button_id, bool *long_press);

enum button_codes {
    BP_BUTT_NO_PRESS = 0,
    BP_BUTT_SHORT_PRESS,
    BP_BUTT_LONG_PRESS,
    BP_BUTT_DOUBLE_TAP,
};

enum button_codes button_check_press(uint8_t button_id);

