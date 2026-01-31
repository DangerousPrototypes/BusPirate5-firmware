/**
 * @file ui_prompt.h
 * @brief User interface prompt and menu system.
 * @details Provides interactive menu system with validation, default values,
 *          and various input types (integers, lists, pins).
 */

/**
 * @brief Single menu item descriptor.
 */
typedef struct prompt_item {
    uint description;  ///< Translation key for item description
} prompt_item;

/**
 * @brief Menu prompt configuration structure.
 */
typedef struct ui_prompt {
    uint description;                      ///< Translation key for menu description
    const struct prompt_item* menu_items;  ///< Array of menu items
    uint menu_items_count;                 ///< Number of menu items
    uint prompt_text;                      ///< Translation key for prompt text
    uint32_t minval;                       ///< Minimum valid value
    uint32_t maxval;                       ///< Maximum valid value
    uint32_t defval;                       ///< Default value
    uint32_t (*menu_action)(uint32_t a, uint32_t b); ///< Optional action callback
    const struct ui_prompt_config* config; ///< Menu behavior configuration
} ui_prompt;

/**
 * @brief Menu behavior configuration.
 */
typedef struct ui_prompt_config {
    bool allow_prompt_text;    ///< Show prompt text
    bool allow_prompt_defval;  ///< Show default value in prompt
    bool allow_defval;         ///< Accept default value
    bool allow_exit;           ///< Allow exit without selection
    bool (*menu_print)(const struct ui_prompt* menu);        ///< Custom menu printer
    bool (*menu_prompt)(const struct ui_prompt* menu);       ///< Custom prompt printer
    bool (*menu_validate)(const struct ui_prompt* menu, uint32_t* value); ///< Custom validator
} ui_prompt_config;

/**
 * @brief Result of prompt interaction.
 */
typedef struct prompt_result {
    uint8_t number_format; ///< Number format used (hex/dec/bin)
    bool success;          ///< User provided valid value
    bool exit;             ///< User requested exit
    bool no_value;         ///< User provided empty input
    bool default_value;    ///< User accepted default
    bool error;            ///< Error occurred during prompt
} prompt_result;

/**
 * @brief Display menu, prompt user, and validate input.
 * @param[out] result  Result structure
 * @param menu         Menu configuration
 * @param[out] value   Validated user input
 * @return true on success, false on error
 */
bool ui_prompt_uint32(struct prompt_result* result, const struct ui_prompt* menu, uint32_t* value);

/**
 * @name Ordered list menu helpers
 * @{
 */
bool ui_prompt_menu_ordered_list(const struct ui_prompt* menu);
bool ui_prompt_prompt_ordered_list(const struct ui_prompt* menu);
bool ui_prompt_validate_ordered_list(const struct ui_prompt* menu, uint32_t* value);
/** @} */

/**
 * @name Integer input menu helpers
 * @{
 */
bool ui_prompt_menu_int(const struct ui_prompt* menu);
bool ui_prompt_prompt_int(const struct ui_prompt* menu);
bool ui_prompt_validate_int(const struct ui_prompt* menu, uint32_t* value);
/** @} */

/**
 * @name I/O pin selection menu helpers
 * @{
 */
bool ui_prompt_menu_bio_pin(const struct ui_prompt* menu);
bool ui_prompt_prompt_bio_pin(const struct ui_prompt* menu);

bool ui_prompt_float(struct prompt_result* result,
                     float minval,
                     float maxval,
                     float defval,
                     bool allow_exit,
                     float* user_value,
                     bool none);
bool ui_prompt_float_units(struct prompt_result* result, const char* menu, float* user_value, uint8_t* user_units);
bool ui_prompt_any_key_continue(struct prompt_result* result,
                                uint32_t delay,
                                uint32_t (*refresh_func)(uint8_t pin, bool refresh),
                                uint8_t pin,
                                bool refresh);
bool ui_prompt_vt100_mode(prompt_result* result, uint32_t* value);
void ui_prompt_invalid_option(void);
uint32_t ui_prompt_yes_no(void);
bool ui_prompt_bool(prompt_result* result, bool defval_show, bool defval, bool allow_exit, bool* user_value);

void ui_prompt_mode_settings_int(const char* label, uint32_t value, const char* units);
void ui_prompt_mode_settings_string(const char* label, const char* string, const char* units);

// default prompt configurations for general purpose mode setup
extern const struct ui_prompt_config prompt_int_cfg;
extern const struct ui_prompt_config prompt_list_cfg;

/** @} */