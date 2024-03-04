

typedef struct ui_info_help
{
	uint help; //should be a section handling designator
	const char command[9]; //ugh
	uint description; //translation key
} ui_info_help;

void ui_help_print_args(struct command_result *res);
void ui_help_options(const struct ui_info_help (*help), uint32_t count);
void ui_help_usage(const char * const flash_usage[], uint32_t count);
