//#include "pirate/button.h"
enum button_codes;
void button_scr_handler(struct command_result *res);
bool button_exec(enum button_codes button_code);

typedef struct {
    const char *verb;
    const char *default_file;
} button_press_type_t;

extern button_press_type_t button_press_types[];
extern const uint8_t num_button_press_types;
