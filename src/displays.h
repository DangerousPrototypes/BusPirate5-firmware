#ifndef __DISPLAYS_H__
#define __DISPLAYS_H__
// #include "pirate.h"

enum {
    DISP_DEFAULT = 0,
#ifdef BP_USE_SCOPE
    DISP_SCOPE,
#endif
    DISP_DISABLED,
    MAXDISPLAY
};

typedef struct _display {
    void (*display_periodic)(void);                             // regularly polled for events (byte arrival, etc.)
    uint32_t (*display_setup)(void);                            // setup UI
    uint32_t (*display_setup_exc)(void);                        // real setup
    void (*display_cleanup)(void);                              // cleanup for HiZ
    void (*display_settings)(void);                             // display settings
    void (*display_help)(void);                                 // display protocol specific help
    char display_name[32];                                      // friendly name (promptname)
    uint32_t (*display_command)(struct command_result* result); // per mode command parser - ignored if 0
    void (*display_lcd_update)(uint32_t flags);                 // replacement for ui_lcd_update if non-0
} _display;

extern struct _display displays[MAXDISPLAY];

void nullfunc1(void);
uint32_t nullfunc2(uint32_t c);
uint32_t nullfunc3(void);
void nullfunc4(uint32_t c);
const char* nullfunc5(void);
void nohelp(void);
void noperiodic(void);
#endif
