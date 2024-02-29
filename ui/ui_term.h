void ui_term_init(void);
void ui_term_detect(void);

void ui_term_color_text(uint32_t rgb);
void ui_term_color_background(uint32_t rgb);
uint32_t ui_term_color_text_background(uint32_t rgb_text, uint32_t rgb_background);
uint32_t ui_term_color_text_background_buf(char *buf, uint32_t rgb_text, uint32_t rgb_background);
char* ui_term_color_reset(void);
char* ui_term_color_prompt(void);
char* ui_term_color_info(void);
char* ui_term_color_notice(void);
char* ui_term_color_warning(void);
char* ui_term_color_error(void);
char* ui_term_color_num_float(void);
char* ui_term_color_pacman(void);
char* ui_term_cursor_show(void);
char* ui_term_cursor_hide(void);

void ui_term_error_report(uint32_t error_text);

uint32_t ui_term_get_user_input(void);
bool ui_term_cmdln_char_insert(char *c);
bool ui_term_cmdln_char_backspace(void);
bool ui_term_cmdln_char_delete(void);
void ui_term_cmdln_fkey(char *c);
void ui_term_cmdln_arrow_keys(char *c);
int ui_term_cmdln_history(int ptr);


