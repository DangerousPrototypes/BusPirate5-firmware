#ifndef LOGIC_BAR_H
#define LOGIC_BAR_H

// Function declarations
void la_draw_frame(void);
void logic_bar_redraw(uint32_t start_pos, uint32_t total_samples);
void la_periodic(void);
bool logic_bar_start(void);
void logic_bar_stop(void);
void logic_bar_hide(void);
void logic_bar_show(void);
void logic_bar_navigate(void);
void logic_bar_update(void);
void logic_bar_config(char low, char high);

#endif // LOGIC_BAR_H