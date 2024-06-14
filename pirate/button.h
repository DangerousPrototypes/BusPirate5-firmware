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

// enum for button press types
enum button_codes {
    BP_BUTT_NO_PRESS = 0,
    BP_BUTT_SHORT_PRESS,
    BP_BUTT_LONG_PRESS,
    BP_BUTT_DOUBLE_TAP,
};

#define BUTTON_FLAG_HIDE_COMMENTS 1u<<0
#define BUTTON_FLAG_EXIT_ON_ERROR 1u<<1
#define BUTTON_FLAG_FILE_CONFIGURED 1u<<2
#define BUTTON_LONG_FLAG_FILE_CONFIGURED 1u<<3

// check the type of button press
enum button_codes button_check_press(uint8_t button_id);
bool button_exec(enum button_codes button_code);

extern uint8_t button_flags;
extern char button_script_file[BP_FILENAME_MAX + 1];
extern char button_long_script_file[BP_FILENAME_MAX + 1];
