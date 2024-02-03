#ifndef UI_DISPLAY_H
#define UI_DISPLAY_H
void ui_display_enable(struct command_attributes *attributes, struct command_response *response);
void ui_display_int_display_format(opt_args (*args), struct command_result *res);
void ui_display_enable_args(opt_args (*args), struct command_result *res);
#endif
