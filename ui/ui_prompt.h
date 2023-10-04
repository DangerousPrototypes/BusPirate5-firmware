typedef struct prompt_item
{
	uint description;
} prompt_item;

typedef struct ui_prompt
{
	uint description;
	const struct prompt_item* menu_items;
	uint menu_items_count;
	uint prompt_text;
	uint32_t minval; 
	uint32_t maxval; 
	uint32_t defval;
	uint32_t (*menu_action)(uint32_t a, uint32_t b);
    const struct ui_prompt_config* config;
} ui_prompt;

typedef struct ui_prompt_config 
{
	bool allow_prompt_text;
	bool allow_prompt_defval;
	bool allow_defval; 
	bool allow_exit;
	bool (*menu_print)(const struct ui_prompt* menu);
	bool (*menu_prompt)(const struct ui_prompt* menu);
	bool (*menu_validate)(const struct ui_prompt* menu, uint32_t* value);
} ui_prompt_config;

typedef struct prompt_result {
	uint8_t number_format;
    bool success;
	bool exit;
	bool no_value;
	bool default_value;
	bool error;
} prompt_result;



// takes a menu config struct and shows a time/menu/prompt accepts and validates user input
bool ui_prompt_uint32(struct prompt_result *result, const struct ui_prompt* menu, uint32_t* value);

// helper functions for ui_prompt_uint32
// ORDERED LISTS
bool ui_prompt_menu_ordered_list(const struct ui_prompt* menu);
bool ui_prompt_prompt_ordered_list(const struct ui_prompt* menu);
bool ui_prompt_validate_ordered_list(const struct ui_prompt* menu, uint32_t* value);
// INTEGER
bool ui_prompt_menu_int(const struct ui_prompt * menu);
bool ui_prompt_prompt_int(const struct ui_prompt * menu);
bool ui_prompt_validate_int(const struct ui_prompt* menu, uint32_t* value);
// BIO PINS
bool ui_prompt_menu_bio_pin(const struct ui_prompt* menu);
bool ui_prompt_prompt_bio_pin(const struct ui_prompt* menu);

bool ui_prompt_float(struct prompt_result *result, float minval, float maxval, float defval, bool allow_exit, float* user_value);
bool ui_prompt_float_units(struct prompt_result *result, const char *menu, float* user_value, uint8_t* user_units);
bool ui_prompt_any_key_continue(struct prompt_result *result, uint32_t delay, uint32_t (*refresh_func)(uint8_t pin, bool refresh), uint8_t pin, bool refresh);
bool ui_prompt_vt100_mode(prompt_result *result, uint32_t *value);
void ui_prompt_invalid_option(void);
uint32_t ui_prompt_yes_no(void);

// default prompt configurations for general purpose mode setup
extern const struct ui_prompt_config prompt_int_cfg;
extern const struct ui_prompt_config prompt_list_cfg;