/**
 * @file displays.h
 * @brief Display mode abstraction layer.
 * @details Provides interface for different display modes (default, scope, disabled).
 */

#ifndef __DISPLAYS_H__
#define __DISPLAYS_H__

/**
 * @brief Display mode enumeration.
 */
enum {
    DISP_DEFAULT = 0,  /**< Default display mode */
#ifdef BP_USE_SCOPE
    DISP_SCOPE,        /**< Oscilloscope display mode */
#endif
    DISP_DISABLED,     /**< Display disabled */
    MAXDISPLAY         /**< Maximum display modes */
};

/**
 * @brief Display mode structure definition.
 */
typedef struct _display {
    void (*display_periodic)(void);                             /**< Regularly polled for events */
    uint32_t (*display_setup)(void);                            /**< Setup UI */
    uint32_t (*display_setup_exc)(void);                        /**< Real setup */
    void (*display_cleanup)(void);                              /**< Cleanup for HiZ */
    void (*display_settings)(void);                             /**< Display settings */
    void (*display_help)(void);                                 /**< Display protocol specific help */
    char display_name[32];                                      /**< Friendly name */
    uint32_t (*display_command)(struct command_result* result); /**< Per mode command parser */
    void (*display_lcd_update)(uint32_t flags);                 /**< Replacement for ui_lcd_update */
} _display;

extern struct _display displays[MAXDISPLAY];

/**
 * @brief Null function placeholder (void return).
 */
void nullfunc1(void);

/**
 * @brief Null function placeholder (uint32_t return from uint32_t).
 * @param c  Input parameter
 * @return   0
 */
uint32_t nullfunc2(uint32_t c);

/**
 * @brief Null function placeholder (uint32_t return).
 * @return  0
 */
uint32_t nullfunc3(void);

/**
 * @brief Null function placeholder (void return from uint32_t).
 * @param c  Input parameter
 */
void nullfunc4(uint32_t c);

/**
 * @brief Null function placeholder (const char* return).
 * @return  NULL
 */
const char* nullfunc5(void);

/**
 * @brief No help placeholder.
 */
void nohelp(void);

/**
 * @brief No periodic placeholder.
 */
void noperiodic(void);

#endif
