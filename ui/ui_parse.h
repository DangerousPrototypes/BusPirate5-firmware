

//gets all number accepted formats
bool ui_parse_get_int(struct prompt_result *result, uint32_t *value);

// get DEC (base 10) with eXit option
bool ui_parse_get_uint32(struct prompt_result *result, uint32_t* value);

bool ui_parse_get_string(struct prompt_result *result, char *str, uint8_t *size);

// get float with eXit option
bool ui_parse_get_float(struct prompt_result *result, float* value);
bool ui_parse_get_delimited_sequence(struct prompt_result *result, char delimiter, uint32_t* value);
bool ui_parse_get_units(struct prompt_result *result, char* units, uint8_t length, uint8_t* unit_type);
bool ui_parse_get_macro(struct prompt_result *result, uint32_t* value);
bool ui_parse_get_attributes(struct prompt_result *result, uint32_t* attr, uint8_t len);
bool ui_parse_get_bool(struct prompt_result *result, bool* value);

bool ui_parse_get_colon(uint32_t *value);
bool ui_parse_get_dot(uint32_t *value);

void ui_parse_consume_whitespace(void);





