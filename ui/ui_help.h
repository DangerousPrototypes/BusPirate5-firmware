

typedef struct ui_info_help
{
	uint help; //should be a section handling designator
	const char command[9]; //ugh
	uint description; //translation key
} ui_info_help;

void ui_help_print_args(opt_args (*args), struct command_result *res);
void ui_help_print(const struct ui_info_help (*ui_info_help), uint32_t count);

