
#define UI_UPDATE_IMAGE (1u << 0)
#define UI_UPDATE_INFOBAR (1u << 1)
#define UI_UPDATE_NAMES (1u << 2)
#define UI_UPDATE_LABELS (1u << 3)
#define UI_UPDATE_VOLTAGES (1u << 4)
#define UI_UPDATE_CURRENT (1u << 5)
#define UI_UPDATE_FORCE (1u << 6) // force label update
#define UI_UPDATE_ALL                                                                                                  \
    (UI_UPDATE_IMAGE | UI_UPDATE_INFOBAR | UI_UPDATE_NAMES | UI_UPDATE_LABELS | UI_UPDATE_VOLTAGES | UI_UPDATE_CURRENT)
