# Architecture Smell Audit — Bus Pirate 5/6/7 Firmware

**Date:** 2026-02-20
**Scope:** Full codebase audit — all findings (previously identified + new)
**Target:** RP2040 (264KB RAM, Cortex-M0+) / RP2350 (512KB RAM, Cortex-M33)

---

## Previously Identified Findings

These issues were identified in earlier audit passes and are included here for completeness.

### P1. `system_config` God-Struct Shared Between Cores Without Synchronization

**Severity:** Critical
**Files:** `src/system_config.h`

The monolithic `system_config` struct (130+ fields) is accessed from both Core 0 (UI/commands) and Core 1 (USB/LCD/RGB) with no mutex, spinlock, or memory barriers. Fields like `pin_changed`, `terminal_usb_enable`, and display state are written by Core 0 and read by Core 1 concurrently.

**Impact:** Torn reads on multi-byte fields, stale cached values due to missing barriers, and undefined behavior from data races. On Cortex-M0+ (no hardware cache coherency), writes may not be visible across cores without explicit `__dmb()`.

**Status:** Open — requires incremental refactoring to split into per-core or synchronized sections.

---

### P2. 194KB+ Static Buffer Allocation (73% of RP2040 RAM)

**Severity:** Critical
**Files:** `src/pirate/mem.c`, `src/syntax_compile.c`, various

~194KB of the RP2040's 264KB RAM is statically allocated: 128KB big buffer (`mem.c`), ~56KB syntax buffers, plus global state. Leaves ~70KB for stack, TinyUSB, and runtime allocations.

**Impact:** No headroom for new features. Stack overflow risk on deep call chains (USB callbacks + FatFS + NAND). Any new static allocation can push the firmware past RP2040 limits.

**Status:** Open — architectural constraint, not easily fixable without redesign.

---

### P3. `lcd_update_request` Written From ISR Without `volatile`

**Severity:** Critical
**Files:** `src/pirate.c`

The `lcd_update_request` flag is written from a timer interrupt callback (`lcd_timer_callback`) and read from the Core 1 main loop. It is declared as a plain `bool`, not `volatile`.

**Impact:** The compiler may optimize away the read in the main loop, caching the value in a register. The LCD could stop updating or miss update requests entirely.

**Status:** Open.

---

### P4. `mem_alloc` Not Thread-Safe (Plain `bool` Flag)

**Severity:** Critical
**Files:** `src/pirate/mem.c:17, 21–35`

The `allocated` flag is a plain `bool` with no synchronization. The check-then-set pattern (`if (!allocated) { allocated = true; }`) is not atomic. Both cores could simultaneously see `allocated == false` and both proceed to "allocate" the buffer.

**Impact:** Double allocation of the single 128KB big buffer — both owners corrupt each other's data.

**Status:** Open.

---

### P5. Four Incompatible Error-Handling Mechanisms

**Severity:** Medium
**Files:** Various

The codebase uses four different error reporting patterns: return codes, `printf` to terminal, `assert_error()` system halt, and silent failure. There's no unified error propagation model.

**Impact:** Inconsistent user experience and difficult debugging. Some errors hang the system while equivalent errors in other paths are silently ignored.

**Status:** Open — requires codebase-wide policy decision.

---

### P6. Platform Headers Duplicating Enums and `static const` Arrays

**Severity:** Medium
**Files:** `src/platform/bpi5-rev9.h`, `src/platform/bpi5-rev10.h`, `src/platform/bpi6-rev2.h`, etc.

Each platform header redeclares the same enums and `static const` arrays (pin mappings, ADC channels, color palettes). Changes must be propagated manually across all headers.

**Impact:** Maintenance burden and copy-paste divergence. Adding a new board requires duplicating hundreds of lines.

**Status:** Open.

---

### P7. `pirate.h` as God-Header Triggering Full Rebuilds

**Severity:** Medium
**Files:** `src/pirate.h`

Nearly every source file includes `pirate.h`, which transitively includes platform headers, system config, and hardware definitions. Any change to `pirate.h` or its transitive includes triggers a full rebuild.

**Impact:** Slow iteration during development. On CI, a one-line change to a platform constant rebuilds the entire firmware.

**Status:** Open.

---

### P8. 700 Lines of Boilerplate in `modes.c` (Fixable With Defaults Macro)

**Severity:** Medium
**Files:** `src/modes.c`

Each mode registration block repeats ~25 function pointer assignments. Most modes only override a few callbacks; the rest are identical defaults.

**Impact:** Adding a new mode requires copying ~25 lines of boilerplate. Easy to miss one pointer, causing a NULL dereference crash.

**Recommended fix:** A `MODE_DEFAULTS()` macro that fills the struct with safe defaults, then modes only override what they need.

**Status:** Open.

---

### P9. CMake Flat File List With No Library Targets

**Severity:** Medium
**Files:** `CMakeLists.txt`

All source files are listed in a single flat `add_executable()` call with no intermediate library targets. There's no modularity in the build graph.

**Impact:** No incremental build benefit from module boundaries. Cannot build/test subsystems independently.

**Status:** Open.

---

### P10. Inconsistent Naming Across Mode Implementations

**Severity:** Low
**Files:** `src/mode/*.c`

Mode function naming varies: some use `mode_setup()`, others `mode_setup_exc()`. Some modes prefix with the mode name (`spi_write`), others don't. No consistent convention.

**Impact:** Makes code navigation and refactoring harder. Grep for patterns is unreliable.

**Status:** Open.

---

### P11. `BP_PENDANTIC` Typo

**Severity:** Low
**Files:** `src/pirate.h`

The macro `BP_PENDANTIC` should be `BP_PEDANTIC`. Typo propagated through conditional compilation guards.

**Status:** Open — trivial rename.

---

### P12. Unstable Mode Enum IDs Across Builds With Different Feature Flags

**Severity:** Medium
**Files:** `src/modes.h`, `src/modes.c`

Mode enum values are assigned sequentially and shift when `BP_USE_*` feature flags change. A build with `BP_USE_HW1WIRE` disabled produces different IDs for all subsequent modes.

**Impact:** Binary mode clients that use numeric mode IDs will break when firmware is rebuilt with different feature flags. Saved configuration referencing mode IDs may load the wrong mode.

**Status:** Open.

---

### P13. `reserve_for_future_mode_specific_allocations` Wasting 10KB

**Severity:** Low
**Files:** `src/system_config.h`

A 10KB reserved array in `system_config` exists for future mode-specific allocations. It's never used.

**Impact:** Wastes 10KB of the 264KB RP2040 RAM budget (3.8%).

**Status:** Open — can be removed when needed.

---

### P14–P16. Well-Designed Subsystems (No Action Needed)

- **P14. SPSC queues** (`src/spsc_queue.h`): Correct lock-free implementation with proper barriers. ✅
- **P15. Syntax pipeline** (`src/syntax_compile.c`, `src/syntax_run.c`, `src/syntax_post.c`): Clean three-phase architecture. ✅
- **P16. Mode dispatch table** (`src/modes.h`, `src/modes.c`): Function-pointer polymorphism is correct. ✅

---

## New Critical Findings

### 1. PSU Current-Limit DAC Never Written — Comparison Instead of Assignment

**Severity:** Critical
**Files:** `src/pirate/psu.c:156–157` (before fix)
**Status:** ✅ Fixed in this audit

The current-limit DAC values used comparison (`==`) instead of assignment (`=`):

```c
// BEFORE (broken):
dac[0] == (i_dac >> 8) & 0xF;
dac[1] == i_dac & 0xFF;

// AFTER (fixed):
dac[0] = (i_dac >> 8) & 0xF;
dac[1] = i_dac & 0xFF;
```

**Impact:** The current DAC received stale voltage values instead of current-limit values. On Bus Pirate 6 (I2C DAC platform), the overcurrent protection was effectively non-functional — the device could source unlimited current into a short circuit, risking damage to the target and the Bus Pirate itself.

**Effort:** 1 minute (fix applied).

---

### 2. NAND FTL Disk I/O — Mutex Deadlock on Error Paths

**Severity:** Critical
**Files:** `src/nand/nand_ftl_diskio.c:81, 102, 152` (before fix)
**Status:** ✅ Fixed in this audit

Three functions acquired `diskio_mutex` but returned `RES_ERROR` without releasing it:

- `diskio_read()` — line 81
- `diskio_write()` — line 102
- `diskio_ioctl(CTRL_TRIM)` — line 152

**Impact:** Any NAND read/write/trim error permanently deadlocks the filesystem. Since the mutex is blocking (`mutex_enter_blocking`), every subsequent FatFS operation hangs forever. On a device that stores configuration to NAND, a single bad block triggers a hard lockup requiring power cycle.

**Fix:** Added `mutex_exit(&diskio_mutex)` before each error return.

**Effort:** 5 minutes (fix applied).

---

### 3. FatFS Mutex Timeout Uses Error Code Instead of Timeout Value

**Severity:** Critical
**Files:** `src/fatfs/ffsystem.c:135` (before fix)
**Status:** ✅ Fixed in this audit

```c
// BEFORE (broken):
return mutex_enter_timeout_ms(sobj, FR_TIMEOUT);  // FR_TIMEOUT = 15 (enum value)

// AFTER (fixed):
return mutex_enter_timeout_ms(sobj, FF_FS_TIMEOUT);  // FF_FS_TIMEOUT = 1000 (ms)
```

**Impact:** `FR_TIMEOUT` is a FatFS result code enum (value 15), not a duration. The mutex timeout was 15ms instead of the configured 1000ms. Under any contention between cores accessing the filesystem, operations would spuriously fail with `FR_TIMEOUT` errors — manifesting as intermittent file save/load failures.

**Effort:** 1 minute (fix applied).

---

### 4. JTAG Mode Releases Wrong Pins on Cleanup

**Severity:** Critical
**Files:** `src/mode/jtag.c:48–58` (before fix)
**Status:** ✅ Fixed in this audit

Setup claimed `BIO2, BIO3, M_SPI_CLK, M_SPI_CDO, M_SPI_CDI, M_SPI_CS`.
Cleanup released `BIO0, BIO1, BIO2, BIO3, M_SPI_CLK, M_SPI_CDO, M_SPI_CDI, M_SPI_CS`.

**Impact:** BIO0 and BIO1 were erroneously released (never claimed). BIO2 and BIO3 were correctly released but the extra releases could corrupt pin state tracking for other modes. After exiting JTAG mode, BIO0/BIO1 pin labels and function states could be inconsistent.

**Fix:** Removed BIO0/BIO1 releases to match the setup claims exactly.

**Effort:** 2 minutes (fix applied).

---

### 5. I2S Mode Leaks Three Pins and Running PIO State Machines on Cleanup

**Severity:** Critical
**Files:** `src/mode/i2s.c:215–226` (before fix)
**Status:** ✅ Fixed in this audit

Setup claimed 6 pins: `BIO0, BIO1, BIO2, BIO5, BIO6, BIO7`.
Cleanup only released 3: `BIO0, BIO1, BIO2`.

Additionally, PIO state machines were not disabled before removing programs.

**Impact:** After exiting I2S mode, BIO5/BIO6/BIO7 remain claimed — the user cannot use them for other protocols or commands until power cycle. The running PIO state machines continue consuming PIO resources and could interfere with the next mode.

**Fix:** Added releases for BIO5/BIO6/BIO7 and `pio_sm_set_enabled(..., false)` before program removal.

**Effort:** 5 minutes (fix applied).

---

### 6. PIO State Machines Not Disabled Before Program Removal (UART, I2C, 1-Wire)

**Severity:** Critical
**Files:** `src/pirate/hwuart_pio.c:83–88`, `src/pirate/hwi2c_pio.c:43–46`, `src/pirate/hw1wire_pio.c:47–50` (before fix)
**Status:** ✅ Fixed in this audit

All three PIO-based protocol drivers removed PIO programs without first disabling the state machine.

**Impact:** A running state machine continues executing instructions from freed program memory. The PIO instruction memory can be reallocated to a different program, causing the old SM to execute random instructions — resulting in unpredictable GPIO behavior, potential electrical damage to connected devices, or PIO lockup.

**Fix:** Added `pio_sm_set_enabled(pio, sm, false)` before each `pio_remove_program()` call.

**Effort:** 5 minutes (fix applied).

---

## Medium Findings

### 7. `bio.c` — No Bounds Checking on Pin Index Parameters

**Severity:** Medium
**Files:** `src/pirate/bio.c:78, 92, 104, 116, 131, 147, 156, 168`

All public `bio_*` functions accept a `uint8_t bio` parameter indexing into `bio2bufiopin[]` and `bio2bufdirpin[]` arrays (both size 8, `BIO_MAX_PINS`). None validate `bio < BIO_MAX_PINS`.

**Impact:** A caller passing `bio >= 8` reads/writes out-of-bounds memory, which on Cortex-M0+ (no MMU) silently corrupts adjacent globals or accesses invalid GPIO registers. Currently all callers appear to pass valid indices, but there is no defense against future mistakes.

**Recommended fix:**
```c
static inline bool bio_valid(uint8_t bio) {
    return bio < BIO_MAX_PINS;
}
// Add assert(bio_valid(bio)) at top of each function
```

**Effort:** 30 minutes.

---

### 8. `rgb.c` — Shared `pixels[]` Array Accessed From Timer ISR and Core 0 Without Synchronization

**Severity:** Medium
**Files:** `src/pirate/rgb.c:189, 346, 390–396`

The global `CPIXEL_COLOR pixels[COUNT_OF_PIXELS]` array is:
- Written by Core 0 user commands (`rgb_set_all`, `rgb_set_array`, `rgb_put` via `assign_pixel_color`)
- Read by `update_pixels()` called from `pixel_timer_callback()` (timer interrupt on Core 1)

No mutex, volatile, or memory barriers protect this access.

**Impact:** Torn reads — the timer can observe a partially-written pixel color (e.g., red component from new color, green from old). The `CPIXEL_COLOR` struct is likely 4 bytes; on Cortex-M0+ only 32-bit aligned word access is atomic, so a struct copy may not be atomic. Visual glitches (single-frame color artifacts) are the most likely symptom.

**Recommended fix:** Use `__dmb()` memory barriers around writes in `assign_pixel_color` and reads in `update_pixels`, or use a double-buffer with an atomic swap flag.

**Effort:** 1 hour.

---

### 9. `intercore_helpers.c` — Synchronous Message Send Has No Timeout

**Severity:** Medium
**Files:** `src/pirate/intercore_helpers.c:44–52`

```c
do {
    uint32_t response = multicore_fifo_pop_blocking();
    assert(response == raw_msg.raw);
    if (response == raw_msg.raw) {
        return;
    }
} while (1);
```

`multicore_fifo_pop_blocking()` blocks forever if Core 1 crashes or doesn't echo the message. The `assert` on line 48 halts in debug but is stripped in release builds, making the `if` check on line 49 always true after a successful pop — the `while(1)` is unreachable dead code.

**Impact:** If Core 1 is stuck (e.g., in a TinyUSB callback), Core 0 hangs permanently. No watchdog or timeout can recover from this state.

**Recommended fix:** Use `multicore_fifo_pop_timeout_us()` with a reasonable timeout (e.g., 100ms) and return an error code to the caller.

**Effort:** 30 minutes.

---

### 10. `pirate.c` — Core 1 Init Wait Has No Timeout

**Severity:** Medium
**Files:** `src/pirate.c:706–717`

Core 1's entry function polls in a `do-while(1)` loop waiting for `BP_ICM_INIT_CORE1` from Core 0. If Core 0 crashes or takes too long, Core 1 spins forever.

**Impact:** Recovery from a Core 0 crash during initialization is impossible without power cycle.

**Recommended fix:** Add a timeout (e.g., 5 seconds) with a watchdog reset fallback.

**Effort:** 30 minutes.

---

### 11. `psu.c` — No Input Validation on Voltage/Current Setpoints

**Severity:** Medium
**Files:** `src/pirate/psu.c:101–131`

`psu_set_v()` and `psu_set_i()` accept arbitrary float values without range validation. While `vset > PWM_TOP` is capped, there's no check for negative values or values below the minimum (0.8V for voltage).

**Impact:** Passing a negative voltage or zero current limit could produce undefined DAC/PWM values. On the PWM platform, a wrapped-around unsigned value could set maximum voltage.

**Recommended fix:** Clamp inputs to valid ranges (0.8–5.0V, 0–500mA) at the top of each function.

**Effort:** 15 minutes.

---

### 12. `storage.c` — Write Operations Ignore Return Values

**Severity:** Medium
**Files:** `src/pirate/storage.c:169–172, 207–218`

`f_write()` return values and bytes-written counts are not checked in `storage_save_binary_blob_rollover()`. Similarly, `f_printf()` calls in `storage_save_mode()` have no error checking.

**Impact:** Silent data corruption — if a write fails (disk full, NAND error), the function reports success. Configuration saves can silently produce truncated/corrupt files.

**Recommended fix:** Check `f_write()` return codes and `bw` values; propagate errors to callers.

**Effort:** 30 minutes.

---

### 13. `pico-i2c-sniff` Library — Non-Atomic FIFO Counters in Dual-Core Context

**Severity:** Medium
**Files:** `src/lib/pico-i2c-sniff/ram_fifo.c:10–13, 29–42`

The `capture_count`, `capture_set`, and `capture_get` variables are plain `uint32_t` without `volatile` or memory barriers. The I2C sniffer uses Core 1 to produce data and Core 0 to consume it.

**Impact:** The compiler may optimize reads/writes of these counters, causing missed updates between cores. On Cortex-M0+, 32-bit aligned reads are atomic, but without memory barriers the store buffer can delay visibility — leading to missed captures or overcounted data.

**Recommended fix:** Mark counters as `volatile` and add `__dmb()` barriers after writes and before reads.

**Effort:** 30 minutes.

---

## Low Findings

### 14. `bpio.c` — COBS In-Place Decode With Shared Input/Output Buffer

**Severity:** Low
**Files:** `src/binmode/bpio.c:840`

```c
cobs_decode(buf, len, buf, sizeof(buf), &decoded_len);
```

COBS decoding is done in-place (same source and destination buffer). While COBS decoded output is always ≤ encoded input, this pattern is fragile — any future change to the encoding or buffer management could silently introduce overflow.

**Impact:** Currently safe due to COBS properties, but a maintenance hazard.

**Effort:** 15 minutes to use separate decode buffer (at cost of ~640 bytes stack).

---

### 15. `bpio.c` — No ACK/NAK for Partial USB Packets

**Severity:** Low
**Files:** `src/binmode/bpio.c:803–833`

The binary protocol read loop waits for a COBS delimiter (0x00) with a 500ms timeout. If a partial packet arrives and times out, it's silently discarded. The client has no way to know if a request was received.

**Impact:** Unreliable communication over lossy USB connections. The client must implement its own retry logic.

**Effort:** Hours (protocol change).

---

### 16. `debug_rtt.h` — Well-Designed, Zero-Cost When Disabled

**Severity:** None (informational)
**Files:** `src/debug_rtt.h`

The debug system uses `static inline` with `__attribute__((always_inline))` functions that compile to nothing when debug is disabled. The `BP_DEBUG_PRINT` macro uses `do { if(...) } while(0)` pattern. No side effects in release builds. **No action needed.**

---

## What's Good

These patterns are well-designed and should be preserved:

- **SPSC queues** (`src/spsc_queue.h`): Textbook lock-free single-producer/single-consumer implementation with correct `volatile` usage and `__dmb()` memory barriers. Proper power-of-2 bitmask wrapping.
- **Syntax pipeline** (`src/syntax_compile.c`, `src/syntax_run.c`, `src/syntax_post.c`): Clean three-phase compiler architecture. The `goto` usage in `syntax_compile.c` is safe — all error paths are handled correctly.
- **Mode dispatch table** (`src/modes.h`, `src/modes.c`): Function-pointer polymorphism pattern is correct and efficient for embedded.
- **Display abstraction** (`src/displays.c`): Clean dispatch table with independent setup/cleanup/update callbacks. Properly decoupled from the mode system.
- **FatFS mutex implementation** (`src/fatfs/ffsystem.c`): Uses Pico SDK mutex correctly for dual-core safety (after the timeout constant fix).
- **Debug system** (`src/debug_rtt.h`): Zero-cost when disabled via proper inlining and dead-code elimination.
- **`snprintf` usage in `pirate.c`**: All prompt buffer formatting uses `snprintf` with `sizeof(prompt_buf)` — no buffer overflows.

---

## Priority Table

Ranked by (impact × likelihood / effort). Items marked ✅ have been fixed in this audit.

### Previously Identified Findings

| # | Finding | Severity | Impact | Status |
|---|---------|----------|--------|--------|
| P1 | `system_config` god-struct cross-core races | Critical | Data corruption, undefined behavior | Open |
| P2 | 194KB+ static allocation (73% RAM) | Critical | No headroom, stack overflow risk | Open |
| P3 | `lcd_update_request` not `volatile` (ISR) | Critical | LCD stops updating | Open |
| P4 | `mem_alloc` not thread-safe | Critical | Double allocation, data corruption | Open |
| P5 | Four incompatible error-handling mechanisms | Medium | Inconsistent behavior | Open |
| P6 | Platform headers duplicate enums/arrays | Medium | Maintenance burden | Open |
| P7 | `pirate.h` god-header full rebuilds | Medium | Slow builds | Open |
| P8 | 700 lines boilerplate in `modes.c` | Medium | Copy-paste bugs | Open |
| P9 | CMake flat file list | Medium | No modularity | Open |
| P10 | Inconsistent mode naming | Low | Navigation difficulty | Open |
| P11 | `BP_PENDANTIC` typo | Low | Cosmetic | Open |
| P12 | Unstable mode enum IDs | Medium | Config/binary protocol breakage | Open |
| P13 | 10KB reserved array wasted | Low | 3.8% RAM waste | Open |

### New Findings From This Audit

| # | Finding | Severity | Impact | Effort | Status |
|---|---------|----------|--------|--------|--------|
| 1 | PSU current DAC never written (`==` vs `=`) | Critical | Electrical damage risk | 1 min | ✅ Fixed |
| 2 | NAND diskio mutex deadlock on error | Critical | System hang on NAND error | 5 min | ✅ Fixed |
| 3 | FatFS timeout uses enum not ms | Critical | Intermittent FS failures | 1 min | ✅ Fixed |
| 6 | PIO SMs not disabled before cleanup | Critical | Random GPIO, PIO lockup | 5 min | ✅ Fixed |
| 4 | JTAG releases wrong pins | Critical | Pin state corruption | 2 min | ✅ Fixed |
| 5 | I2S leaks pins and PIO SMs | Critical | Resource leak, PIO lockup | 5 min | ✅ Fixed |
| 7 | `bio.c` no bounds checks | Medium | Memory corruption on bad input | 30 min | Open |
| 8 | `rgb.c` shared pixels array race | Medium | Visual glitches | 1 hr | Open |
| 9 | Intercore sync no timeout | Medium | Permanent hang | 30 min | Open |
| 10 | Core 1 init no timeout | Medium | Unrecoverable boot hang | 30 min | Open |
| 11 | PSU no input validation | Medium | Undefined voltage/current | 15 min | Open |
| 12 | `storage.c` unchecked writes | Medium | Silent data corruption | 30 min | Open |
| 13 | `ram_fifo.c` non-atomic counters | Medium | Missed I2C captures | 30 min | Open |
| 14 | `bpio.c` in-place COBS decode | Low | Maintenance hazard | 15 min | Open |
| 15 | `bpio.c` no ACK/NAK | Low | Unreliable binary protocol | Hours | Open |
