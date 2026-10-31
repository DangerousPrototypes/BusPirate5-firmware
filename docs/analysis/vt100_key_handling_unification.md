# VT100 / Key Handling Unification Analysis

## Executive Summary

The firmware has **6 independent escape-sequence parsers** and **5 separate key-code enum definitions** spread across ~10,500 lines in 12+ source files. Each subsystem re-invents the same CSI decoding logic with slightly different I/O primitives, different constant names, different enum values, and different subsets of supported keys. This is a strong candidate for a shared `vt100_keys` library.

> **Update (2026-02-27):** The shared library (`src/lib/vt100_keys/`) has been
> created and the first batch of migrations is complete. `menu_demo.c`,
> `vt100_menu.h` defaults, and `logic_bar.c` now use it. See
> [migration plan](../architecture/vt100_keys_migration_plan.md) for status
> (4/9 steps done).

---

## 1. Inventory of All Key/VT100 Handling Sites

### 1.1 Escape-Sequence *Input* Parsers (the core duplication)

| # | File | Function | I/O primitive | Key enum | Keys decoded |
|---|------|----------|---------------|----------|--------------|
| 1 | [linenoise.c](../src/lib/bp_linenoise/linenoise.c#L1585) | `linenoiseEditFeed()` | `lnReadBlocking()` (callback on embedded) | Inline `enum KEY_ACTION` — no virtual key constants | ↑↓←→ Home End Del |
| 2 | [kilo.c](../src/lib/kilo/kilo.c#L282) | `editorReadKey()` | `read(fd,...)` | `ARROW_LEFT=1000, HOME_KEY, END_KEY, PAGE_UP, F10_KEY` | ↑↓←→ Home End Del PgUp PgDn F10 |
| 3 | [hx/util.c](../src/lib/hx/util.c#L110) | `read_key()` | `hx_io_read()` (embedded) / `read(STDIN)` | `KEY_UP=1000, KEY_HOME, KEY_END, KEY_PAGEUP, KEY_F10` | ↑↓←→ Home End Del PgUp PgDn F10 |
| 4 | ~~[menu_demo.c](../src/commands/global/menu_demo.c)~~ | ~~`demo_read_key()`~~ → `vt100_key_read()` | `rx_fifo_get_blocking()` callback | **`VT100_KEY_*` (shared)** | ✅ Migrated |
| 5 | ~~[logic_bar.c](../src/toolbars/logic_bar.c)~~ | ~~Inline `case '\033'`~~ → `vt100_key_read()` | `rx_fifo_get_blocking()` callback | **`VT100_KEY_*` (shared)** | ✅ Migrated |
| 6 | [scope.c](../src/display/scope.c#L749) ×3 | Inline `case 0x1b` | `next_char()` → `rx_fifo_try_get()` | None — raw char compare | ↑↓←→ only |

**Every parser implements the same algorithm:**
```
read ESC → read '[' → read letter or digit → map to virtual key
```
Yet each does it independently with copy-pasted code.

### 1.2 Key-Code Enum Definitions (5 incompatible sets)

| Location | Style | Arrow Up | Home | F10 | Pushback? |
|----------|-------|----------|------|-----|-----------|
| [linenoise.c#L506](../src/lib/bp_linenoise/linenoise.c#L506) | `enum KEY_ACTION` | *(no constant — inline dispatch)* | *(inline)* | N/A | No |
| [kilo.c#L122](../src/lib/kilo/kilo.c#L122) | `enum KEY_ACTION` | `ARROW_UP = 1000` | `HOME_KEY` | `F10_KEY` | Yes |
| [hx/util.h#L16](../src/lib/hx/util.h#L16) | `enum key_codes` | `KEY_UP = 1000` | `KEY_HOME` | `KEY_F10` | Yes |
| ~~[menu_demo.c](../src/commands/global/menu_demo.c)~~ | ~~`enum demo_keys`~~ **deleted** | `VT100_KEY_UP = 0x100` | `VT100_KEY_HOME` | `VT100_KEY_F10` | Yes (shared) | ✅ Migrated |
| [vt100_menu.h](../src/lib/vt100_menu/vt100_menu.h#L231) | `#define` defaults | `VT100_MENU_KEY_UP = VT100_KEY_UP` | *(N/A)* | `VT100_MENU_KEY_F10 = VT100_KEY_F10` | N/A | ✅ Updated |

Note: the menu system uses configurable key fields in its state struct, set to `VT100_MENU_KEY_*` defaults at init. After the step 2 migration, defaults now reference `VT100_KEY_*` from the shared library. Callers using the shared decoder no longer need overrides; callers with their own enums (hx, kilo) still override.

### 1.3 VT100 *Output* Sequences (less duplicated but still scattered)

| Location | Notation | What it emits |
|----------|----------|---------------|
| [ui_term.c](../src/ui/ui_term.c) + [ui_term.h](../src/ui/ui_term.h) | `\033[` (octal) | Full cursor/scroll/color/erase abstraction — the "official" layer |
| [vt100_menu.c](../src/lib/vt100_menu/vt100_menu.c) | `\x1b[` (hex) | cursor pos, save/restore, hide/show, colors, erase — **re-implements** ui_term helpers |
| [kilo.c](../src/lib/kilo/kilo.c) | `\x1b[` (hex) | cursor hide/show, position, erase, colors — **re-implements** same primitives |
| [hx/editor.c](../src/lib/hx/editor.c) | `\x1b[` (hex) | alt-screen, cursor, color attributes — **re-implements** same primitives |
| [edit.c](../src/commands/global/edit.c) / [hexedit.c](../src/commands/global/hexedit.c) | `\x1b[` | alt-screen enter/exit |
| [menu_demo.c](../src/commands/global/menu_demo.c) | `\x1b[` | alt-screen, cursor, colors |
| [logic_bar.c](../src/toolbars/logic_bar.c) | `\e[` (GCC ext) | cursor pos, erase, scroll region |
| [scope.c](../src/display/scope.c) | mixed `\x1b[` | cursor, colors, erase (heavy inline use) |

Three different **notations** for the same byte (`\033[` vs `\x1b[` vs `\e[`), making grep/audit harder.

### 1.4 Pushback / Unget Mechanisms (3 identical patterns)

Each full-screen app needs a one-key pushback buffer for menu passthrough:

```c
// kilo.c
static int editorReadKey_pushback = -1;
static void editorReadKey_unget(int key) { editorReadKey_pushback = key; }

// hx/util.c  
static int read_key_pushback = -1;
void read_key_unget(int key) { read_key_pushback = key; }

// menu_demo.c
static int demo_key_pushback = -1;
static void demo_read_key_unget(int key) { demo_key_pushback = key; }
```

Identical logic, three copies.

---

## 2. Root Causes of Divergence

1. **Third-party origins**: linenoise, kilo, and hx are all imported open-source projects that each brought their own terminal handling.

2. **Different I/O models**: The embedded firmware has multiple ways to read a character:
   - `lnReadBlocking()` — linenoise callback wrapper over `linenoiseReadBlockingFn`
   - `read(fd, ...)` — POSIX (kilo, desktop hx)
   - `hx_io_read()` — embedded hx wrapper
   - `rx_fifo_get_blocking()` / `rx_fifo_try_get()` — Bus Pirate FIFO (logic_bar, scope, menu_demo)
   
3. **No shared abstraction existed** when each feature was added.

---

## 3. What Could Be Unified

### 3.1 A `vt100_keys` Library (Highest Value)

A single shared module providing:

```c
/* vt100_keys.h — Unified terminal key decoder */

/* Standard virtual key codes */
enum vt100_key {
    VT100_KEY_NULL      = 0,
    /* Ctrl keys keep their raw values (1-26) */
    VT100_KEY_CTRL_A    = 0x01,
    VT100_KEY_CTRL_B    = 0x02,
    // ... etc ...
    VT100_KEY_TAB       = 0x09,
    VT100_KEY_ENTER     = 0x0d,
    VT100_KEY_ESC       = 0x1b,
    VT100_KEY_BACKSPACE = 0x7f,
    
    /* Virtual keys (above 0xFF — cannot collide with raw bytes) */
    VT100_KEY_UP        = 0x100,
    VT100_KEY_DOWN,
    VT100_KEY_RIGHT,
    VT100_KEY_LEFT,
    VT100_KEY_HOME,
    VT100_KEY_END,
    VT100_KEY_DELETE,
    VT100_KEY_PAGEUP,
    VT100_KEY_PAGEDOWN,
    VT100_KEY_INSERT,
    VT100_KEY_F1,    /* ... through F12 */
    // ...
    VT100_KEY_F10,
    VT100_KEY_F12,
};

/* I/O callback signature: read 1 char, return 1 on success, 0 on timeout/empty, -1 on error */
typedef int (*vt100_read_fn)(char *c);

/* Decoder state (for non-blocking / incremental use) */
typedef struct {
    vt100_read_fn read_blocking;    /* blocks until char available */
    vt100_read_fn read_nonblocking; /* returns 0 if nothing available */
    int pushback;                   /* one-key pushback buffer */
} vt100_key_state_t;

void vt100_key_init(vt100_key_state_t *s, vt100_read_fn blocking, vt100_read_fn nonblocking);
int  vt100_key_read(vt100_key_state_t *s);        /* blocking read, returns vt100_key */
void vt100_key_unget(vt100_key_state_t *s, int k); /* push back one key */
```

**Key design decisions:**
- Virtual keys start at `0x100` (not 1000) to leave clean space for raw bytes and future UTF-8 codepoints
- Two callback flavors: blocking (for full-screen apps) and non-blocking (for scope/logic_bar timeout-based parsing)
- Built-in pushback buffer eliminates the 3 duplicate unget patterns
- Stateful struct allows multiple simultaneous decoders (not needed today but costs nothing)

### 3.2 A `vt100_output` Shared Helper Set (Medium Value)

The `ui_term.c` module already provides this for the firmware's main output path, but the third-party libs (kilo, hx) and the menu system can't use it because:
- `ui_term_*()` functions call `printf()` directly — the libs need write-callback or buffer output
- `ui_term_*()` guards behind `system_config.terminal_ansi_color` — the full-screen apps **always** want VT100

The `_buf()` variants in `ui_term.h` partially solve this but only cover a subset. **Recommendation:** expand the `_buf()` API to cover all primitives (alt-screen, color SGR, etc.) and have the libraries use those.

### 3.3 Normalize Escape Notation (Low Cost, High Clarity)

Standardize on **one notation** project-wide. Recommendation: `\x1b` (hex) because:
- Most portable (works on any C compiler, not just GCC)
- Already dominant in the codebase
- `\033` (octal) is also portable but less readable
- `\e` is a GCC extension — non-standard

---

## 4. Migration Plan

### Phase 1: Create `src/lib/vt100_keys/` (new shared library)

1. Implement `vt100_keys.h` / `vt100_keys.c` as described above
2. Full CSI decoder handling: `ESC [`, `ESC O`, single-digit `ESC [ N ~`, two-digit `ESC [ NN ~`
3. Include comprehensive key support: arrows, Home/End, Delete, Insert, PgUp/PgDn, F1–F12
4. Unit-testable: the decoder operates on a callback, so tests can inject byte sequences

### Phase 2: Migrate consumers (one at a time, lowest risk first)

| Order | Consumer | Difficulty | Notes | Status |
|-------|----------|------------|-------|--------|
| 1 | `menu_demo.c` | Trivial | Replaced `demo_read_key()` + enum entirely | ✅ Done |
| 2 | `vt100_menu.h` | Easy | Defaults now reference `VT100_KEY_*` | ✅ Done |
| 3 | `logic_bar.c` | Easy | Replaced inline `case '\033'` with `vt100_key_read()` | ✅ Done |
| 4 | `hx/util.c` | Easy | Replace `read_key()` with thin wrapper around `vt100_key_read()` | |
| 5 | `kilo.c` | Medium | Replace `editorReadKey()`, update enum values under `#ifdef BUSPIRATE` | |
| 6 | `linenoise.c` | Hard | Deep integration with completion, UTF-8, multi-mode; defer or adapt partially | |
| 7 | `scope.c` | Easy | Replace 3× inline `case 0x1b` blocks; defer to last (needs broader refactor) | |

### Phase 3: Normalize output (optional, lower priority)

1. Expand `ui_term_*_buf()` to cover alt-screen, color SGR, and all primitives
2. Gradually replace raw `printf("\x1b[...")` calls in kilo/hx/menu with buffer helpers
3. Standardize on `\x1b` notation for any remaining raw sequences

---

## 5. Estimated Impact

### Code reduction
- **~300 lines** of duplicate CSI parser code eliminated (6 parsers → 1)
- **~50 lines** of duplicate pushback/unget code eliminated (3 copies → 0)
- **5 enum definitions** → 1 shared header

### Bug-fix propagation
Currently if a terminal emits an unusual escape variant (e.g. `ESC [ 7 ~` for Home on rxvt), the fix must be applied in up to 6 places. With a shared library, it's fixed once.

### New feature velocity
Adding a new key (e.g. Shift+arrows `ESC [1;2A`, or mouse reporting) requires changing **one** decoder, and all apps get it automatically.

### Risk
- Low for phases 1–2 (new code, opt-in migration)
- Medium for linenoise (phase 2, step 7) — heavily modified third-party code
- The `vt100_menu` system already abstracts keys via function pointers, so it becomes even simpler with shared codes

---

## 6. Comparison Table: Current vs Proposed

| Aspect | Before | Current (4/9 done) | After Full Unification |
|--------|--------|-------------------|------------------------|
| Escape parsers | 6 independent copies | 4 remaining (kilo, hx, linenoise, scope) | 1 shared library |
| Key enums | 5 incompatible definitions | 3 remaining (kilo, hx, linenoise) | 1 shared header |
| Pushback buffers | 3 separate implementations | 1 remaining (hx, kilo) | Built into library |
| I/O coupling | Each parser hardcodes its read function | 2 migrated to callbacks | Callback-based, decoupled |
| Key coverage | Varies (linenoise: no F-keys; scope: arrows only) | menu_demo + logic_bar: full | Consistent full coverage |
| Adding a new key | Touch 6 files | Touch 4 files | Touch 1 file |
| Fixing a parse bug | Touch 6 files | Touch 4 files | Touch 1 file |
| Notation | Mixed `\033[` / `\x1b[` / `\e[` | Unchanged | Standardized `\x1b[` |
| vt100_menu key mapping | Caller must override 7 fields after init | Defaults match shared enum | Direct enum match, no overrides |

---

## 7. Files Affected (Complete List)

### Input parsing (escape-sequence decoders to replace)
- [src/lib/bp_linenoise/linenoise.c](../src/lib/bp_linenoise/linenoise.c#L1585) — `linenoiseEditFeed()` ESC handler
- [src/lib/kilo/kilo.c](../src/lib/kilo/kilo.c#L282) — `editorReadKey()`
- [src/lib/hx/util.c](../src/lib/hx/util.c#L110) — `read_key()`
- ~~[src/commands/global/menu_demo.c](../src/commands/global/menu_demo.c) — `demo_read_key()`~~ ✅ Migrated
- ~~[src/toolbars/logic_bar.c](../src/toolbars/logic_bar.c) — inline ESC parser~~ ✅ Migrated
- [src/display/scope.c](../src/display/scope.c#L749) — inline ESC parser (×3 copies within the file)

### Shared library (new)
- [src/lib/vt100_keys/vt100_keys.h](../src/lib/vt100_keys/vt100_keys.h) — shared enum + API ✅
- [src/lib/vt100_keys/vt100_keys.c](../src/lib/vt100_keys/vt100_keys.c) — CSI decoder ✅
- [tests/test_vt100_keys.c](../../tests/test_vt100_keys.c) — 50 host-side tests ✅

### Key enum definitions (to consolidate)
- [src/lib/bp_linenoise/linenoise.c](../src/lib/bp_linenoise/linenoise.c#L506) — `enum KEY_ACTION`
- [src/lib/kilo/kilo.c](../src/lib/kilo/kilo.c#L122) — `enum KEY_ACTION`
- [src/lib/hx/util.h](../src/lib/hx/util.h#L16) — `enum key_codes`
- ~~[src/commands/global/menu_demo.c](../src/commands/global/menu_demo.c) — `enum demo_keys`~~ ✅ Deleted
- [src/lib/vt100_menu/vt100_menu.h](../src/lib/vt100_menu/vt100_menu.h#L231) — `#define VT100_MENU_KEY_*` ✅ Now references `VT100_KEY_*`

### VT100 output (scattered raw sequences to eventually centralize)
- [src/lib/vt100_menu/vt100_menu.c](../src/lib/vt100_menu/vt100_menu.c) — menu rendering
- [src/lib/kilo/kilo.c](../src/lib/kilo/kilo.c) — editor rendering
- [src/lib/hx/editor.c](../src/lib/hx/editor.c) — hex editor rendering
- [src/commands/global/edit.c](../src/commands/global/edit.c#L113) — alt-screen management
- [src/commands/global/hexedit.c](../src/commands/global/hexedit.c#L114) — alt-screen management
- [src/commands/global/menu_demo.c](../src/commands/global/menu_demo.c#L411) — alt-screen + UI
- [src/toolbars/logic_bar.c](../src/toolbars/logic_bar.c) — toolbar rendering (uses `\e[`)
- [src/display/scope.c](../src/display/scope.c) — scope rendering
- [src/ui/ui_term.c](../src/ui/ui_term.c) — canonical output abstraction (keep & extend)
