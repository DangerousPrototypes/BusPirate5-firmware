#ifndef _DISP_DISABLED_H_
#define _DISP_DISABLED_H_
uint32_t disp_disabled_setup(void);
uint32_t disp_disabled_setup_exc(void);
void disp_disabled_cleanup(void);
void disp_disabled_settings(void);
void disp_disabled_help(void);
void disp_disabled_lcd_update(uint32_t flags);
#endif
