typedef struct ui_help_options {
    uint help;             // should be a section handling designator
    const char command[9]; // ugh
    uint description;      // translation key
} ui_help_options_t;

void ui_help_options(const struct ui_help_options(*help), uint32_t count);
void ui_help_usage(const char* const flash_usage[], uint32_t count);
bool ui_help_show_v2(bool help_flag,
                  const char* const usage_desc[],
                  const char* const usage_cmd[],
                  uint32_t count_of_usage,
                  const struct ui_help_options* options,
                  uint32_t count_of_options);
bool ui_help_show(bool help_flag,
                  const char* const usage[],
                  uint32_t count_of_usage,
                  const struct ui_help_options* options,
                  uint32_t count_of_options);
bool ui_help_check_vout_vref(void);
bool ui_help_sanity_check(bool vout, uint8_t pullup_mask);
void ui_help_mode_commands(const struct _mode_command_struct* commands, uint32_t count);
void ui_help_error(uint32_t error);
void ui_help_mode_commands_exec(const struct _mode_command_struct* commands, uint32_t count, const char* mode);