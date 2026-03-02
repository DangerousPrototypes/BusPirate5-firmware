# Adding a VT100 Menu Bar to a Fullscreen App

> A step-by-step guide to integrating the VT100 menu framework into a Bus Pirate fullscreen application.  
> Reference implementation: `src/commands/global/menu_demo.c` — self-contained, compilable demo  
> Framework source: `src/lib/vt100_menu/vt100_menu.h`, `src/lib/vt100_menu/vt100_menu.c`

---

## Overview

The **vt100_menu** framework provides a classic TUI menu bar with dropdown selection — the VT100 ancestor of the modern desktop menu. It is designed for Bus Pirate fullscreen apps (edit, hexedit, scope, etc.) where users currently have to memorize hidden Ctrl-key combos.

> **Quick start:** Copy `src/commands/global/menu_demo.c` — it's a ~300-line standalone app with step-by-step numbered comments (Steps 1–12) explaining every integration pattern. Type `menu_demo` at the Bus Pirate prompt to see it in action.

```
 File  Edit  Search  Help                           F10=Menu
+------------+
| Save    ^S |
|------------|
| Quit    ^Q |
| Quit (no   |
|   save)    |
+------------+
 (editor content underneath)
```

### What the Framework Provides

| Feature | Description |
|---------|-------------|
| **Menu bar** | Blue background bar on row 1, "F10=Menu" hint when inactive |
| **Dropdown overlays** | Bordered dropdown boxes with items, separators, shortcuts |
| **Keyboard navigation** | Arrow keys, Enter, Escape, F10 toggle |
| **Accelerator keys** | First-letter matching (press `S` to select "Save") |
| **Key passthrough** | Unrecognised keys close the menu and are returned to the app |
| **Repaint callback** | Editor redraws content when switching dropdowns (no cascade blanking) |
| **Cursor management** | Hides cursor during menu interaction, restores on exit |
| **Checked/disabled items** | Flags for toggles and greyed-out items |
| **Zero allocation** | No malloc — state struct lives on the stack or in arena |

### What the Framework Does NOT Provide

- Mouse support (pure keyboard)
- Nested submenus (one level of dropdown only)
- Scroll within a dropdown (all items must fit on screen)
- Key event loop — the caller's existing loop drives everything

### Design Constraints

The framework is deliberately simple to match the embedded context:

- **No heap allocation** — the `vt100_menu_state_t` struct is ~60 bytes on the stack
- **No global state** — multiple instances are possible (but unlikely needed)
- **Pure VT100** — ASCII box drawing, 8-colour SGR attributes, no Unicode required
- **Callback-decoupled** — read_key/write_out/repaint are function pointers, not #includes
- **Separate translation unit** — `vt100_menu.c` compiles independently; key codes are runtime fields, not compile-time #defines

---

## Architecture

### Terminal Layout With Menu Bar

```
Row 1:    [Menu bar: " File  Edit  Search  Help        F10=Menu"]
Row 2:    [Editor content starts here]
  ...
Row N-1:  [Editor content]
Row N:    [Status bar / ruler]
```

When a dropdown is open, it overlays content rows without erasing them. When the dropdown closes or switches, the `repaint` callback redraws the content underneath.

### Data Flow

```
F10 pressed
  → editor sets menu_pending flag
  → main loop calls vt100_menu_run(&menu_state)
    → menu draws bar + dropdown
    → blocking key loop:
        ← arrows: navigate
        ← Enter: return action_id
        ← Escape/F10: return MENU_RESULT_CANCEL
        ← Ctrl-Q etc: return MENU_RESULT_PASSTHROUGH + unhandled_key
    → menu erases dropdown, restores cursor
  → caller dispatches action_id (save, quit, etc.)
  → caller pushes unhandled_key back into read_key (if passthrough)
  → full screen redraw
```

### Return Values

| Return | Meaning | Caller Action |
|--------|---------|---------------|
| `> 0` | Action ID of selected menu item | Dispatch to handler function |
| `MENU_RESULT_CANCEL` (-1) | User pressed Escape | Redraw screen |
| `MENU_RESULT_REDRAW` (-2) | Menu closed normally | Redraw screen |
| `MENU_RESULT_PASSTHROUGH` (-3) | Unrecognised key | Push `state.unhandled_key` back, redraw |

---

## File Structure

| File | Purpose |
|------|---------|
| `src/lib/vt100_menu/vt100_menu.h` | Public API, data structures, constants |
| `src/lib/vt100_menu/vt100_menu.c` | Rendering, navigation, interaction loop |
| `src/lib/myapp/myapp.c` | Your fullscreen app — defines menus, integrates framework |
| `src/CMakeLists.txt` | `vt100_menu.c` already listed; add your app's `.c` files |

---

## Step 1: Include the Header

```c
#ifdef BUSPIRATE
#include "lib/vt100_menu/vt100_menu.h"
#endif
```

The `#ifdef BUSPIRATE` guard allows the same source to build in both the Bus Pirate firmware and a host development environment.

---

## Step 2: Define Menu Items

Each menu item is a `vt100_menu_item_t` with a label, optional shortcut hint, action ID (>0), and flags:

```c
/* Action IDs — unique positive integers */
enum {
    ACT_SAVE       = 1,
    ACT_QUIT       = 2,
    ACT_QUIT_NOSAVE = 3,
    ACT_UNDO       = 10,
    ACT_HELP       = 40,
};

/* File menu items */
static const vt100_menu_item_t file_items[] = {
    { "Save",           "^S",  ACT_SAVE,        0 },
    { NULL,              NULL,  0,               MENU_ITEM_SEPARATOR },
    { "Quit",           "^Q",  ACT_QUIT,        0 },
    { "Quit (no save)",  NULL,  ACT_QUIT_NOSAVE, 0 },
};

/* Help menu items */
static const vt100_menu_item_t help_items[] = {
    { "Help",   ":help",  ACT_HELP,  0 },
};
```

### Item Flags

| Flag | Effect |
|------|--------|
| `MENU_ITEM_SEPARATOR` | Draws a `+------+` line; not selectable |
| `MENU_ITEM_DISABLED` | Greyed-out text; skipped by arrow navigation |
| `MENU_ITEM_CHECKED` | Shows `*` prefix beside the label |

### Sizing Rules

- Keep labels short — each item adds `label + shortcut + 4` characters of dropdown width
- The dropdown auto-sizes to the widest item (minimum 12 columns)
- Dropdowns clamp to the right screen edge; very long labels will work but may look cramped
- Test with `"Quit (no save)"` (14 chars) — this was the original edge case that found the padding bug

---

## Step 3: Define Top-Level Menus

Group items into top-level menus. The `count` field is the number of items (excluding any sentinel):

```c
static const vt100_menu_def_t my_menus[] = {
    { "File",   file_items,   4 },
    { "Help",   help_items,   1 },
};
#define MY_MENU_COUNT 2
```

The bar renders as: `" File  Help                              F10=Menu"`

---

## Step 4: I/O Callback Wrappers

The menu framework doesn't know how your app reads keys or writes output. Provide two thin wrappers:

```c
/* Read one key — delegates to your app's key decoder */
static int my_menu_read_key(void) {
    return read_key();  /* or editorReadKey(fd), etc. */
}

/* Write raw bytes to terminal — delegates to your app's output path */
static int my_menu_write(int fd, const void* buf, int count) {
    (void)fd;
    return (int)my_io_write(1, buf, (size_t)count);
}
```

---

## Step 5: Repaint Callback (Optional but Recommended)

When the user switches between menus with left/right arrows, the old dropdown's screen area needs restoration. Without a repaint callback, the framework blanks the area with spaces (functional but ugly — the "cascade" effect). With a repaint callback, your editor redraws its content cleanly:

```c
static void my_menu_repaint(void) {
    editor_refresh_screen(g_editor);  /* your normal screen paint */
}
```

---

## Step 6: Key Pushback (For Key Passthrough)

When the user presses a non-menu key (e.g. Ctrl-Q) while the menu is open, the framework closes and returns `MENU_RESULT_PASSTHROUGH` with the key in `state.unhandled_key`. To make that key reach your normal key handler, add a one-key pushback to your `read_key()`:

```c
static int read_key_pushback = -1;

void read_key_unget(int key) {
    read_key_pushback = key;
}

int read_key(void) {
    if (read_key_pushback >= 0) {
        int k = read_key_pushback;
        read_key_pushback = -1;
        return k;
    }
    /* ... normal key reading ... */
}
```

This is zero-overhead on the normal path — `-1` is checked once and skipped.

---

## Step 7: Initialise in Your Main Loop

In your app's entry point, after setting up the editor/viewer state:

```c
int my_app_run(const char* filename) {
    /* ... init editor, open file, clear screen ... */

    /* Initialise the menu system */
    vt100_menu_state_t menu_state;
    vt100_menu_init(&menu_state, my_menus, MY_MENU_COUNT,
        1,                              /* bar_row: row 1 */
        (uint8_t)screen_cols,
        (uint8_t)screen_rows,
        my_menu_read_key,
        my_menu_write);

    /* Override key codes for your app's enum values */
    menu_state.key_up    = MY_KEY_UP;
    menu_state.key_down  = MY_KEY_DOWN;
    menu_state.key_left  = MY_KEY_LEFT;
    menu_state.key_right = MY_KEY_RIGHT;
    menu_state.key_enter = MY_KEY_ENTER;
    menu_state.key_esc   = MY_KEY_ESC;
    menu_state.key_f10   = MY_KEY_F10;

    /* Set optional callbacks */
    menu_state.repaint   = my_menu_repaint;

    /* ... enter main loop ... */
}
```

### Why Key Codes Are Runtime Fields

Each app defines its own `enum` for arrow/enter/escape key codes, and those values may differ between apps. Since `vt100_menu.c` is a separate translation unit, `#define` overrides in the caller's `.c` file don't propagate. The runtime `state->key_*` fields solve this cleanly — set them once after `vt100_menu_init()`. See `menu_demo.c` Step 7 for the exact pattern.

---

## Step 8: Add F10 Key Decoding

Your app's key decoder needs to recognise F10 (ESC[21~). This is a two-digit CSI sequence:

```c
/* Inside your read_key() escape sequence handler, after reading seq[2]: */
} else if (seq[2] >= '0' && seq[2] <= '9') {
    /* Two-digit CSI: ESC [ N N ~ (e.g. F10 = ESC[21~) */
    char seq3;
    if (read_byte(&seq3) == 0) return KEY_ESC;
    if (seq3 == '~') {
        int code = (seq[1] - '0') * 10 + (seq[2] - '0');
        switch (code) {
        case 21: return KEY_F10;
        }
    }
}
```

Also add `KEY_F10` to your key enum:

```c
enum key_codes {
    /* ... existing keys ... */
    KEY_F10,            /* ESC[21~ */
};
```

---

## Step 9: Handle F10 in Your Key Processor

Flag the menu as pending — don't call `vt100_menu_run()` directly from the key handler:

```c
/* In your keypress handler, inside MODE_NORMAL: */
case KEY_F10: editor->menu_pending = true; return;
```

---

## Step 10: Integrate Into the Main Loop

The main loop draws the screen, draws the menu bar hint, processes keys, and checks the pending flag:

```c
while (1) {
    /* 1. Redraw screen content */
    editor_refresh_screen(editor);

    /* 2. Draw passive menu bar ("F10=Menu" hint) */
    vt100_menu_draw_bar(&menu_state);

    /* 3. Hide cursor if your app uses reverse-video cursor instead of terminal cursor */
    /* io_write(1, "\x1b[?25l", 6); */

    /* 4. Process keypress */
    editor_process_keypress(editor);

    /* 5. Check if menu was requested */
    if (editor->menu_pending) {
        editor->menu_pending = false;
        int action = vt100_menu_run(&menu_state);
        if (action > 0) {
            my_menu_dispatch(editor, action);
        } else if (action == MENU_RESULT_PASSTHROUGH && menu_state.unhandled_key) {
            read_key_unget(menu_state.unhandled_key);
        }
        clear_screen();
        /* continue to top of loop for full redraw */
    }
}
```

---

## Step 11: Dispatch Actions

Route action IDs to your editor's existing functions:

```c
static void my_menu_dispatch(struct editor* e, int action) {
    switch (action) {
    case ACT_SAVE:        editor_save(e); break;
    case ACT_QUIT:
        if (e->dirty) {
            editor_status(e, "Unsaved changes! Use File > Quit (no save)");
        } else {
            exit(0);
        }
        break;
    case ACT_QUIT_NOSAVE: exit(0); break;
    case ACT_UNDO:        editor_undo(e); break;
    case ACT_HELP:        editor_show_help(e); break;
    }
}
```

---

## Step 12: Adjust Screen Geometry

The menu bar occupies row 1, so your app's content must start at row 2:

```c
/* Header rows: menu bar (1) + any column headers + separators */
#ifdef BUSPIRATE
#define HEADER_ROWS 3   /* menu bar + column header + separator */
#else
#define HEADER_ROWS 0
#endif
```

In your screen refresh, start drawing at row 2:

```c
void editor_refresh_screen(struct editor* e) {
    struct charbuf* b = charbuf_create();
    charbuf_append(b, "\x1b[?25l", 6);       /* hide cursor */
#ifdef BUSPIRATE
    charbuf_append(b, "\x1b[2;1H", 6);       /* row 2, col 1 */
#else
    charbuf_append(b, "\x1b[H", 3);          /* row 1, col 1 */
#endif
    /* ... render content ... */
}
```

---

## Skip-Redraw Optimization (Advanced)

Some apps use a skip-redraw loop to avoid VT100 corruption during fast key auto-repeat. After a menu interaction, the skip-redraw state must be invalidated to force a repaint:

```c
if (editor->menu_pending) {
    /* ... menu handling ... */
    clear_screen();
    prev_line = -1;  /* invalidate skip-redraw state */
    break;           /* exit inner loop, force full redraw */
}
```

Without this invalidation, the screen stays blank after the menu closes because no editor state changed — the skip-redraw condition remains true.

---

## Cursor Visibility

The framework's `vt100_menu_draw_bar()` saves the cursor, hides it, draws, then restores and shows. This is correct for apps that use a visible terminal cursor, but wrong for apps that use reverse-video highlighting as their cursor.

**For apps that don't use a terminal cursor**, hide it again after drawing the bar:

```c
vt100_menu_draw_bar(&menu_state);
/* Reverse-video cursor — no terminal cursor needed */
io_write(1, "\x1b[?25l", 6);
```

**For apps that do use a terminal cursor**, no extra step is needed — `draw_bar` restores cursor visibility automatically.

---

## ESC Key Handling

In NORMAL mode, pressing ESC while the editor is already in NORMAL mode can blank the status bar if `editor_setmode(MODE_NORMAL)` clears the status message:

```c
/* Guard against redundant mode-set that blanks the status */
case KEY_ESC:
    if (!(e->mode & MODE_NORMAL)) editor_setmode(e, MODE_NORMAL);
    return;
```

---

## Colour Scheme Reference

The menu uses standard 8-colour SGR attributes for maximum terminal compatibility:

| Element | Foreground | Background | SGR Code |
|---------|-----------|------------|----------|
| Bar (normal) | White (37) | Blue (44) | `\x1b[0;37;44m` |
| Bar (selected tab) | Bold White (1;37) | Cyan (46) | `\x1b[1;37;46m` |
| Dropdown (normal) | Black (30) | White (47) | `\x1b[0;30;47m` |
| Dropdown (selected) | Bold White (1;37) | Blue (44) | `\x1b[1;37;44m` |
| Dropdown (disabled) | Dark Grey (90) | White (47) | `\x1b[0;90;47m` |
| Shortcut hint | Dark Grey (90) | White (47) | `\x1b[0;90;47m` |
| Shortcut (selected) | Cyan (36) | Blue (44) | `\x1b[0;36;44m` |

---

## API Reference

### vt100_menu_init()

```c
void vt100_menu_init(vt100_menu_state_t* state,
                     const vt100_menu_def_t* menus,
                     uint8_t menu_count,
                     uint8_t bar_row,
                     uint8_t cols, uint8_t rows,
                     int (*read_key_fn)(void),
                     int (*write_fn)(int, const void*, int));
```

Zeroes the state struct, stores pointers, sets default key codes. Call once at app startup. Override `state->key_*` fields and `state->repaint` after this call.

### vt100_menu_run()

```c
int vt100_menu_run(vt100_menu_state_t* state);
```

Enters the blocking interactive menu loop. Saves/hides cursor, draws bar + dropdown, reads keys until selection or cancel. Returns action_id (>0), `MENU_RESULT_CANCEL`, `MENU_RESULT_REDRAW`, or `MENU_RESULT_PASSTHROUGH`.

### vt100_menu_draw_bar()

```c
void vt100_menu_draw_bar(const vt100_menu_state_t* state);
```

Draws the passive menu bar with "F10=Menu" hint. Call once per screen refresh in your main loop. Saves/restores cursor position.

### vt100_menu_erase()

```c
void vt100_menu_erase(const vt100_menu_state_t* state);
```

Clears the menu bar row. Useful if you need to temporarily remove the bar.

### vt100_menu_reserved_rows()

```c
uint8_t vt100_menu_reserved_rows(const vt100_menu_state_t* state);
```

Returns 1 (bar active) or 0 (no menus). Use for geometry calculations.

---

## Minimal Integration Example

For a new fullscreen app that just needs a File menu with Save and Quit, the minimal integration is approximately 40 lines of code:

```c
#include "lib/vt100_menu/vt100_menu.h"

enum { ACT_SAVE = 1, ACT_QUIT = 2 };

static const vt100_menu_item_t file_items[] = {
    { "Save", "^S", ACT_SAVE, 0 },
    { "Quit", "^Q", ACT_QUIT, 0 },
};

static const vt100_menu_def_t menus[] = {
    { "File", file_items, 2 },
};

static int my_read_key(void) { return read_key(); }
static int my_write(int fd, const void* b, int n) {
    (void)fd; return io_write(1, b, n);
}
static void my_repaint(void) { refresh_screen(); }

/* In your main loop setup: */
vt100_menu_state_t ms;
vt100_menu_init(&ms, menus, 1, 1, cols, rows, my_read_key, my_write);
ms.key_up = KEY_UP; ms.key_down = KEY_DOWN;
ms.key_left = KEY_LEFT; ms.key_right = KEY_RIGHT;
ms.key_enter = KEY_ENTER; ms.key_esc = KEY_ESC;
ms.key_f10 = KEY_F10; ms.repaint = my_repaint;

/* In your main loop: */
refresh_screen();
vt100_menu_draw_bar(&ms);
process_keypress();
if (menu_pending) {
    menu_pending = false;
    int a = vt100_menu_run(&ms);
    if (a == ACT_SAVE) save();
    else if (a == ACT_QUIT) exit(0);
    else if (a == MENU_RESULT_PASSTHROUGH) read_key_unget(ms.unhandled_key);
    clear_screen();
}
```

---

## Future Integration Candidates

The menu framework can be added to any Bus Pirate fullscreen app that currently relies on hidden key combos. Potential candidates:

| App | Notes |
|-----|-------|
| `menu_demo` | Reference implementation — File/Color/Help menus |
| `hexedit` (hx) | File/Edit/Search/Help menus |
| `edit` (kilo) | File/Edit/Help menus |
| `scope` | Trigger settings, timebase, channel selection |
| `logic` | Trigger config, capture settings, export |
| `flash` (SPI flash tool) | Read/Write/Verify/Erase operations |
| Binary mode TUI | Protocol selection, connection settings |

---

## Common Pitfalls

| Pitfall | Symptom | Fix |
|---------|---------|-----|
| Key codes not overridden after init | Arrow keys swapped between editors | Set `state->key_*` fields explicitly |
| No `repaint` callback | Cascade-like blanks when switching menus | Set `state->repaint = your_refresh_fn` |
| Missing skip-redraw invalidation | Blank screen after menu closes | Set sentinel value (e.g. `prev_line = -1`) after `clear_screen()` |
| ESC in NORMAL mode blanks status | File name disappears after ESC + arrow | Guard: `if (!(mode & MODE_NORMAL)) setmode(NORMAL)` |
| Cursor visible during menu | Blinking cursor in status area | Add `\x1b[?25l` after `draw_bar()` for reverse-video editors |
| `#define` key overrides don't reach menu.c | Separate translation unit ignores caller's defines | Use runtime `state->key_*` fields (already the design) |
| Dropdown wider than label content | Right border pushed past box | Padding clamp must allow `pad = 0` (not `pad >= 1`) |
