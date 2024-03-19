typedef struct ui_help_options
{
	uint help; //should be a section handling designator
	const char command[9]; //ugh
	uint description; //translation key
} ui_help_options_t;

void ui_help_options(const struct ui_help_options (*help), uint32_t count);
void ui_help_usage(const char * const flash_usage[], uint32_t count);
bool ui_help_show(bool help_flag, const char * const usage[], uint32_t count_of_usage, const struct ui_help_options *options, uint32_t count_of_options);
