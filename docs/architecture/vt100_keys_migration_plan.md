# vt100_keys — Build & Migration Plan

## Goal

Replace 6 independent VT100 escape-sequence parsers and 5 incompatible key
enums with a single shared `src/lib/vt100_keys/` library.  Each step is a
self-contained commit that builds, runs, and can be tested in isolation.

---

## Current Build Integration Points

Before changing anything, here is how each consumer currently compiles:

| Consumer | Build mechanism | I/O read function | Key enum |
|----------|----------------|-------------------|----------|
| **kilo** | `kilo_build.c` unity-includes `kilo.c` with `#define BUSPIRATE` | `kilo_io_read()` via `#define read` in `kilo_compat.h` | `enum KEY_ACTION` (local to `kilo.c`) |
| **hx** | `hx_build.c` unity-includes `util.c`, `editor.c`, etc. with `#define BUSPIRATE` | `hx_io_read()` via `#ifdef BUSPIRATE` in `util.c` | `enum key_codes` (in `util.h`) |
| **vt100_menu** | Direct source in `buspirate_common` list | Callback `state->read_key` — no parsing | `#define VT100_MENU_KEY_*` defaults, overridden per-caller |
| **menu_demo** | Direct source in `buspirate_common` list | `rx_fifo_try_get()` | `enum demo_keys` (local) |
| **linenoise** | Direct source in `buspirate_common` list | `lnReadBlocking()` callback | `enum KEY_ACTION` (local, no virtual keys) |
| **logic_bar** | Direct source in `buspirate_common` list | `rx_fifo_get_blocking()` | None (raw char compare) |
| **scope** | Direct source in `buspirate_common` list | `next_char()` → `rx_fifo_try_get()` | None (raw char compare) |

Key constraint: **kilo and hx are upstream-derived code** compiled via unity
build wrappers (`kilo_build.c` / `hx_build.c`).  Their internal enums and
functions are `static`.  The new library must work *alongside* their existing
code initially, with gradual replacement.

---

## Step 0 — Create the library (no consumers yet) ✅

**Status: DONE** — `src/lib/vt100_keys/vt100_keys.{c,h}` created, 50 host-side
tests passing, added to `buspirate_common` in `CMakeLists.txt`, builds clean on
RP2040 and ninja targets.

### Files created

```
src/lib/vt100_keys/
    vt100_keys.h      — Public API: enum, types, function declarations
    vt100_keys.c      — CSI decoder implementation
```

### API design

```c
/* vt100_keys.h */
#ifndef VT100_KEYS_H
#define VT100_KEYS_H

#include <stdint.h>

/* ── Virtual key codes ───────────────────────────────────────────────
 * 0x00–0x7F  : raw ASCII / control characters (pass through unchanged)
 * 0x100+     : virtual keys (escape sequences decoded to these)
 * Using 0x100 base avoids collision with any raw byte.              */
enum vt100_key {
    /* Control characters — keep raw values for direct comparison */
    VT100_KEY_CTRL_A    = 0x01,
    VT100_KEY_CTRL_B    = 0x02,
    VT100_KEY_CTRL_C    = 0x03,
    VT100_KEY_CTRL_D    = 0x04,
    VT100_KEY_CTRL_E    = 0x05,
    VT100_KEY_CTRL_F    = 0x06,
    VT100_KEY_CTRL_H    = 0x08,
    VT100_KEY_TAB       = 0x09,
    VT100_KEY_CTRL_K    = 0x0b,
    VT100_KEY_CTRL_L    = 0x0c,
    VT100_KEY_ENTER     = 0x0d,
    VT100_KEY_CTRL_N    = 0x0e,
    VT100_KEY_CTRL_P    = 0x10,
    VT100_KEY_CTRL_Q    = 0x11,
    VT100_KEY_CTRL_R    = 0x12,
    VT100_KEY_CTRL_S    = 0x13,
    VT100_KEY_CTRL_T    = 0x14,
    VT100_KEY_CTRL_U    = 0x15,
    VT100_KEY_CTRL_W    = 0x17,
    VT100_KEY_ESC       = 0x1b,
    VT100_KEY_BACKSPACE = 0x7f,

    /* Virtual keys (decoded from escape sequences) */
    VT100_KEY_UP        = 0x100,
    VT100_KEY_DOWN      = 0x101,
    VT100_KEY_RIGHT     = 0x102,
    VT100_KEY_LEFT      = 0x103,
    VT100_KEY_HOME      = 0x104,
    VT100_KEY_END       = 0x105,
    VT100_KEY_DELETE     = 0x106,
    VT100_KEY_INSERT    = 0x107,
    VT100_KEY_PAGEUP    = 0x108,
    VT100_KEY_PAGEDOWN  = 0x109,

    /* Function keys */
    VT100_KEY_F1        = 0x110,
    VT100_KEY_F2        = 0x111,
    VT100_KEY_F3        = 0x112,
    VT100_KEY_F4        = 0x113,
    VT100_KEY_F5        = 0x114,
    VT100_KEY_F6        = 0x115,
    VT100_KEY_F7        = 0x116,
    VT100_KEY_F8        = 0x117,
    VT100_KEY_F9        = 0x118,
    VT100_KEY_F10       = 0x119,
    VT100_KEY_F11       = 0x11a,
    VT100_KEY_F12       = 0x11b,
};

/* ── I/O callback signatures ─────────────────────────────────────── */

/**
 * Blocking read: must read exactly 1 byte into *c.
 * Return 1 on success, -1 on error.
 */
typedef int (*vt100_read_blocking_fn)(char *c);

/**
 * Non-blocking read: try to read 1 byte into *c.
 * Return 1 if a byte was read, 0 if nothing available.
 */
typedef int (*vt100_read_try_fn)(char *c);

/* ── Decoder state ───────────────────────────────────────────────── */

typedef struct {
    vt100_read_blocking_fn read_blocking;
    vt100_read_try_fn      read_try;
    int pushback;  /* -1 = empty */
} vt100_key_state_t;

/* ── Public API ──────────────────────────────────────────────────── */

/**
 * Initialise decoder state.
 *
 * @param read_blocking  Required. Blocks until one byte is available.
 * @param read_try       Optional (may be NULL). Used for timeout-based
 *                       bare-ESC detection.  If NULL, any ESC that isn't
 *                       followed by '[' or 'O' is returned as VT100_KEY_ESC
 *                       using the blocking read (which means a bare ESC press
 *                       blocks until the next byte — same as linenoise today).
 */
void vt100_key_init(vt100_key_state_t *s,
                    vt100_read_blocking_fn read_blocking,
                    vt100_read_try_fn read_try);

/**
 * Read one key (blocking).  Returns a raw ASCII byte or a VT100_KEY_* code.
 */
int vt100_key_read(vt100_key_state_t *s);

/**
 * Push back one key code so the next vt100_key_read() returns it immediately.
 */
void vt100_key_unget(vt100_key_state_t *s, int key);

#endif /* VT100_KEYS_H */
```

### Decoder logic (`vt100_keys.c`)

One function (~80 lines) that handles the full CSI matrix:

- `ESC [` + letter: `A`→UP, `B`→DOWN, `C`→RIGHT, `D`→LEFT, `H`→HOME, `F`→END
- `ESC [ N ~`: `1`→HOME, `2`→INSERT, `3`→DELETE, `4`→END, `5`→PAGEUP, `6`→PAGEDOWN, `7`→HOME, `8`→END
- `ESC [ NN ~`: two-digit codes → F5–F12 (15,17,18,19,20,21,23,24)
- `ESC O` + letter: `H`→HOME, `F`→END, `P`→F1, `Q`→F2, `R`→F3, `S`→F4
- Bare ESC: use `read_try` for timeout, fall back to blocking

### CMakeLists.txt change

Add two lines to the `buspirate_common` source list, near the existing `lib/vt100_menu/` entries:

```cmake
        lib/vt100_menu/vt100_menu.c
        lib/vt100_menu/vt100_menu.h
+       lib/vt100_keys/vt100_keys.c
+       lib/vt100_keys/vt100_keys.h
```

### Test

Create `tests/test_vt100_keys.c` — a host-side (gcc) test that:
1. Feeds known byte sequences through a mock `read_blocking` / `read_try`
2. Asserts the correct `VT100_KEY_*` code is returned
3. Tests pushback, bare ESC timeout, unknown sequences

```bash
gcc -I src -o tests/test_vt100_keys \
    tests/test_vt100_keys.c src/lib/vt100_keys/vt100_keys.c
./tests/test_vt100_keys   # 50/50 tests pass
```

### Build verification

```bash
cmake --build ./build_rp2040 --parallel --target bus_pirate5_rev10
```

Nothing uses the library yet, so this is a no-op integration — it just proves
the new files compile clean alongside everything else.

---

## Step 1 — Migrate `menu_demo.c` (trivial, proves the API) ✅

**Status: DONE** — Deleted ~80 lines of bespoke decoder, replaced with
`vt100_key_state_t` + two thin I/O callbacks. The 7-line key override block
was removed entirely (defaults now match after step 2).

**Risk: Minimal** — menu_demo is a demo/teaching command, not user-critical.

### Changes

1. **`menu_demo.c`**: Delete `enum demo_keys`, `demo_key_pushback`,
   `demo_read_key_unget()`, and the entire `demo_read_key()` function (~80 lines).
   Replace with:

   ```c
   #include "lib/vt100_keys/vt100_keys.h"

   static vt100_key_state_t demo_keys;

   /* I/O callbacks for vt100_keys */
   static int demo_read_blocking(char *c) {
       rx_fifo_get_blocking(c);
       return 1;
   }
   static int demo_read_try(char *c) {
       return rx_fifo_try_get(c) ? 1 : 0;
   }
   ```

2. Replace all `DEMO_KEY_*` references with `VT100_KEY_*`:

   | Old | New |
   |-----|-----|
   | `DEMO_KEY_UP` | `VT100_KEY_UP` |
   | `DEMO_KEY_DOWN` | `VT100_KEY_DOWN` |
   | `DEMO_KEY_LEFT` | `VT100_KEY_LEFT` |
   | `DEMO_KEY_RIGHT` | `VT100_KEY_RIGHT` |
   | `DEMO_KEY_ENTER` | `VT100_KEY_ENTER` |
   | `DEMO_KEY_ESC` | `VT100_KEY_ESC` |
   | `DEMO_KEY_CTRL_Q` | `VT100_KEY_CTRL_Q` |
   | `DEMO_KEY_F10` | `VT100_KEY_F10` |

3. Replace `demo_read_key()` calls with `vt100_key_read(&demo_keys)`.

4. Replace `demo_read_key_unget(k)` with `vt100_key_unget(&demo_keys, k)`.

5. In init, replace `demo_key_pushback = -1` with:
   ```c
   vt100_key_init(&demo_keys, demo_read_blocking, demo_read_try);
   ```

6. The `demo_menu_read_key` callback becomes:
   ```c
   static int demo_menu_read_key(void) {
       return vt100_key_read(&demo_keys);
   }
   ```

### vt100_menu key-code overrides now match directly

```c
    menu_state.key_up    = VT100_KEY_UP;
    menu_state.key_down  = VT100_KEY_DOWN;
    /* ... etc ... */
```

### Lines changed: ~80 deleted, ~20 added. Net: −60 lines.

### Build & test

```bash
cmake --build ./build_rp2040 --parallel --target bus_pirate5_rev10
# Flash and run: type "menu_demo", test F10, arrows, Ctrl-Q
```

---

## Step 2 — Migrate `vt100_menu.h` defaults ✅

**Status: DONE** — All 7 `VT100_MENU_KEY_*` defaults now reference
`VT100_KEY_*` values. Include uses project-relative path
(`"lib/vt100_keys/vt100_keys.h"`) since the CMake include dir is `src/`.
hx and kilo still override via `menu_state.key_*` fields — unaffected.

**Risk: None** — only changes defaults that every caller overrides anyway.

### Changes

1. Changed the `#define VT100_MENU_KEY_*` defaults in `vt100_menu.h` to use
   the `vt100_keys.h` enum values:

   ```c
   #include "lib/vt100_keys/vt100_keys.h"

   #ifndef VT100_MENU_KEY_UP
   #define VT100_MENU_KEY_UP      VT100_KEY_UP
   #endif
   /* ... etc ... */
   ```

2. Callers that use the shared enum **no longer need to override the key
   fields** after `vt100_menu_init()`.  Callers that still use their own
   enums (kilo, hx) continue to override — no change for them.

### Lines changed: ~14 modified in `vt100_menu.h`.

---

## Step 3 — Migrate `logic_bar.c` (easy, inline parser) ✅

**Status: DONE** — Replaced the `case '\033':` nested parser (~30 lines) with
`vt100_key_read()` + flat switch. The original non-blocking polling loop
(`rx_fifo_try_get` + `continue`) was replaced by a blocking `vt100_key_read()`
call, which is functionally equivalent (the old code just spun). Both blocking
and non-blocking callbacks are provided. The `vt100_key_state_t` is local to
`logic_bar_navigate()` rather than file-scope.

**Risk: Low** — the logic bar only decodes left/right arrows.

### Lines changed: ~30 deleted, ~20 added. Net: −10 lines.

### Bonus

The logic bar now also correctly handles Home/End/PgUp/PgDn/F-keys without
crashing or corrupting state (previously those sequences produced garbage
because the parser only consumed 3 bytes of what might be a 4-byte sequence).

---

## Step 4 — Migrate `scope.c` (defer to last)

> **NOTE:** Scope should be migrated **last**, not in the 0–3 first batch.
> The 3× duplicated key-handling blocks are a symptom of broader structural
> issues in `scope.c` (repeated code across display modes, mixed concerns).
> Tackle this after the library is proven in kilo/hx/linenoise, and ideally
> combine with a wider scope.c refactor pass.

**Risk: Low** — scope has 3 near-identical inline escape parsers.

### Changes

Scope's `next_char()` is non-blocking (`rx_fifo_try_get`), but the escape
parsing blocks on subsequent bytes.  Two options:

**Option A (minimal):** Create a blocking wrapper that retries `rx_fifo_try_get`
in a loop (scope already does this effectively with its `next_char` + loop).

**Option B (cleaner):** Scope already has `for (;;) { if (rx_fifo_try_get(&c))
...}` loops — use a blocking callback that wraps this pattern.

Use Option A:

```c
static int scope_read_blocking(char *c) {
    while (!rx_fifo_try_get(c)) {
        tight_loop_contents();
    }
    return 1;
}
static int scope_read_try(char *c) {
    return rx_fifo_try_get(c) ? 1 : 0;
}
static vt100_key_state_t scope_keys;
```

Replace each of the 3 `case 0x1b:` blocks (~15 lines each, ~45 total) with:

```c
int key = vt100_key_read(&scope_keys);
switch (key) {
    case VT100_KEY_UP:    scope_up();    break;
    case VT100_KEY_DOWN:  scope_down();  break;
    case VT100_KEY_RIGHT: scope_right(); break;
    case VT100_KEY_LEFT:  scope_left();  break;
    /* ... other keys ... */
}
```

**Important:** Scope reads single characters ('q', 'o', 'n', '+', etc.) in
the same loop.  These are raw bytes < 0x80, which `vt100_key_read()` passes
through unchanged.  The switch statement just merges the current two levels
(`case 0x1b:` + inner switch) into one flat switch.

### Lines changed: ~45 deleted, ~30 added per block, but blocks reduce from 3 to 1 pattern (or 3 calls to the same function). Net: ~−30 lines.

---

## Step 5 — Migrate `hx/util.c` (medium, upstream-derived)

**Risk: Medium** — hx is upstream-derived with `#ifdef BUSPIRATE` blocks.

### Strategy

Don't delete `read_key()` from `util.c`.  Instead, **gut its body** under
`#ifdef BUSPIRATE` to delegate to the shared library:

```c
int read_key() {
#ifdef BUSPIRATE
    return vt100_key_read(&hx_key_state);
#else
    /* ... original POSIX implementation unchanged for desktop builds ... */
#endif
}
```

Similarly for `read_key_unget()`:

```c
void read_key_unget(int key) {
#ifdef BUSPIRATE
    vt100_key_unget(&hx_key_state, key);
#else
    read_key_pushback = key;
#endif
}
```

### Key enum compatibility

The enum in `util.h` (`KEY_UP = 1000`, etc.) is used throughout `editor.c`.
Two options:

**Option A (safe, recommended):** Keep the `enum key_codes` in `util.h` but
redefine the values to match `vt100_keys.h` under `#ifdef BUSPIRATE`:

```c
#ifdef BUSPIRATE
#include "lib/vt100_keys/vt100_keys.h"
enum key_codes {
    KEY_NULL      = 0,
    KEY_CTRL_B    = VT100_KEY_CTRL_B,
    KEY_CTRL_D    = VT100_KEY_CTRL_D,
    /* ... */
    KEY_UP        = VT100_KEY_UP,
    KEY_DOWN      = VT100_KEY_DOWN,
    KEY_RIGHT     = VT100_KEY_RIGHT,
    KEY_LEFT      = VT100_KEY_LEFT,
    KEY_DEL       = VT100_KEY_DELETE,
    KEY_HOME      = VT100_KEY_HOME,
    KEY_END       = VT100_KEY_END,
    KEY_PAGEUP    = VT100_KEY_PAGEUP,
    KEY_PAGEDOWN  = VT100_KEY_PAGEDOWN,
    KEY_F10       = VT100_KEY_F10,
};
#else
/* original enum unchanged */
enum key_codes { ... };
#endif
```

This means **zero changes to `editor.c`** — it still uses `KEY_UP`, `KEY_F10`,
etc., and they now have values that match the shared library's output.

**Option B (aggressive):** `#define KEY_UP VT100_KEY_UP` etc. and delete
the enum.  More fragile for upstream merges.

### I/O callback setup

In `hx_compat.c` (or `hx_build.c`), add init:

```c
#include "lib/vt100_keys/vt100_keys.h"

vt100_key_state_t hx_key_state;

static int hx_read_blocking_cb(char *c) {
    return (hx_io_read(0, c, 1) == 1) ? 1 : -1;
}
static int hx_read_try_cb(char *c) {
    /* hx_io_read wraps rx_fifo which can be made non-blocking */
    return rx_fifo_try_get(c) ? 1 : 0;
}
```

Call `vt100_key_init(&hx_key_state, hx_read_blocking_cb, hx_read_try_cb)`
at the start of `hx_run()`.

### vt100_menu override block becomes unnecessary

After this step, the menu integration in `editor.c` can simplify:

```c
    /* Before (7 overrides needed): */
    menu_state.key_up    = KEY_UP;
    /* ... etc ... */

    /* After (values match defaults — overrides can be deleted): */
    /* (nothing needed — defaults in vt100_menu.h match vt100_keys.h) */
```

### Lines changed: ~100 lines modified across `util.h`, `util.c`, `hx_compat.c`, `editor.c`.

### Build order note

`hx_build.c` unity-includes `util.c`.  Since `util.c` will `#include
"lib/vt100_keys/vt100_keys.h"` under `#ifdef BUSPIRATE`, the include path
already works (CMake include dirs cover `src/`).

---

## Step 6 — Migrate `kilo.c` (medium, upstream-derived)

**Risk: Medium** — same pattern as hx but kilo is a single-file unity build.

### Strategy

Same as hx: gut `editorReadKey()` under `#ifdef BUSPIRATE`:

```c
int editorReadKey(int fd) {
#ifdef BUSPIRATE
    (void)fd;
    return vt100_key_read(&kilo_key_state);
#else
    /* ... original POSIX implementation unchanged ... */
#endif
}
```

### Key enum compatibility

Redefine the enum values under `#ifdef BUSPIRATE`:

```c
#ifdef BUSPIRATE
#include "lib/vt100_keys/vt100_keys.h"
enum KEY_ACTION{
    KEY_NULL = 0,
    CTRL_C    = VT100_KEY_CTRL_C,
    CTRL_D    = VT100_KEY_CTRL_D,
    CTRL_F    = VT100_KEY_CTRL_F,
    CTRL_H    = VT100_KEY_CTRL_H,
    TAB       = VT100_KEY_TAB,
    CTRL_L    = VT100_KEY_CTRL_L,
    ENTER     = VT100_KEY_ENTER,
    CTRL_Q    = VT100_KEY_CTRL_Q,
    CTRL_S    = VT100_KEY_CTRL_S,
    CTRL_T    = VT100_KEY_CTRL_T,
    CTRL_U    = VT100_KEY_CTRL_U,
    ESC       = VT100_KEY_ESC,
    BACKSPACE = VT100_KEY_BACKSPACE,
    ARROW_LEFT  = VT100_KEY_LEFT,
    ARROW_RIGHT = VT100_KEY_RIGHT,
    ARROW_UP    = VT100_KEY_UP,
    ARROW_DOWN  = VT100_KEY_DOWN,
    DEL_KEY     = VT100_KEY_DELETE,
    HOME_KEY    = VT100_KEY_HOME,
    END_KEY     = VT100_KEY_END,
    PAGE_UP     = VT100_KEY_PAGEUP,
    PAGE_DOWN   = VT100_KEY_PAGEDOWN,
    F10_KEY     = VT100_KEY_F10,
};
#else
enum KEY_ACTION{ /* original */ };
#endif
```

**Zero changes** to `editorProcessKeypress()` or any other function in kilo.c
— they still use `ARROW_UP`, `HOME_KEY`, etc.

### I/O callback

In `kilo_compat.c`:

```c
#include "lib/vt100_keys/vt100_keys.h"

vt100_key_state_t kilo_key_state;

static int kilo_read_blocking_cb(char *c) {
    return (kilo_io_read(STDIN_FILENO, c, 1) == 1) ? 1 : -1;
}
static int kilo_read_try_cb(char *c) {
    return rx_fifo_try_get(c) ? 1 : 0;
}
```

Call `vt100_key_init(...)` at the top of `kilo_run()`.

### Pushback simplification

Delete `editorReadKey_pushback` / `editorReadKey_unget()` — use
`vt100_key_unget(&kilo_key_state, ...)` instead (called from the menu
passthrough block).

### vt100_menu overrides become unnecessary

Same as hx — the 7-line override block after `vt100_menu_init()` can be deleted.

### Lines changed: ~80 lines modified in `kilo.c`, ~15 in `kilo_compat.c`.

---

## Step 7 — Migrate `linenoise.c` (hard, defer or partial)

**Risk: Highest** — linenoise is the command-line input handler used on every
keystroke.  It has deep integration with tab completion, UTF-8 multi-byte
reading, simple_mode fallback, and a non-blocking event-loop API
(`linenoiseEditFeed`).

### Why it's harder

1. **No virtual key codes**: linenoise dispatches actions inline in the ESC
   handler — there's no intermediate `KEY_UP` constant.  The action
   (e.g. `linenoiseEditHistoryNext`) is called directly.

2. **Non-blocking API**: `linenoiseEditFeed()` is designed to be called
   repeatedly with partial input.  The ESC handler must handle the case
   where only part of an escape sequence has arrived.

3. **`simple_mode` guards**: Many key actions are gated by
   `if (!l->simple_mode)`.  The shared library doesn't know about this.

4. **UTF-8 reading**: After the switch statement, linenoise reads remaining
   UTF-8 continuation bytes.  This interleaves with escape sequence handling.

### Recommended approach: Partial, opt-in

Don't replace linenoise's parser wholesale.  Instead:

**Phase 7a** — Extract a `vt100_key_decode_csi()` helper from `vt100_keys.c`
that takes pre-read bytes and returns a key code:

```c
/* Given that ESC has been read and seq[0..n-1] contain the bytes after ESC,
 * return the decoded virtual key, or VT100_KEY_ESC if unrecognised. */
int vt100_key_decode_csi(const char *seq, int len);
```

**Phase 7b** — In `linenoiseEditFeed()`, replace the `case ESC:` switch tree
with a call to `vt100_key_decode_csi()`, then dispatch on the returned code:

```c
case ESC:
    if (lnReadBlocking(l,seq,1) == -1) break;
    if (lnReadBlocking(l,seq+1,1) == -1) break;
    if (seq[0] == '[' && seq[1] >= '0' && seq[1] <= '9') {
        if (lnReadBlocking(l,seq+2,1) == -1) break;
    }
    int vk = vt100_key_decode_csi(seq, /* len */);
    switch (vk) {
        case VT100_KEY_UP:    linenoiseEditHistoryNext(l, PREV); break;
        case VT100_KEY_DOWN:  linenoiseEditHistoryNext(l, NEXT); break;
        case VT100_KEY_RIGHT: linenoiseEditMoveRight(l);         break;
        case VT100_KEY_LEFT:  linenoiseEditMoveLeft(l);          break;
        case VT100_KEY_HOME:  linenoiseEditMoveHome(l);          break;
        case VT100_KEY_END:   linenoiseEditMoveEnd(l);           break;
        case VT100_KEY_DELETE: linenoiseEditDelete(l);           break;
    }
    break;
```

This replaces ~60 lines of nested switch/case with ~15 lines, while keeping
linenoise's own I/O model, simple_mode guards, and UTF-8 handling intact.

**Phase 7c** (optional, future) — Add `VT100_KEY_*` constants to linenoise's
enum so higher layers can distinguish keys.  Currently `linenoiseEditFeed()`
returns a `char*` or `linenoiseEditMore`, so the key code is not exposed.
This only matters if a future feature needs it.

### Lines changed: ~60 deleted, ~20 added in linenoise.c. Plus ~15 lines
for the new `vt100_key_decode_csi()` in `vt100_keys.c/h`.

---

## Step 8 — Cleanup

After all consumers are migrated:

1. **Delete the `key_up/key_down/...` fields** from `vt100_menu_state_t` and
   have `vt100_menu_run()` compare against `VT100_KEY_*` constants directly.
   This removes the entire key-mapping indirection layer from the menu system.

2. **Standardize escape notation** to `\x1b` project-wide.  A simple
   find-and-replace in `logic_bar.c` (`\e[` → `\x1b[`) and `ui_term.c`
   (`\033[` → `\x1b[`) with no functional change.

3. **Update `docs/guides/vt100_menu_guide.md`** to reference `vt100_keys.h`
   instead of showing a custom enum in the integration example.

---

## Summary: Commit-by-Commit

| Step | Commit | Files changed | Risk | Lines Δ | Status |
|------|--------|---------------|------|---------|--------|
| 0 | Create `vt100_keys.{c,h}` + test | 3 new, 1 CMake edit | None | +200 | ✅ Done |
| 1 | Migrate `menu_demo.c` | 1 file | Minimal | −60 | ✅ Done |
| 2 | Update `vt100_menu.h` defaults | 1 file | None | ~0 | ✅ Done |
| 3 | Migrate `logic_bar.c` | 1 file | Low | −10 | ✅ Done |
| 4 | Migrate `hx` | 3 files (`util.h`, `util.c`, `hx_compat.c`) + editor.c cleanup | Medium | −60 | |
| 5 | Migrate `kilo` | 2 files (`kilo.c`, `kilo_compat.c`) | Medium | −50 | |
| 6 | Partial migrate `linenoise` | 2 files (`vt100_keys.c`, `linenoise.c`) | Medium | −40 | |
| 7 | Migrate `scope.c` *(last — combine with scope refactor)* | 1 file | Low | −30 | |
| 8 | Cleanup: menu fields, notation, docs | 5+ files | Low | −30 | |
| **Total** | | | | **~−80 net** (200 new lib − 280 removed duplication) | **4/9 done** |

## Build Verification at Each Step

Every step must pass:

```bash
# Cross-compile for RP2040
cmake --build ./build_rp2040 --parallel --target bus_pirate5_rev10

# Cross-compile for RP2350
cmake --build ./build_rp2350 --parallel --target bus_pirate6

# Host-side unit test (50 tests)
gcc -I src -o tests/test_vt100_keys \
    tests/test_vt100_keys.c src/lib/vt100_keys/vt100_keys.c \
    && tests/test_vt100_keys
```

## Rollback

Each step is independent.  If step N causes a regression, revert that single
commit.  Steps 0–4 are especially safe since they touch only non-critical or
demo code.  Steps 5–6 modify upstream-derived files but preserve the desktop
build path unchanged behind `#ifndef BUSPIRATE`.
