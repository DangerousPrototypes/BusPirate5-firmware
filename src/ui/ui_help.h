/**
 * @file ui_help.h
 * @brief Help system and sanity checking interface.
 * @details Provides command help display, usage information,
 *          and hardware configuration validation.
 */

/**
 * @brief Help option descriptor.
 */
typedef struct ui_help_options {
    uint help;             ///< Section handling designator
    const char command[9]; ///< Command string
    uint description;      ///< Translation key for description
} ui_help_options_t;

/**
 * @brief Display command options with descriptions.
 * @param help   Array of help option descriptors
 * @param count  Number of options
 */
void ui_help_options(const struct ui_help_options(*help), uint32_t count);

/**
 * @brief Display usage information.
 * @param flash_usage  Array of usage strings from flash
 * @param count        Number of usage lines
 */
void ui_help_usage(const char* const flash_usage[], uint32_t count);

/**
 * @brief Show help if help flag is set.
 * @param help_flag         Help requested flag
 * @param usage             Usage strings array
 * @param count_of_usage    Number of usage lines
 * @param options           Option descriptors array
 * @param count_of_options  Number of options
 * @return true if help was shown
 */
bool ui_help_show(bool help_flag,
                  const char* const usage[],
                  uint32_t count_of_usage,
                  const struct ui_help_options* options,
                  uint32_t count_of_options);

/**
 * @brief Check if VOUT or VREF power is enabled.
 * @return true if either is enabled
 */
bool ui_help_check_vout_vref(void);

/**
 * @brief Sanity check hardware configuration.
 * @param vout         VOUT required flag
 * @param pullup_mask  Pullup configuration mask
 * @return true if configuration valid
 */
bool ui_help_sanity_check(bool vout, uint8_t pullup_mask);

/**
 * @brief Display mode-specific commands.
 * @param commands  Array of mode command descriptors
 * @param count     Number of commands
 */
void ui_help_mode_commands(const struct _mode_command_struct* commands, uint32_t count);

/**
 * @brief Display error message.
 * @param error  Error code
 */
void ui_help_error(uint32_t error);

/**
 * @brief Execute mode command help.
 * @param commands  Array of mode command descriptors
 * @param count     Number of commands
 * @param mode      Mode name string
 */
void ui_help_mode_commands_exec(const struct _mode_command_struct* commands, uint32_t count, const char* mode);

/**
 * @brief Auto-generate global command help grouped by category.
 * @details Walks commands[] array, prints each category heading followed
 *          by commands in that category with their descriptions.
 *          Hidden commands (CMD_CAT_HIDDEN) are skipped.
 */
void ui_help_global_commands(void);

/**
 * @brief Reset shared help pager row counter.
 * @details Call before a sequence of help display functions
 *          to ensure continuous paging across sections.
 */
void ui_help_pager_reset(void);

/**
 * @brief Disable paging for help output.
 * @details Sets pager to never pause, useful for scripts or redirected output.
 */
void ui_help_pager_disable(void);