// ============================================================================
// menu_demo.h — Header for the VT100 menu bar reference implementation
// ============================================================================
// See menu_demo.c for full documentation.

#ifndef BP_MENU_DEMO_H
#define BP_MENU_DEMO_H

extern const struct bp_command_def menu_demo_def;

/**
 * @brief Handler for menu_demo command — launches fullscreen VT100 menu demo.
 * @param res  Command result structure
 */
void menu_demo_handler(struct command_result* res);

#endif /* BP_MENU_DEMO_H */
