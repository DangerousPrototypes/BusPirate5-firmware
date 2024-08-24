#ifndef LOGIC_BAR_H
#define LOGIC_BAR_H

// Function declarations
void la_draw_frame(void);
void logic_bar_redraw(uint32_t start_pos, uint32_t total_samples);
void la_periodic(void);
void logic_bar_start(void);
void logic_bar_stop(void);
void logic_bar_hide(void);
void logic_bar_show(void);
void logic_bar_navigate(void);
void logic_bar_update(void);

#endif // LOGIC_BAR_H