
typedef enum _ui_update_flag_t {
    UI_UPDATE_NONE     = 0u,
    UI_UPDATE_IMAGE    = (1u << 0),
    UI_UPDATE_INFOBAR  = (1u << 1),
    UI_UPDATE_NAMES    = (1u << 2),
    UI_UPDATE_LABELS   = (1u << 3),
    UI_UPDATE_VOLTAGES = (1u << 4),
    UI_UPDATE_CURRENT  = (1u << 5),
    UI_UPDATE_FORCE    = (1u << 6), // force label update
    // space for one more UI update flag before this changes from uint8_t to uint16_t....

    UI_UPDATE_ALL      = (
        UI_UPDATE_IMAGE    |
        UI_UPDATE_INFOBAR  |
        UI_UPDATE_NAMES    |
        UI_UPDATE_LABELS   |
        UI_UPDATE_VOLTAGES |
        UI_UPDATE_CURRENT  |
        0u
        ),
} ui_update_flag_t;

// if size grew, need to check all uses of this enum to ensure larger size is OK
// Likely yes, but ... safer to check.
_Static_assert(sizeof(ui_update_flag_t) == sizeof(uint8_t), "ui_update_flag_t larger than byte; review of code required");
