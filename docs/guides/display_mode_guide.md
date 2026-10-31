+++
weight = 90418
title = 'Adding a New Display Mode'
+++

# Adding a New Display Mode

> How to implement and register a custom display mode in the Bus Pirate firmware.

---

## The `_display` Struct

Each display mode implements callbacks defined in `src/displays.h`:

```c
typedef struct _display {
    void (*display_periodic)(void);
    uint32_t (*display_setup)(void);
    uint32_t (*display_setup_exc)(void);
    void (*display_cleanup)(void);
    void (*display_settings)(void);
    void (*display_help)(void);
    char display_name[32];
    uint32_t (*display_command)(struct command_result* result);
    void (*display_lcd_update)(uint32_t flags);
} _display;
```

| Field | Type | Purpose |
|-------|------|---------|
| `display_periodic` | function ptr | Regular polling for events |
| `display_setup` | function ptr | Setup UI |
| `display_setup_exc` | function ptr | Hardware setup |
| `display_cleanup` | function ptr | Cleanup on exit |
| `display_settings` | function ptr | Display current settings |
| `display_help` | function ptr | Protocol-specific help |
| `display_name` | `char[32]` | Friendly name |
| `display_command` | function ptr | Mode command parser |
| `display_lcd_update` | function ptr | LCD screen update |

## Display Mode Enum

New entries go before `DISP_DISABLED` so `MAXDISPLAY` stays correct:

```c
enum {
    DISP_DEFAULT = 0,
#ifdef BP_USE_SCOPE
    DISP_SCOPE,
#endif
    DISP_DISABLED,
    MAXDISPLAY
};
```

## Dispatch Table

The `displays[]` array in `src/displays.c` maps each enum value to its callbacks:

```c
struct _display displays[MAXDISPLAY] = {
    {
        noperiodic,
        disp_default_setup,
        disp_default_setup_exc,
        disp_default_cleanup,
        disp_default_settings,
        0,
        "Default",
        0,
        disp_default_lcd_update,
    },
#ifdef BP_USE_SCOPE
    {
        scope_periodic,
        scope_setup,
        scope_setup_exc,
        scope_cleanup,
        scope_settings,
        scope_help,
        "Scope",
        scope_commands,
        scope_lcd_update,
    },
#endif
    {
        noperiodic,
        disp_disabled_setup,
        disp_disabled_setup_exc,
        disp_disabled_cleanup,
        disp_disabled_settings,
        0,
        "Disabled",
        0,
        0,
    },
};
```

## Null Function Placeholders

Use these for callbacks you don't need:

```c
void noperiodic(void);    // No periodic polling
void nohelp(void);        // No help text
void nullfunc1(void);     // Void placeholder
uint32_t nullfunc3(void); // Returns 0
```

## Steps to Add a New Display Mode

1. Create `src/display/mydisp.h` and `src/display/mydisp.c`.
2. Implement the callback functions: `setup`, `setup_exc`, `cleanup`, `settings`, `lcd_update`.
3. Add an enum entry before `DISP_DISABLED` in `displays.h` (with `#ifdef` guard if optional).
4. Add the corresponding dispatch table entry in `displays.c`.
5. Add `#define BP_USE_MYDISP` in `pirate.h` if the mode is optional.
6. Add source files to `src/CMakeLists.txt`.

## LCD Update

- The LCD is a 240×320 IPS display (on BP5 REV10).
- `display_lcd_update(uint32_t flags)` is called periodically from Core 1.
- Use `BP_LCD_WIDTH` and `BP_LCD_HEIGHT` from the platform header.
- Refresh rate: `BP_LCD_REFRESH_RATE_MS` (default 500 ms).

## Related Documentation

- [board_abstraction_guide.md](board_abstraction_guide.md) — LCD pin definitions
- Source: `src/displays.h`, `src/displays.c`, `src/display/scope.h`
