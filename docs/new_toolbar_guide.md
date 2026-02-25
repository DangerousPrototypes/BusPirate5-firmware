# Implementing a New Bus Pirate Toolbar

> A step-by-step guide to creating a bottom-of-screen toolbar for Bus Pirate 5/6/7 firmware.  
> Reference implementation: `src/toolbars/pin_watcher.c` (2-line, Core1 periodic)  
> Secondary example: `src/toolbars/sys_stats.c` (1-line, Core1 periodic)

---

## Overview

A **toolbar** is a fixed-height block of terminal rows pinned to the bottom of the VT100 screen. Toolbars display live data (pin states, system stats, logic analyzer waveforms) below the scrollable command area, without interfering with normal `printf` output.

The toolbar system provides:

- **Registry** — central array that tracks up to 4 active toolbars
- **Layout** — automatic scroll-region calculation so toolbars stack correctly
- **Lifecycle** — activate/teardown with proper erase, scroll-region adjustment, and redraw
- **Dual rendering paths** — Core0 printf for simple toolbars, Core1 buffer for live-data toolbars
- **Periodic updates** — Core1 state machine calls your callback every ~100ms with change flags

### Two Rendering Paths

| Path | Runs on | API | Best for |
|------|---------|-----|----------|
| **Core0 printf** | Core0 | `.draw` callback uses `printf()` | Event-driven, one-shot (test_toolbar, logic_bar) |
| **Core1 _buf()** | Core1 | `.update_core1` callback uses `snprintf`/`_buf()` | Periodic live data (pin_watcher, sys_stats, statusbar) |

Most new toolbars should use the **Core1 path** — it avoids interleaving with command output and provides flicker-free updates.

### How the Core1 Path Works

1. The LCD tick fires on Core1 every ~100ms
2. Core1 builds an `update_flags` bitmask (what changed: voltages? labels? current?)
3. The Core1 state machine iterates registered toolbars with `.update_core1` callbacks
4. Your callback writes VT100 content into a shared 1024-byte buffer (`tx_tb_buf`)
5. The USB TX state machine drains the buffer atomically to the terminal
6. No interleaving with Core0 `printf` output — the SPSC queue is checked first

### How the Core0 Path Works

1. Something triggers a redraw (activate, teardown, screen clear)
2. `toolbar_redraw_all()` iterates toolbars and calls each `.draw` callback
3. Your `.draw` callback uses `printf()` / `ui_term_*()` directly
4. The prepare/release envelope handles cursor save/hide/restore/show

---

## Architecture

### Terminal Layout

```
┌─────────────────────────┐ ← row 1
│ scrollable command area │
│ (printf output goes     │
│  here, scrolls up)      │
├─────────────────────────┤ ← scroll_bottom = rows - total_height
│ [toolbar N]  newest     │   e.g. pin_watcher (2 lines)
│ [toolbar 1]             │   e.g. sys_stats   (1 line)
│ [toolbar 0]  bottommost │   e.g. statusbar   (4 lines, anchor_bottom)
└─────────────────────────┘ ← row = terminal_ansi_rows
```

Toolbars stack upward from the bottom. The VT100 scroll region (`\033[1;Nr`) is set so printf output only scrolls within the command area, never overwriting toolbars.

### Stacking Order

- Toolbars appear in **registration order**: first registered = bottommost on screen
- `anchor_bottom = true` forces a toolbar to index 0 (always bottommost) — used by the statusbar
- Most toolbars leave `anchor_bottom = false` and stack above the statusbar

### Core1 State Machine

```
TB_C1_IDLE → TB_C1_RENDERING → TB_C1_DRAINING → TB_C1_RENDERING → … → TB_C1_IDLE
```

- **IDLE**: no update in progress
- **RENDERING**: find next toolbar with `.update_core1`, call it, wrap in cursor envelope
- **DRAINING**: wait for USB TX to finish sending the buffer, then advance to next toolbar
- The state machine processes one toolbar per loop iteration, avoiding long blocking

---

## File Structure

| File | Purpose |
|------|---------|
| `src/toolbars/mytoolbar.c` | All toolbar logic — struct, callbacks, start/stop API |
| `src/toolbars/mytoolbar.h` | Public API: `start()`, `stop()`, `is_active()`, optionally `update()` |
| `src/commands/global/cmd_toolbar.c` | Add a toggle action (for dev/test access) |
| `src/CMakeLists.txt` | Add the `.c` file to the build |

---

## Step 1: Includes

Start with the required headers. Annotate each include so future readers know what it provides:

```c
#include <stdio.h>                 // printf (for start/stop, NOT for Core1 rendering)
#include <stdint.h>                // uint8_t, uint16_t, uint32_t
#include <stdbool.h>               // bool
#include "pico/stdlib.h"           // RP2040 SDK
#include "pirate.h"                // Global defines, pin labels, colors
#include "system_config.h"         // system_config struct (terminal size, etc.)
#include "ui/ui_term.h"            // VT100 helpers: cursor_position_buf, color_buf, etc.
#include "ui/ui_toolbar.h"         // Toolbar API: toolbar_t, toolbar_activate, etc.
#include "ui/ui_flags.h"           // UI_UPDATE_* flags for selective rendering
```

Add hardware-specific includes as needed (e.g. `pirate/bio.h` for GPIO, `pirate/psu.h` for PSU state).

---

## Step 2: Height Constant

Every toolbar must declare a fixed height — the number of terminal rows it occupies:

```c
#define MY_TOOLBAR_HEIGHT 2
```

The registry uses this to calculate `toolbar_scroll_bottom()` and `toolbar_get_start_row()`. The height must not change while the toolbar is registered.

---

## Step 3: The .draw Callback

The `.draw` callback is called from Core0 during `toolbar_redraw_all()` (triggered by activate, teardown, or screen clear).

**For Core1 toolbars** (periodic live data), `.draw` is a one-liner trampoline:

```c
static void my_toolbar_draw_cb(toolbar_t* tb, uint16_t start_row, uint16_t width) {
    (void)tb; (void)start_row; (void)width;
    toolbar_update_blocking();  // signals Core1 to call .update_core1 with UI_UPDATE_ALL
}
```

This works because `toolbar_update_blocking()` sends an intercore message that bypasses `terminal_toolbar_pause`, so it functions correctly even inside the `toolbar_draw_prepare()` envelope.

**For Core0-only toolbars** (event-driven), `.draw` contains the actual rendering:

```c
static void my_toolbar_draw_cb(toolbar_t* tb, uint16_t start_row, uint16_t width) {
    for (uint16_t i = 0; i < tb->height; i++) {
        ui_term_cursor_position(start_row + i, 0);
        ui_term_erase_line();
        printf("Row %u content", i + 1);
    }
}
```

**Important:** `.draw` is called inside a prepare/release envelope — cursor is already saved and hidden. Do NOT call `toolbar_draw_prepare()`/`toolbar_draw_release()` from within `.draw`.

---

## Step 4: The Core1 _buf() Callback

This is the heart of a periodic toolbar. It is called from the Core1 state machine with a pre-allocated buffer and the toolbar's screen coordinates.

### Signature

```c
static uint32_t my_update_core1_cb(toolbar_t* tb, char* buf, size_t buf_len,
                                   uint16_t start_row, uint16_t width,
                                   uint32_t update_flags);
```

### Parameters

| Parameter | Description |
|-----------|-------------|
| `tb` | Pointer to your toolbar struct (access `.height`, `.owner_data`) |
| `buf` | Buffer to write VT100 escape sequences + text into |
| `buf_len` | Available bytes (1024 shared across all toolbars per cycle) |
| `start_row` | First terminal row, 1-based (from `toolbar_get_start_row()`) |
| `width` | Terminal width in columns |
| `update_flags` | Bitmask of `UI_UPDATE_*` flags indicating what changed |

### Return Value

| Return | Meaning |
|--------|---------|
| `0` | Nothing to send — skip this cycle. Zero overhead. |
| `> 0` | Number of bytes written. State machine sends via USB. |

### update_flags Reference

| Flag | Meaning |
|------|---------|
| `UI_UPDATE_VOLTAGES` | ADC pin voltage readings changed |
| `UI_UPDATE_LABELS` | Pin names or configuration changed |
| `UI_UPDATE_CURRENT` | PSU current sense changed |
| `UI_UPDATE_INFOBAR` | Info bar content changed |
| `UI_UPDATE_FORCE` | Force full repaint (from `toolbar_update_blocking()`) |
| `UI_UPDATE_ALL` | All of the above combined |

### Rendering Pattern

```c
static uint32_t my_update_core1_cb(toolbar_t* tb, char* buf, size_t buf_len,
                                   uint16_t start_row, uint16_t width,
                                   uint32_t update_flags) {
    (void)tb;
    uint32_t len = 0;

    /* 1. Gate: skip if nothing we care about changed */
    const uint32_t care_mask = UI_UPDATE_VOLTAGES | UI_UPDATE_FORCE;
    if (!(update_flags & care_mask)) {
        return 0;   // skip — zero overhead
    }

    /* 2. Position cursor at our row */
    len += ui_term_cursor_position_buf(&buf[len], buf_len - len, start_row, 0);

    /* 3. Render content with colors */
    uint32_t cols = 0;
    len += ui_term_color_text_background_buf(&buf[len], buf_len - len, 0xFFFFFF, 0x004466);
    int n = snprintf(&buf[len], buf_len - len, " My Data: %d ", some_value);
    len += n; cols += n;

    /* 4. Pad remaining columns (instead of erase_line — avoids flicker) */
    for (uint16_t c = cols; c < width; c++) {
        if (len < buf_len - 1) buf[len++] = ' ';
    }

    /* 5. Reset colors */
    len += snprintf(&buf[len], buf_len - len, "%s", ui_term_color_reset());

    return len;
}
```

---

## Step 5: The toolbar_t Struct

The toolbar descriptor is the single source of truth for how your toolbar integrates with the registry:

```c
static toolbar_t my_toolbar = {
    .name         = "my_toolbar",        // shown by `toolbar list`
    .height       = MY_TOOLBAR_HEIGHT,   // terminal rows to reserve
    .enabled      = false,               // managed by activate/teardown — don't set manually
    .anchor_bottom = false,              // true = always bottommost (statusbar only)
    .owner_data   = NULL,                // opaque pointer for your private state
    .draw         = my_toolbar_draw_cb,  // Core0 full-paint callback
    .update       = NULL,                // reserved for future use
    .update_core1 = my_update_core1_cb,  // Core1 periodic callback (NULL for Core0-only)
    .destroy      = NULL,                // cleanup on unregister (NULL if not needed)
};
```

### Field Reference

| Field | Required | Notes |
|-------|----------|-------|
| `.name` | Yes | Human-readable, used by `toolbar list` debug command |
| `.height` | Yes | Must match your rendering. Cannot change while registered. |
| `.enabled` | — | Set by `toolbar_activate()` / `toolbar_teardown()`. Initialize to `false`. |
| `.anchor_bottom` | — | Only `true` for the statusbar. Leave `false` for normal toolbars. |
| `.owner_data` | — | Use if you need shared state between callbacks without file-scope globals. |
| `.draw` | Yes | Core0 painter. For Core1 toolbars: one-liner calling `toolbar_update_blocking()`. |
| `.update` | — | Reserved. Set to `NULL`. |
| `.update_core1` | — | Core1 renderer. `NULL` for Core0-only toolbars (test_toolbar, logic_bar). |
| `.destroy` | — | Called on `toolbar_unregister()`. Free resources, stop timers. `NULL` if not needed. |

---

## Step 6: Start / Stop / Is_Active Lifecycle

Every toolbar exposes a public API with at least three functions:

### Start

```c
bool my_toolbar_start(void) {
    if (my_toolbar.enabled) {
        return true;   // already active — idempotent
    }

    // 1. Push blank lines to scroll existing content up
    //    Without this, text would be hidden behind the toolbar
    for (uint16_t i = 0; i < MY_TOOLBAR_HEIGHT; i++) {
        printf("\r\n");
    }

    // 2. Register, set scroll region, redraw all
    if (!toolbar_activate(&my_toolbar)) {
        return false;  // registry full (max 4 toolbars)
    }

    // 3. Cursor is now outside the scroll region — move it back in
    ui_term_cursor_position(toolbar_scroll_bottom(), 0);
    return true;
}
```

The three steps in `start()` are critical:

1. **Push lines** — `printf("\r\n")` × height makes visual room before the scroll region shrinks
2. **Activate** — `toolbar_activate()` handles register → scroll region → `toolbar_redraw_all()`
3. **Reposition** — the cursor ends up below the new scroll bottom; move it back up

### Stop

```c
void my_toolbar_stop(void) {
    if (!my_toolbar.enabled) {
        return;   // not active — idempotent
    }
    toolbar_teardown(&my_toolbar);
}
```

`toolbar_teardown()` handles everything: erase all toolbar rows, unregister, recalculate scroll region, redraw remaining toolbars. No manual cleanup needed.

### Is_Active

```c
bool my_toolbar_is_active(void) {
    return my_toolbar.enabled;
}
```

---

## Step 7: On-Demand Updates

Toolbars with `.update_core1` get periodic updates automatically from the LCD tick (~100ms). But sometimes you want to force an immediate refresh from Core0:

```c
void my_toolbar_update(void) {
    if (!my_toolbar.enabled) {
        return;
    }
    toolbar_update_blocking();
}
```

`toolbar_update_blocking()` sends an intercore message to Core1, which calls `.update_core1` with `UI_UPDATE_ALL`. It blocks until Core1 acknowledges.

For Core0-only toolbars, use `toolbar_redraw_all()` instead, or call your draw function wrapped in `toolbar_draw_prepare()`/`toolbar_draw_release()`.

---

## Step 8: Core1 Rendering Rules

These rules apply to any code inside a `.update_core1` callback:

| Rule | Why |
|------|-----|
| **No `printf()`** | printf goes through Core0's SPSC queue — you're on Core1 |
| **No `ui_term_*()`** calls | These use printf internally — use `_buf()` variants instead |
| **`snprintf()` only** | Write into the provided buffer at `&buf[len]` |
| **Column-pad with spaces** | Don't use `ui_term_erase_line` — it causes flicker (momentarily blanks the line) |
| **Track column count** | Count visible characters, pad to `width` to overwrite stale content |
| **Return 0 to skip** | If nothing changed, return 0 — zero USB overhead |
| **No cursor save/restore** | The caller wraps your content in a cursor envelope automatically |
| **Reset colors at end** | Always end with `ui_term_color_reset()` to avoid color bleeding |

### The _buf() Pattern

Every `ui_term_*()` function that outputs to the terminal has a `_buf()` variant that writes into a caller-provided buffer instead of calling printf:

```c
// Instead of:
ui_term_cursor_position(row, col);          // ← uses printf (Core0 only)

// Use:
len += ui_term_cursor_position_buf(&buf[len], buf_len - len, row, col);  // ← buffer (Core1 safe)
```

Available `_buf()` variants:
- `ui_term_cursor_position_buf()` — move cursor to row,col
- `ui_term_cursor_save_buf()` / `ui_term_cursor_restore_buf()` — save/restore cursor
- `ui_term_cursor_hide_buf()` / `ui_term_cursor_show_buf()` — hide/show cursor
- `ui_term_color_text_background_buf()` — set foreground and background color
- `ui_term_erase_line_buf()` — erase current line (avoid — causes flicker)

### Column Padding (Anti-Flicker)

The key technique for flicker-free rendering. Instead of erasing the line first (which momentarily blanks it), overwrite the entire line width with content + spaces:

```c
uint32_t cols = 0;

// Render your content, tracking visible columns
int n = snprintf(&buf[len], buf_len - len, " Data: %d ", value);
len += n; cols += n;

// Pad remaining columns with spaces
for (uint16_t c = cols; c < width; c++) {
    if (len < buf_len - 1) buf[len++] = ' ';
}
```

---

## Step 9: The Header File

Keep the header minimal — only the public lifecycle API:

```c
// src/toolbars/my_toolbar.h
#pragma once

#include <stdbool.h>

bool my_toolbar_start(void);
void my_toolbar_stop(void);
bool my_toolbar_is_active(void);
void my_toolbar_update(void);   // optional: on-demand refresh
```

---

## Step 10: Registration in cmd_toolbar.c

Add a toggle action to the `toolbar` debug command. This follows the existing pattern for stats/pins:

### 10a: Add include

```c
#include "toolbars/my_toolbar.h"
```

### 10b: Add action enum value

```c
enum toolbar_actions {
    TOOLBAR_LIST = 1,
    TOOLBAR_TEST,
    TOOLBAR_REMOVE,
    TOOLBAR_STATS,
    TOOLBAR_PINS,
    TOOLBAR_STATUSBAR,
    TOOLBAR_MYTOOLBAR,    // ← add
};
```

### 10c: Add action definition

```c
static const bp_command_action_t toolbar_action_defs[] = {
    // ...existing entries...
    { TOOLBAR_MYTOOLBAR, "mytoolbar", 0x00 },
};
```

### 10d: Add usage string

```c
"Toggle my toolbar:%s toolbar mytoolbar",
```

### 10e: Add switch case

```c
case TOOLBAR_MYTOOLBAR: {
    if (my_toolbar_is_active()) {
        my_toolbar_stop();
        printf("My toolbar removed\r\n");
    } else {
        if (my_toolbar_start()) {
            printf("My toolbar started\r\n");
        } else {
            printf("Registry full — cannot add toolbar\r\n");
        }
    }
    break;
}
```

---

## Step 11: CMakeLists.txt

Add your `.c` file to the source list in `src/CMakeLists.txt`:

```cmake
toolbars/my_toolbar.c
```

---

## Quick Reference: Toolbar API

### Lifecycle Functions

| Function | Core | Purpose |
|----------|------|---------|
| `toolbar_activate(tb)` | Core0 | Register + scroll region + redraw all |
| `toolbar_teardown(tb)` | Core0 | Erase + unregister + scroll region + redraw remaining |
| `toolbar_teardown_all()` | Core0 | Tear down every toolbar (used by reboot/bootloader) |

### Query Functions

| Function | Returns |
|----------|---------|
| `toolbar_total_height()` | Sum of all registered toolbar heights |
| `toolbar_count_registered()` | Number of registered toolbars |
| `toolbar_scroll_bottom()` | Last row of the scroll region (rows - total_height) |
| `toolbar_get_start_row(tb)` | First row (1-based) assigned to this toolbar |

### Rendering Control

| Function | Core | Purpose |
|----------|------|---------|
| `toolbar_redraw_all()` | Core0 | Redraw all toolbars (wraps in prepare/release) |
| `toolbar_draw_prepare()` | Core0 | Pause toolbar updates, hide cursor |
| `toolbar_draw_release()` | Core0 | Show cursor, resume updates |
| `toolbar_update_blocking()` | Core0 | Signal Core1 to render, block until done |
| `toolbar_pause_updates()` | Either | Set `terminal_toolbar_pause = true` |
| `toolbar_resume_updates()` | Either | Set `terminal_toolbar_pause = false` |

### Core1 State Machine

| Function | Core | Purpose |
|----------|------|---------|
| `toolbar_core1_begin_update(flags)` | Core1 | Start a rendering cycle with given update_flags |
| `toolbar_core1_service()` | Core1 | Process one state machine step (call every Core1 loop) |

### Debug

| Function | Purpose |
|----------|---------|
| `toolbar_print_registry()` | Print all registered toolbars with positions |

---

## Patterns & Recipes

### 1-Line Stats Bar (sys_stats pattern)

The simplest periodic toolbar. One row, always renders (uptime changes every tick), ignores `update_flags`:

```c
#define HEIGHT 1

// .update_core1 ignores update_flags — always renders
static uint32_t cb(toolbar_t* tb, char* buf, size_t buf_len,
                   uint16_t start_row, uint16_t width, uint32_t update_flags) {
    (void)tb; (void)update_flags;
    uint32_t len = 0;
    len += ui_term_cursor_position_buf(&buf[len], buf_len - len, start_row, 0);
    // ... render one row, pad to width ...
    return len;
}
```

See: `src/toolbars/sys_stats.c`

### Multi-Row with Selective Update (pin_watcher pattern)

Multiple rows where some are static (labels) and others change every tick (live data). Uses `update_flags` to skip static rows on normal ticks:

```c
#define HEIGHT 2

static uint32_t cb(toolbar_t* tb, char* buf, size_t buf_len,
                   uint16_t start_row, uint16_t width, uint32_t update_flags) {
    const uint32_t care_mask = UI_UPDATE_VOLTAGES | UI_UPDATE_LABELS | UI_UPDATE_FORCE;
    if (!(update_flags & care_mask)) return 0;

    bool full = (update_flags & (UI_UPDATE_LABELS | UI_UPDATE_FORCE)) != 0;

    if (full) {
        // Row 1: static labels — only on full paint
    }
    // Row 2: live data — always when care_mask matches
    return len;
}
```

See: `src/toolbars/pin_watcher.c`

### Core0-Only Toolbar (test_toolbar / logic_bar pattern)

For event-driven toolbars that don't need periodic updates. No `.update_core1` — the `.draw` callback uses printf directly:

```c
static toolbar_t my_toolbar = {
    // ...
    .draw         = my_draw_cb,      // uses printf()
    .update_core1 = NULL,            // no Core1 rendering
};

static void my_draw_cb(toolbar_t* tb, uint16_t start_row, uint16_t width) {
    for (uint16_t i = 0; i < tb->height; i++) {
        ui_term_cursor_position(start_row + i, 0);
        ui_term_erase_line();
        printf("Row %u content", i + 1);
    }
}
```

Trigger redraws manually: `toolbar_redraw_all()` or wrap your draw call in `toolbar_draw_prepare()`/`toolbar_draw_release()`.

See: `src/commands/global/cmd_toolbar.c` (test_toolbar)

### Anchor Bottom (statusbar pattern)

Force a toolbar to always be the bottommost on screen, regardless of registration order:

```c
static toolbar_t my_toolbar = {
    // ...
    .anchor_bottom = true,   // inserted at index 0 in the registry
};
```

Only the statusbar uses this. New toolbars should leave it `false`.

See: `src/ui/ui_statusbar.c`

---

## Checklist

- [ ] Create `src/toolbars/mytoolbar.c` with toolbar struct and callbacks
- [ ] Define height constant
- [ ] Implement `.draw` callback (one-liner for Core1, or printf for Core0)
- [ ] Implement `.update_core1` callback (if periodic — with `update_flags` gating)
- [ ] Initialize `toolbar_t` struct with all fields
- [ ] Implement `start()` — guard, push lines, `toolbar_activate()`, reposition cursor
- [ ] Implement `stop()` — guard, `toolbar_teardown()`
- [ ] Implement `is_active()` — return `.enabled`
- [ ] Optionally implement `update()` — guard, `toolbar_update_blocking()`
- [ ] Create `src/toolbars/mytoolbar.h` with public API declarations
- [ ] Add toggle action in `src/commands/global/cmd_toolbar.c`
- [ ] Add `.c` file to `src/CMakeLists.txt`
- [ ] Build and verify with `toolbar list`, `toolbar mytoolbar`

---

## Related Documentation

- [new_mode_guide.md](new_mode_guide.md) — Implementing a new protocol mode
- [new_command_guide.md](new_command_guide.md) — Implementing a new command
- `src/toolbars/pin_watcher.c` — Full reference implementation with extensive comments
- `src/toolbars/sys_stats.c` — Simpler 1-line periodic toolbar
- `src/ui/ui_toolbar.h` — Toolbar API declarations and struct documentation
- `src/ui/ui_toolbar.c` — Registry, layout engine, and Core1 state machine implementation
- `src/ui/ui_flags.h` — `UI_UPDATE_*` flag definitions
- `src/commands/global/cmd_toolbar.c` — Debug/test command with toggle actions
