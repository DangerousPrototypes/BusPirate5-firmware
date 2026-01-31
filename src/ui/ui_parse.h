/**
 * @file ui_parse.h
 * @brief User input parsing utilities interface.
 * @details Provides functions to parse various input types:
 *          integers, floats, strings, booleans, delimited sequences,
 *          and special syntax (colon, dot operators).
 */

/**
 * @brief Parse integer in any accepted format (hex/dec/bin).
 * @param[out] result  Parse result structure
 * @param[out] value   Parsed integer value
 * @return true on success
 */
bool ui_parse_get_int(struct prompt_result* result, uint32_t* value);

/**
 * @brief Parse unsigned 32-bit decimal integer with exit option.
 * @param[out] result  Parse result structure
 * @param[out] value   Parsed value
 * @return true on success
 */
bool ui_parse_get_uint32(struct prompt_result* result, uint32_t* value);

/**
 * @brief Parse string input.
 * @param[out] result  Parse result structure
 * @param[out] str     Output string buffer
 * @param[out] size    String length
 * @return true on success
 */
bool ui_parse_get_string(struct prompt_result* result, char* str, uint8_t* size);

/**
 * @brief Parse floating point value with exit option.
 * @param[out] result  Parse result structure
 * @param[out] value   Parsed float value
 * @return true on success
 */
bool ui_parse_get_float(struct prompt_result* result, float* value);

/**
 * @brief Parse delimited sequence (e.g., "1:2:3" or "A.B.C").
 * @param[out] result   Parse result structure
 * @param delimiter     Sequence delimiter character
 * @param[out] value    Parsed value
 * @return true on success
 */
bool ui_parse_get_delimited_sequence(struct prompt_result* result, char delimiter, uint32_t* value);

/**
 * @brief Parse unit suffix (Hz, kHz, MHz, ns, us, ms, %).
 * @param[out] result     Parse result structure
 * @param[out] units      Unit string buffer
 * @param length          Buffer length
 * @param[out] unit_type  Unit type index
 * @return true on success
 */
bool ui_parse_get_units(struct prompt_result* result, char* units, uint8_t length, uint8_t* unit_type);

/**
 * @brief Parse macro reference.
 * @param[out] result  Parse result structure
 * @param[out] value   Macro value
 * @return true on success
 */
bool ui_parse_get_macro(struct prompt_result* result, uint32_t* value);

/**
 * @brief Parse attribute flags.
 * @param[out] result  Parse result structure
 * @param[out] attr    Attribute values array
 * @param len          Number of attributes
 * @return true on success
 */
bool ui_parse_get_attributes(struct prompt_result* result, uint32_t* attr, uint8_t len);

/**
 * @brief Parse boolean value.
 * @param[out] result  Parse result structure
 * @param[out] value   Boolean value
 * @return true on success
 */
bool ui_parse_get_bool(struct prompt_result* result, bool* value);

/**
 * @brief Parse colon operator (e.g., "10:20").
 * @param[out] value  Parsed value
 * @return true on success
 */
bool ui_parse_get_colon(uint32_t* value);

/**
 * @brief Parse dot operator (e.g., "A.B").
 * @param[out] value  Parsed value
 * @return true on success
 */
bool ui_parse_get_dot(uint32_t* value);

/**
 * @brief Skip whitespace in input buffer.
 */
void ui_parse_consume_whitespace(void);
