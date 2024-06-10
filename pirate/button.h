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
bool button_check_irq(uint8_t button_id, bool *long_press);
