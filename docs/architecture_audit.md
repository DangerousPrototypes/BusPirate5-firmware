# Architecture Smell Audit — Bus Pirate 5/6/7 Firmware

**Date:** 2026-02-20
**Scope:** Full codebase audit excluding 16 previously documented findings
**Target:** RP2040 (264KB RAM, Cortex-M0+) / RP2350 (512KB RAM, Cortex-M33)

---

## Critical Findings

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
