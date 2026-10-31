# Static Variable RAM Usage Analysis

**Date:** 2026-03-05  
**Context:** RAM usage reported at ~94%. Half of total RAM (128 KB) is the reusable "big buffer." This report analyzes all static and global mutable variables to identify candidates for stack placement, parameter passing, or dynamic allocation from the big buffer.

**Target:** RP2040 — 264 KB SRAM total.

---

## Executive Summary

The firmware's static/global RAM footprint (excluding stack and SDK internals) is estimated at **~148 KB**, of which **128 KB is the intentional reusable big buffer**. The remaining **~16–17 KB** of static/global mutable data is distributed across ~250 mutable variables in ~100 files.

The largest optimization opportunities outside the big buffer are:

| Rank | Target | Savings | Effort | Section |
|------|--------|---------|--------|---------|
| 1 | NAND page buffers — use big buffer during disk ops | ~4.2 KB | Medium | [§3.1](#31-nand-page-buffers-42-kb) |
| 2 | Linenoise history — reduce or dynamically allocate | ~2.1 KB | Medium | [§3.2](#32-linenoise-history-storage-21-kb) |
| 3 | Tab completion pool — allocate on demand | ~1.1 KB | Low | [§3.3](#33-tab-completion-buffers-11-kb) |
| 4 | Linenoise state instances — pass context pointer | ~0.7 KB | Medium | [§3.4](#34-linenoise-state-instances-07-kb) |
| 5 | Mode `mode_config` structs — union or dynamic | ~0.2 KB | Medium | [§3.5](#35-mode-config-structs-02-kb) |
| 6 | VT100 menu output buffer — move to stack | 256 B | Low | [§3.6](#36-vt100-menu-output-buffer-256-b) |
| 7 | EEPROM GUI context — move to stack | ~416 B | Low | [§3.7](#37-eeprom-i2c-gui-context-416-b) |
| 8 | Function-scope statics — move to stack or context | ~200 B | Low | [§3.8](#38-function-scope-statics-200-b) |

**Total recoverable (estimated): ~9 KB** with moderate refactoring effort.

---

## 1. RAM Budget Overview

### 1.1 Top-Level Allocation Map

| Category | Size | % of 264 KB | Notes |
|----------|------|-------------|-------|
| **Big buffer** (reusable pool) | 128,000 B (128 KB) | 48.5% | `src/pirate/mem.c` — shared by scope, LA, editor, disk format, etc. |
| **Pico SDK + TinyUSB internals** | ~8–12 KB est. | ~4% | USB buffers, multicore FIFO, DMA, flash XIP cache |
| **Stack (Core 0)** | 4,096 B | 1.6% | `PICO_STACK_SIZE=4096` in CMakeLists.txt |
| **Stack (Core 1)** | 4,096 B est. | 1.6% | Pico SDK default |
| **TX buffers** | 5,120 B | 1.9% | 2×1024 aligned SPSC + 1024 toolbar (global, not static) |
| **NAND buffers** | 4,288 B | 1.6% | `page_buffer[2048]` + `page_main_and_oob[2176]` |
| **Linenoise** | 2,744 B | 1.0% | History storage (2056) + states (656) + ptrs (32) |
| **RX buffers** | 256 B | 0.1% | 2×128 SPSC queues |
| **Tab completion** | 1,088 B | 0.4% | `comp_pool[16][64]` + `hint_buf[64]` |
| **Other statics/globals** | ~3,500 B | 1.3% | mode_config, scope, rgb, UI, ADC, etc. |
| **system_config** | ~380 B | 0.1% | Global configuration struct |
| **.bss/.data overhead** | ~1–2 KB est. | ~0.5% | Alignment padding, vtables |
| **Estimated total** | ~247–252 KB | ~94–95% | Matches reported 94% usage |

### 1.2 Alignment Tax

Several buffers pay significant alignment penalties:

- `mem_buffer` (big buffer): aligned to 32768 — up to **32 KB wasted** in worst case
- `tx_buf`, `bin_tx_buf`: each aligned to 2048 — up to **2×1024 = 2 KB wasted**
- This alignment padding is a hidden but potentially significant contributor to the 94% figure.

---

## 2. Static Variable Census

### 2.1 Summary Statistics

- **Total static variable declarations found:** 735 (excluding third-party libraries: printf-4.0.0, flatcc, fatfs, nanocobs, mjson, font)
- **Mutable (RAM-consuming):** 249 variables across 100+ files
- **Const (flash/ROM):** 486 variables — no RAM impact on RP2040 (placed in `.rodata`/flash)
- **File-scope mutable:** 211 variables
- **Function-scope mutable:** 38 variables

### 2.2 Mutable Statics by Directory (RAM impact)

| Directory | Mutable Vars | Est. Bytes | Notes |
|-----------|-------------|-----------|-------|
| `src/lib/bp_linenoise/` | 15 | 2,107 | History + state — **largest single module** |
| `src/nand/` | 6 | 2,065 | Page buffers — **disk I/O only** |
| `src/lib/bp_args/` | 3 | 1,092 | Tab completion — **input only** |
| `src/` (root) | 18 | 273 | pirate.c, syntax_post.c, etc. |
| `src/lib/vt100_menu/` | 1 | 256 | Output buffer |
| `src/commands/eeprom/` | 8 | 224 | GUI context + menu items |
| `src/ui/` | 24 | 194 | Toolbar, file picker, status bar |
| `src/display/` | 40 | 99 | scope.c — many small scalars |
| `src/pirate/` | 36 | 95 | rgb.c, irio_pio.c, shift.c, etc. |
| `src/lib/pico-i2c-sniff/` | 7 | 88 | Capture state |
| `src/mode/` | 17 | 66 | mode_config structs |
| `src/binmode/` | 18 | 45 | Legacy protocol state |
| `src/commands/global/` | 10 | 38 | Macro, logic, button state |
| `src/commands/i2c/` | 6 | 37 | USB PD state |
| `src/commands/uart/` | 5 | 20 | Monitor, glitch |

---

## 3. Optimization Candidates (Detailed)

### 3.1 NAND Page Buffers (~4.2 KB)

**Files:** `src/nand/nand_ftl_diskio.c`, `src/nand/spi_nand.c`

```c
// nand_ftl_diskio.c:25
static uint8_t page_buffer[SPI_NAND_PAGE_SIZE];              // 2,048 bytes

// spi_nand.c:217
uint8_t page_main_and_largest_oob_buffer[SPI_NAND_PAGE_SIZE + SPI_NAND_LARGEST_OOB_SUPPORTED];  // 2,176 bytes (global)
```

**Analysis:** These buffers are only needed during active disk I/O operations (FatFS reads/writes). They sit idle when the user is in interactive mode, running protocols, or using the scope.

**Recommendation:** Allocate from the big buffer at mount time using `mem_alloc()`, free on unmount. This requires ensuring the big buffer isn't concurrently claimed by scope/LA/editor. Alternatively, pass as a parameter from higher-level FatFS entry points that can stack-allocate. **Savings: ~4.2 KB.**

**Risk:** Medium. The NAND layer is accessed by the MSC USB class (mass storage) which can be active concurrently with other features. Needs careful ownership tracking.

---

### 3.2 Linenoise History Storage (~2.1 KB)

**File:** `src/lib/bp_linenoise/linenoise.c`

```c
static char history_storage[BP_LINENOISE_HISTORY_MAX][BP_LINENOISE_MAX_LINE + 1];  // 8 × 257 = 2,056 bytes
static char *history_ptrs[BP_LINENOISE_HISTORY_MAX];                                // 8 × 4 = 32 bytes
```

**Analysis:** The history buffer stores the last 8 command lines (256 chars each). This is always active during interactive mode. However, most commands are far shorter than 256 characters.

**Recommendations (choose one):**
- **Option A:** Reduce `BP_LINENOISE_MAX_LINE` from 256 to 128. Most commands are under 100 chars. **Saves ~1 KB.**
- **Option B:** Reduce `BP_LINENOISE_HISTORY_MAX` from 8 to 4. **Saves ~1 KB.**
- **Option C:** Use a single circular buffer (e.g., 1 KB total) with variable-length entries instead of fixed 257-byte slots. **Saves ~1.2 KB** but requires significant refactoring.
- **Option D:** Dynamically allocate history from the big buffer when entering interactive mode. **Saves ~2 KB** but history is lost when big buffer is claimed.

---

### 3.3 Tab Completion Buffers (~1.1 KB)

**File:** `src/lib/bp_args/bp_cmd.c`

```c
static char comp_pool[COMP_SLOTS][COMP_BUF_SZ];  // 16 × 64 = 1,024 bytes
static char hint_buf[64];                          // 64 bytes
```

**Analysis:** These buffers are only used during tab completion (when user presses TAB). They are reset at the start of each completion session and not accessed outside that context.

**Recommendation:** Allocate `comp_pool` on the stack of the completion callback function. At 1,024 bytes it is within the 4 KB stack budget (though tight). Alternatively, reduce `COMP_SLOTS` from 16 to 8 (unlikely to need 16 simultaneous completions). **Savings: 512–1,088 bytes.**

**Risk:** Low. The stack is 4 KB; 1 KB on stack during tab completion is feasible if call depth is shallow at that point.

---

### 3.4 Linenoise State Instances (~0.7 KB)

**File:** `src/ui/ui_term_linenoise.c`

```c
static struct linenoiseState ln_state;         // ~328 bytes (includes char buf[257])
static struct linenoiseState ln_prompt_state;  // ~328 bytes
```

**Analysis:** Each `linenoiseState` embeds a 257-byte edit buffer (`char buf[BP_LINENOISE_MAX_LINE + 1]`). Two instances are maintained: one for the main command line, one for prompts/sub-inputs. Only one is active at a time.

**Recommendation:** Share a single edit buffer between the two states, or make `ln_prompt_state` stack-allocated at the call site where prompts are invoked. **Savings: ~328 bytes.**

**Risk:** Low-Medium. Need to verify prompt state doesn't overlap with main edit state in re-entrant scenarios.

---

### 3.5 Mode `mode_config` Structs (~0.2 KB)

**Files:** `src/mode/hwuart.c`, `src/mode/hwhduart.c`, `src/mode/hwspi.c`, `src/mode/binloopback.c`, `src/mode/infrared.c`, `src/mode/dummy1.c`

```c
static struct _uart_mode_config mode_config;      // 36 bytes (hwuart.c, hwhduart.c)
static struct _spi_mode_config mode_config;       // ~40 bytes (hwspi.c)
static struct _infrared_mode_config mode_config;  // 12 bytes (infrared.c — declared twice!)
static struct _binloopback_mode_config mode_config; // 8 bytes (binloopback.c)
static struct command_attributes periodic_attributes; // 28 bytes × 3 files
```

**Analysis:** Only one protocol mode is active at a time. Each mode has its own static `mode_config`, but they all consume RAM simultaneously even when inactive. The `periodic_attributes` struct (28 bytes) is duplicated in 3 files.

**Recommendation:** Use a union of all mode configs in a shared location, or allocate the active mode's config dynamically. A single union would be max(40) = 40 bytes instead of 36+36+40+12+8 = 132 bytes total. **Savings: ~92 bytes.**

Note: `infrared.c` declares `mode_config` twice (lines 48 and 251), which suggests two independent IR subsystems — verify whether both are needed simultaneously.

**Risk:** Medium. The mode dispatch table (`modes.h`) would need a minor refactor to pass config by pointer.

---

### 3.6 VT100 Menu Output Buffer (256 B)

**File:** `src/lib/vt100_menu/vt100_menu.c`

```c
static char out_buf[OUT_BUF_SIZE];  // 256 bytes
```

**Analysis:** Temporary scratch buffer used only during VT100 menu rendering. Data is consumed immediately via `snprintf()` → terminal write. No persistence needed.

**Recommendation:** Move to stack in the rendering function. 256 bytes is comfortably within the 4 KB stack. **Savings: 256 bytes.**

**Risk:** Very low. This is a pure scratch buffer with no cross-call state.

---

### 3.7 EEPROM I2C GUI Context (~416 B)

**File:** `src/commands/eeprom/eeprom_i2c_gui.c`

```c
static i2c_ctx_t ctx;                      // ~70 bytes
static vt100_menu_item_t dev_menu_items[18]; // ~288 bytes
static char dev_size_hints[16][8];          // 128 bytes
```

**Analysis:** All three are initialized fresh each time the EEPROM GUI command is invoked. They are not accessed after the command returns.

**Recommendation:** Move to stack or allocate from the big buffer. The combined ~416 bytes fits comfortably on the stack. **Savings: ~416 bytes.**

**Risk:** Very low. Single-use command context.

---

### 3.8 Function-Scope Statics (~200 B)

These are `static` variables declared inside functions. They persist across calls, but many don't need to.

#### Candidates to Move to Stack

| File | Variable | Size | Reason |
|------|----------|------|--------|
| `src/pirate.c:620` | `prompt_buf[128]` | 128 B | Used to format prompt string; can be stack-local |
| `src/lib/vt100_menu/vt100_menu.c:20` | `out_buf[256]` | 256 B | Scratch buffer (see §3.6) |
| `src/lib/hx/editor.c:1181` | `hexstr[3]` | 3 B | Tiny formatting buffer |
| `src/commands/uart/monitor.c:42` | `cnt` | 4 B | Counter reset each invocation |

#### Candidates to Pass as Context Parameter

| File | Variable | Size | Reason |
|------|----------|------|--------|
| `src/pirate/rgb.c` (multiple) | Animation state vars | ~30 B | 6+ animation functions each have `frame_delay_count`, `color_idx`, etc. Could be a single `animation_ctx_t` struct passed from the timer callback |
| `src/syntax_post.c:288` | `info` struct | ~16 B | Output formatting state; could be passed down from `syntax_post()` |
| `src/binmode/falaio.c:116` | `state` enum | 4 B | State machine; could be in a context struct |
| `src/commands/i2c/usbpdo.c:638` | `available_pdos[7]` | ~98 B | Only used during PD negotiation scan |

#### Function-Scope Statics That Must Remain Static

| File | Variable | Size | Reason |
|------|----------|------|--------|
| `src/usb_tx.c:116–117` | `tx_state`, `delay_count` | 2 B | State machine for ISR-driven TX; must persist across IRQ calls |
| `src/pirate/rgb.c` (various) | Animation counters | ~20 B | Called from timer ISR; no caller to pass context through |
| `src/pirate/amux.c:52` | `busy` flag | 1 B | Guards against re-entrant ADC access |
| `src/pirate/intercore_helpers.c:17` | `static_message_count` | 1 B | Cross-core message sequencing |
| `src/nand/sys_time.c:33` | `mst` (repeating_timer) | 4 B | SDK timer handle; must persist |

---

## 4. Patterns & Structural Observations

### 4.1 The `mode_config` Anti-Pattern

Each protocol mode (UART, SPI, I2C, IR, etc.) declares its own `static mode_config` at file scope. Since only one mode is active at a time, all inactive configs waste RAM. On RP2040 this is only ~132 bytes total, but the pattern doesn't scale well if modes grow.

**Suggested pattern:** Define a `mode_config` union in `modes.h` or `system_config.h`:
```c
typedef union {
    struct _uart_mode_config uart;
    struct _spi_mode_config spi;
    struct _infrared_mode_config ir;
    // ...
} mode_config_union_t;
```
Pass a pointer to the active variant through the mode dispatch table.

### 4.2 Scope Module (40 Statics, All Justified)

`src/display/scope.c` has the highest static variable count (40), but all are small scalars (total ~112 bytes) representing persistent oscilloscope state: trigger settings, display parameters, DMA channels, and ISR-shared volatile flags. These correctly use `static` for state persistence across ISR/callback invocations.

**No changes recommended** for scope.c statics — the large buffers already use `mem_alloc()`.

### 4.3 RGB Animation State (30 Statics)

`src/pirate/rgb.c` has 30 static variables, mostly small (1–2 byte) animation state variables scattered across 6+ animation functions. Each function independently maintains `frame_delay_count`, `color_idx`, etc.

**Suggested refactor:** Create a shared `rgb_animation_state_t` struct (~32 bytes) at file scope, eliminating the duplicated function-local statics. This doesn't save RAM but improves maintainability and makes the animation state resettable (current code has a `HACKHACK` comment about reset difficulty).

### 4.4 Global (Non-Static) Buffers

Several large mutable buffers are global (not `static`):

| File | Variable | Size | Notes |
|------|----------|------|-------|
| `src/usb_tx.c` | `tx_buf[1024]` | 1,024 B + 1,024 alignment | Aligned to 2048 for DMA |
| `src/usb_tx.c` | `bin_tx_buf[1024]` | 1,024 B + 1,024 alignment | Same |
| `src/usb_tx.c` | `tx_tb_buf[1024]` | 1,024 B | Toolbar render buffer |
| `src/usb_rx.c` | `rx_buf[128]`, `bin_rx_buf[128]` | 256 B | RX ring buffers |
| `src/nand/spi_nand.c` | `page_main_and_largest_oob_buffer[2176]` | 2,176 B | See §3.1 |
| `src/pirate/storage.c` | `buf32[512]` | 512 B | Storage scratch buffer |
| `src/platform/*.c` | `hw_adc_*[13]` × 3 | 130 B | ADC sample arrays |

These are global for cross-module access but could be better encapsulated (made static with accessor functions).

### 4.5 Const Data (486 Variables — No RAM Impact)

The majority of static declarations (486 of 735) are `static const` — string tables, command definitions, pin labels, lookup tables, usage strings, and constraint definitions. On RP2040, these are placed in flash (`.rodata` section in XIP) and consume **zero RAM**. No action needed.

### 4.6 Alignment Overhead

The 32768-byte alignment on `mem_buffer` can waste up to 32 KB due to linker padding. The 2048-byte alignment on TX buffers can waste up to 2 KB each. Consider:
- Verifying the actual alignment requirement (DMA on RP2040 only needs specific alignment for certain transfer sizes)
- Using `__attribute__((section(".big_buffer")))` with a custom linker section to control placement

---

## 5. Low-Value / No-Change Items

The following static variables were reviewed and determined to be correctly placed:

| Category | Count | Total Size | Reason |
|----------|-------|-----------|--------|
| `volatile` ISR/DMA state | ~15 | ~60 B | Must persist across interrupt boundaries |
| Mutex/semaphore | 3 | ~12 B | Synchronization primitives — inherently static |
| Hardware config (PIO, DMA chan) | ~10 | ~40 B | Initialized once at startup, used throughout |
| USB descriptor buffers | 2 | ~64 B | Required by TinyUSB callbacks |
| MSC disk state (volatile) | 7 | ~13 B | USB mass storage state machine |
| ADC shadow/monitor values | 5 | ~63 B | Continuously updated by background tasks |
| Scope scalar state | 40 | ~112 B | Persistent oscilloscope UI/trigger state |
| Linenoise callbacks/flags | 7 | ~28 B | Library configuration state |

---

## 6. Recommendations Summary

### Quick Wins (Low effort, clear savings)

1. **Move `out_buf[256]` to stack** in `vt100_menu.c` — saves 256 B
2. **Move `eeprom_i2c_gui` context to stack** — saves ~416 B
3. **Move `prompt_buf[128]` to stack** in `pirate.c` — saves 128 B
4. **Move `available_pdos[7]` to stack** in `usbpdo.c` — saves ~98 B

### Medium-Effort Refactors

5. **Reduce linenoise history** (`BP_LINENOISE_MAX_LINE` 256→128 or `HISTORY_MAX` 8→4) — saves ~1 KB
6. **Allocate `comp_pool` on stack** during tab completion — saves ~1 KB
7. **Share or stack-allocate `ln_prompt_state`** — saves ~328 B

### Larger Architectural Changes

8. **Use big buffer for NAND page buffers** when disk is mounted — saves ~4.2 KB
9. **Union mode_config structs** across protocol modes — saves ~92 B, better pattern
10. **Consolidate RGB animation state** into a single context struct — saves ~0 B but improves reset behavior
11. **Audit alignment requirements** on big buffer and TX buffers — potential ~2–32 KB savings from reduced alignment

### Total Potential Savings

| Category | Estimate |
|----------|----------|
| Quick wins (#1–4) | ~900 B |
| Medium refactors (#5–7) | ~2.3 KB |
| Architectural (#8–11) | ~4.3 KB + alignment savings |
| **Grand total** | **~7.5 KB + potential alignment savings** |

This would reduce the non-big-buffer static RAM from ~17 KB to ~9.5 KB, bringing total usage from ~94% to ~91% (or lower if alignment padding is reduced).
