# Architecture Smell Audit — Bus Pirate 5/6/7 Firmware

**Date:** 2026-02-20
**Scope:** Full codebase audit — all findings (previously identified + new)
**Target:** RP2040 (264KB RAM, Cortex-M0+) / RP2350 (512KB RAM, Cortex-M33)

> All file links point to this branch. Line numbers are approximate — use them as a starting point.

---

## Critical — Fixed in This PR ✅

### 1. PSU Current-Limit DAC Never Written — Comparison Instead of Assignment

**Files:** [`src/pirate/psu.c:156–157`](https://github.com/DangerousPrototypes/BusPirate5-firmware/blob/copilot/audit-bus-pirate-firmware/src/pirate/psu.c#L156-L157)
**Status:** ✅ Fixed

The current-limit DAC values used comparison (`==`) instead of assignment (`=`):

```c
// BEFORE (broken):
dac[0] == (i_dac >> 8) & 0xF;
dac[1] == i_dac & 0xFF;

// AFTER (fixed):
dac[0] = (i_dac >> 8) & 0xF;
dac[1] = i_dac & 0xFF;
```

**Impact:** On Bus Pirate 6 (I2C DAC platform), the overcurrent protection was non-functional — the device could source unlimited current into a short circuit, risking damage to target and Bus Pirate.

---

### 2. NAND FTL Disk I/O — Mutex Deadlock on Error Paths

**Files:** [`src/nand/nand_ftl_diskio.c:81`](https://github.com/DangerousPrototypes/BusPirate5-firmware/blob/copilot/audit-bus-pirate-firmware/src/nand/nand_ftl_diskio.c#L81), [`L102`](https://github.com/DangerousPrototypes/BusPirate5-firmware/blob/copilot/audit-bus-pirate-firmware/src/nand/nand_ftl_diskio.c#L102), [`L152`](https://github.com/DangerousPrototypes/BusPirate5-firmware/blob/copilot/audit-bus-pirate-firmware/src/nand/nand_ftl_diskio.c#L152)
**Status:** ✅ Fixed

Three functions acquired `diskio_mutex` but returned `RES_ERROR` without releasing it:

- `diskio_read()` — line 81
- `diskio_write()` — line 102
- `diskio_ioctl(CTRL_TRIM)` — line 152

**Impact:** Any NAND read/write/trim error permanently deadlocks the filesystem. A single bad block triggers a hard lockup requiring power cycle.

**Fix:** Added `mutex_exit(&diskio_mutex)` before each error return.

---

### 3. FatFS Mutex Timeout Uses Error Code Instead of Timeout Value

**Files:** [`src/fatfs/ffsystem.c:135`](https://github.com/DangerousPrototypes/BusPirate5-firmware/blob/copilot/audit-bus-pirate-firmware/src/fatfs/ffsystem.c#L135)
**Status:** ✅ Fixed

```c
// BEFORE: FR_TIMEOUT = 15 (enum value, not milliseconds)
return mutex_enter_timeout_ms(sobj, FR_TIMEOUT);
// AFTER: FF_FS_TIMEOUT = 1000 (ms)
return mutex_enter_timeout_ms(sobj, FF_FS_TIMEOUT);
```

**Impact:** Mutex timed out in 15ms instead of 1000ms. Under any contention between cores, FS operations would spuriously fail.

---

### 4. JTAG Mode Releases Wrong Pins on Cleanup

**Files:** [`src/mode/jtag.c:48–58`](https://github.com/DangerousPrototypes/BusPirate5-firmware/blob/copilot/audit-bus-pirate-firmware/src/mode/jtag.c#L48-L58)
**Status:** ✅ Fixed

Setup claims `BIO2, BIO3` + SPI pins. Cleanup released `BIO0, BIO1` instead — corrupting pin state tracking for unclaimed pins.

**Impact:** After exiting JTAG mode, BIO0/BIO1 labels and function states become inconsistent.

**Fix:** Removed BIO0/BIO1 releases to match setup claims.

---

### 5. I2S Mode Leaks Three Pins and Running PIO State Machines on Cleanup

**Files:** [`src/mode/i2s.c:215–226`](https://github.com/DangerousPrototypes/BusPirate5-firmware/blob/copilot/audit-bus-pirate-firmware/src/mode/i2s.c#L215-L226)
**Status:** ✅ Fixed

Setup claims 6 pins (BIO0-2, BIO5-7). Cleanup only released 3 (BIO0-2). PIO state machines not disabled before program removal.

**Impact:** BIO5/6/7 remain claimed until power cycle. Running PIO SMs could interfere with the next mode.

**Fix:** Added BIO5/6/7 releases and `pio_sm_set_enabled(..., false)` before program removal.

---

### 6. PIO State Machines Not Disabled Before Program Removal (UART, I2C, 1-Wire)

**Files:** [`src/pirate/hwuart_pio.c:83–88`](https://github.com/DangerousPrototypes/BusPirate5-firmware/blob/copilot/audit-bus-pirate-firmware/src/pirate/hwuart_pio.c#L83-L88), [`hwi2c_pio.c:43–46`](https://github.com/DangerousPrototypes/BusPirate5-firmware/blob/copilot/audit-bus-pirate-firmware/src/pirate/hwi2c_pio.c#L43-L46), [`hw1wire_pio.c:47–50`](https://github.com/DangerousPrototypes/BusPirate5-firmware/blob/copilot/audit-bus-pirate-firmware/src/pirate/hw1wire_pio.c#L47-L50)
**Status:** ✅ Fixed

All three PIO drivers called `pio_remove_program()` while SM was still running.

**Impact:** SM continues executing freed instruction memory → random GPIO behavior, potential electrical damage, PIO lockup.

**Fix:** Added `pio_sm_set_enabled(pio, sm, false)` before each `pio_remove_program()`.

---

## Critical — Open

### P1. `system_config` God-Struct Shared Between Cores Without Synchronization

**Files:** [`src/system_config.h`](https://github.com/DangerousPrototypes/BusPirate5-firmware/blob/copilot/audit-bus-pirate-firmware/src/system_config.h)

The monolithic `system_config` struct (130+ fields) is accessed from both Core 0 (UI/commands) and Core 1 (USB/LCD/RGB) with no mutex, spinlock, or memory barriers.

**Impact:** Torn reads on multi-byte fields, stale cached values, undefined behavior from data races. On Cortex-M0+ writes may not be visible across cores without `__dmb()`.

**How to fix:** Split into per-core structs or add `__dmb()` barriers around cross-core fields. Long-term: refactor into Core 0 / Core 1 / shared-with-mutex sections.

---

### P2. 194KB+ Static Buffer Allocation (73% of RP2040 RAM)

**Files:** [`src/pirate/mem.c`](https://github.com/DangerousPrototypes/BusPirate5-firmware/blob/copilot/audit-bus-pirate-firmware/src/pirate/mem.c), [`src/syntax_compile.c`](https://github.com/DangerousPrototypes/BusPirate5-firmware/blob/copilot/audit-bus-pirate-firmware/src/syntax_compile.c)

~194KB statically allocated: 128KB big buffer, ~56KB syntax buffers, plus globals. Leaves ~70KB for stack + TinyUSB + runtime.

**Impact:** No headroom for new features. Stack overflow risk on deep call chains.

**How to fix:** Consider sharing big buffer and syntax buffers (not used simultaneously). Effort: days.

---

### P3. `lcd_update_request` Written From ISR Without `volatile`

**Files:** [`src/pirate.c`](https://github.com/DangerousPrototypes/BusPirate5-firmware/blob/copilot/audit-bus-pirate-firmware/src/pirate.c)

Written from timer ISR, read from Core 1 loop. Declared as plain `bool`, not `volatile`.

**Impact:** Compiler may cache the value in a register — LCD could stop updating entirely.

**How to fix:** `static volatile bool lcd_update_request;`

---

### P4. `mem_alloc` Not Thread-Safe (Plain `bool` Flag)

**Files:** [`src/pirate/mem.c:17`](https://github.com/DangerousPrototypes/BusPirate5-firmware/blob/copilot/audit-bus-pirate-firmware/src/pirate/mem.c#L17), [`L21–35`](https://github.com/DangerousPrototypes/BusPirate5-firmware/blob/copilot/audit-bus-pirate-firmware/src/pirate/mem.c#L21-L35)

Plain `bool allocated` with non-atomic check-then-set. Both cores could simultaneously allocate the 128KB buffer.

**Impact:** Double allocation — both owners corrupt each other's data.

**How to fix:** Use Pico SDK `mutex_t` or `spin_lock` around the allocation check. 15 minutes.

---

## Medium — Open

### P5. Four Incompatible Error-Handling Mechanisms

**Files:** Various

Return codes, `printf` to terminal, `assert_error()` halt, and silent failure coexist.

**Impact:** Some errors hang the system while equivalent errors elsewhere are silently ignored.

**How to fix:** Define a codebase error policy and migrate incrementally. Effort: days.

---

### P6. Platform Headers Duplicating Enums and `static const` Arrays

**Files:** [`src/platform/bpi5-rev9.h`](https://github.com/DangerousPrototypes/BusPirate5-firmware/blob/copilot/audit-bus-pirate-firmware/src/platform/bpi5-rev9.h), [`bpi5-rev10.h`](https://github.com/DangerousPrototypes/BusPirate5-firmware/blob/copilot/audit-bus-pirate-firmware/src/platform/bpi5-rev10.h), [`bpi6-rev2.h`](https://github.com/DangerousPrototypes/BusPirate5-firmware/blob/copilot/audit-bus-pirate-firmware/src/platform/bpi6-rev2.h)

Each board header redeclares the same enums and arrays. Changes must be propagated manually.

**How to fix:** Extract common enums/arrays into `platform_common.h` with per-board overrides.

---

### P7. `pirate.h` as God-Header Triggering Full Rebuilds

**Files:** [`src/pirate.h`](https://github.com/DangerousPrototypes/BusPirate5-firmware/blob/copilot/audit-bus-pirate-firmware/src/pirate.h)

Nearly every source file includes it. Any change triggers a full rebuild.

**How to fix:** Break into focused headers (`pirate_types.h`, `pirate_hw.h`, etc.).

---

### P8. 700 Lines of Boilerplate in `modes.c`

**Files:** [`src/modes.c`](https://github.com/DangerousPrototypes/BusPirate5-firmware/blob/copilot/audit-bus-pirate-firmware/src/modes.c)

Each mode repeats ~25 function pointer assignments. Easy to miss one → NULL dereference crash.

**How to fix:** `MODE_DEFAULTS()` macro that fills safe defaults; modes only override what they need. 1 hour.

---

### P9. CMake Flat File List With No Library Targets

**Files:** [`CMakeLists.txt`](https://github.com/DangerousPrototypes/BusPirate5-firmware/blob/copilot/audit-bus-pirate-firmware/CMakeLists.txt)

All source files in one `add_executable()`. No modularity.

**How to fix:** Create `add_library()` targets for subsystems (pirate, modes, binmode, fatfs).

---

### P12. Unstable Mode Enum IDs Across Builds

**Files:** [`src/modes.h`](https://github.com/DangerousPrototypes/BusPirate5-firmware/blob/copilot/audit-bus-pirate-firmware/src/modes.h), [`src/modes.c`](https://github.com/DangerousPrototypes/BusPirate5-firmware/blob/copilot/audit-bus-pirate-firmware/src/modes.c)

Mode IDs shift when `BP_USE_*` feature flags change. Breaks binary protocol clients and saved config.

**How to fix:** Assign fixed enum values: `M_HW1WIRE = 1, M_HWUART = 2, ...` regardless of feature flags.

---

### 7. `bio.c` — No Bounds Checking on Pin Index Parameters

**Files:** [`src/pirate/bio.c:78`](https://github.com/DangerousPrototypes/BusPirate5-firmware/blob/copilot/audit-bus-pirate-firmware/src/pirate/bio.c#L78), [`L92`](https://github.com/DangerousPrototypes/BusPirate5-firmware/blob/copilot/audit-bus-pirate-firmware/src/pirate/bio.c#L92), [`L104`](https://github.com/DangerousPrototypes/BusPirate5-firmware/blob/copilot/audit-bus-pirate-firmware/src/pirate/bio.c#L104), [`L116`](https://github.com/DangerousPrototypes/BusPirate5-firmware/blob/copilot/audit-bus-pirate-firmware/src/pirate/bio.c#L116), [`L131`](https://github.com/DangerousPrototypes/BusPirate5-firmware/blob/copilot/audit-bus-pirate-firmware/src/pirate/bio.c#L131), [`L147`](https://github.com/DangerousPrototypes/BusPirate5-firmware/blob/copilot/audit-bus-pirate-firmware/src/pirate/bio.c#L147), [`L156`](https://github.com/DangerousPrototypes/BusPirate5-firmware/blob/copilot/audit-bus-pirate-firmware/src/pirate/bio.c#L156), [`L168`](https://github.com/DangerousPrototypes/BusPirate5-firmware/blob/copilot/audit-bus-pirate-firmware/src/pirate/bio.c#L168)

All `bio_*` functions accept `uint8_t bio` index without validating `bio < BIO_MAX_PINS` (8). Out-of-bounds corrupts memory.

**How to fix:** Add `assert(bio < BIO_MAX_PINS)` at top of each function. 30 min.

---

### 8. `rgb.c` — Shared `pixels[]` Array Race Between Core 0 and Timer ISR

**Files:** [`src/pirate/rgb.c:189`](https://github.com/DangerousPrototypes/BusPirate5-firmware/blob/copilot/audit-bus-pirate-firmware/src/pirate/rgb.c#L189), [`L346`](https://github.com/DangerousPrototypes/BusPirate5-firmware/blob/copilot/audit-bus-pirate-firmware/src/pirate/rgb.c#L346), [`L390–396`](https://github.com/DangerousPrototypes/BusPirate5-firmware/blob/copilot/audit-bus-pirate-firmware/src/pirate/rgb.c#L390-L396)

`pixels[]` written by Core 0, read by timer ISR on Core 1. No synchronization → torn reads, visual glitches.

**How to fix:** Add `__dmb()` barriers around writes/reads, or double-buffer with atomic swap. 1 hour.

---

### 9. `intercore_helpers.c` — Synchronous Message Send Has No Timeout

**Files:** [`src/pirate/intercore_helpers.c:44–52`](https://github.com/DangerousPrototypes/BusPirate5-firmware/blob/copilot/audit-bus-pirate-firmware/src/pirate/intercore_helpers.c#L44-L52)

`multicore_fifo_pop_blocking()` blocks forever if Core 1 is stuck. No recovery possible.

**How to fix:** Use `multicore_fifo_pop_timeout_us()` with 100ms timeout and return error. 30 min.

---

### 10. `pirate.c` — Core 1 Init Wait Has No Timeout

**Files:** [`src/pirate.c:706–717`](https://github.com/DangerousPrototypes/BusPirate5-firmware/blob/copilot/audit-bus-pirate-firmware/src/pirate.c#L706-L717)

Core 1 polls forever for init message from Core 0. No recovery without power cycle.

**How to fix:** Add 5-second timeout with watchdog reset fallback. 30 min.

---

### 11. `psu.c` — No Input Validation on Voltage/Current Setpoints

**Files:** [`src/pirate/psu.c:101–131`](https://github.com/DangerousPrototypes/BusPirate5-firmware/blob/copilot/audit-bus-pirate-firmware/src/pirate/psu.c#L101-L131)

`psu_set_v()`/`psu_set_i()` accept arbitrary floats. Negative values wrap to max voltage/current.

**How to fix:** Clamp inputs: 0.8–5.0V, 0–500mA at function entry. 15 min.

---

### 12. `storage.c` — Write Operations Ignore Return Values

**Files:** [`src/pirate/storage.c:169–172`](https://github.com/DangerousPrototypes/BusPirate5-firmware/blob/copilot/audit-bus-pirate-firmware/src/pirate/storage.c#L169-L172), [`L207–218`](https://github.com/DangerousPrototypes/BusPirate5-firmware/blob/copilot/audit-bus-pirate-firmware/src/pirate/storage.c#L207-L218)

`f_write()` return values not checked. Configuration saves can silently corrupt.

**How to fix:** Check return codes, verify `bw == expected`, propagate errors. 30 min.

---

### 13. `ram_fifo.c` — Non-Atomic FIFO Counters in Dual-Core Context

**Files:** [`src/lib/pico-i2c-sniff/ram_fifo.c:10–13`](https://github.com/DangerousPrototypes/BusPirate5-firmware/blob/copilot/audit-bus-pirate-firmware/src/lib/pico-i2c-sniff/ram_fifo.c#L10-L13), [`L29–42`](https://github.com/DangerousPrototypes/BusPirate5-firmware/blob/copilot/audit-bus-pirate-firmware/src/lib/pico-i2c-sniff/ram_fifo.c#L29-L42)

FIFO counters plain `uint32_t` without `volatile` or barriers. Producer (Core 1) / consumer (Core 0) may see stale values.

**How to fix:** Mark as `volatile`, add `__dmb()` after writes and before reads. 30 min.

---

## Low — Open

### P10. Inconsistent Naming Across Mode Implementations

**Files:** [`src/mode/`](https://github.com/DangerousPrototypes/BusPirate5-firmware/tree/copilot/audit-bus-pirate-firmware/src/mode)

Function naming varies: `mode_setup()` vs `mode_setup_exc()`, some prefix with mode name, others don't.

**How to fix:** Adopt consistent naming convention. Effort: hours.

---

### P11. `BP_PENDANTIC` Typo

**Files:** [`src/pirate.h`](https://github.com/DangerousPrototypes/BusPirate5-firmware/blob/copilot/audit-bus-pirate-firmware/src/pirate.h)

Should be `BP_PEDANTIC`. Typo in conditional compilation guards.

**How to fix:** Rename macro. 5 min.

---

### P13. `reserve_for_future_mode_specific_allocations` Wasting 10KB

**Files:** [`src/system_config.h`](https://github.com/DangerousPrototypes/BusPirate5-firmware/blob/copilot/audit-bus-pirate-firmware/src/system_config.h)

10KB reserved array never used. Wastes 3.8% of RP2040 RAM.

**How to fix:** Remove the array. 5 min.

---

### 14. `bpio.c` — COBS In-Place Decode With Shared Buffer

**Files:** [`src/binmode/bpio.c:840`](https://github.com/DangerousPrototypes/BusPirate5-firmware/blob/copilot/audit-bus-pirate-firmware/src/binmode/bpio.c#L840)

COBS decode uses same buffer for source and destination. Currently safe but fragile.

**How to fix:** Use separate decode buffer (~640 bytes stack). 15 min.

---

### 15. `bpio.c` — No ACK/NAK for Partial USB Packets

**Files:** [`src/binmode/bpio.c:803–833`](https://github.com/DangerousPrototypes/BusPirate5-firmware/blob/copilot/audit-bus-pirate-firmware/src/binmode/bpio.c#L803-L833)

Partial packets silently discarded after 500ms timeout. Client has no feedback.

**How to fix:** Protocol-level change (ACK/NAK framing). Effort: hours.

---

## Well-Designed Subsystems ✅

These patterns are correct and should be preserved:

- **SPSC queues** ([`src/spsc_queue.h`](https://github.com/DangerousPrototypes/BusPirate5-firmware/blob/copilot/audit-bus-pirate-firmware/src/spsc_queue.h)): Lock-free with correct `volatile` and `__dmb()` barriers.
- **Syntax pipeline** ([`src/syntax_compile.c`](https://github.com/DangerousPrototypes/BusPirate5-firmware/blob/copilot/audit-bus-pirate-firmware/src/syntax_compile.c), [`syntax_run.c`](https://github.com/DangerousPrototypes/BusPirate5-firmware/blob/copilot/audit-bus-pirate-firmware/src/syntax_run.c), [`syntax_post.c`](https://github.com/DangerousPrototypes/BusPirate5-firmware/blob/copilot/audit-bus-pirate-firmware/src/syntax_post.c)): Clean three-phase architecture.
- **Mode dispatch table** ([`src/modes.h`](https://github.com/DangerousPrototypes/BusPirate5-firmware/blob/copilot/audit-bus-pirate-firmware/src/modes.h), [`modes.c`](https://github.com/DangerousPrototypes/BusPirate5-firmware/blob/copilot/audit-bus-pirate-firmware/src/modes.c)): Function-pointer polymorphism, correct and efficient.
- **Display abstraction** ([`src/displays.c`](https://github.com/DangerousPrototypes/BusPirate5-firmware/blob/copilot/audit-bus-pirate-firmware/src/displays.c)): Clean dispatch table, properly decoupled.
- **FatFS mutex** ([`src/fatfs/ffsystem.c`](https://github.com/DangerousPrototypes/BusPirate5-firmware/blob/copilot/audit-bus-pirate-firmware/src/fatfs/ffsystem.c)): Uses Pico SDK mutex correctly (after timeout fix).
- **Debug system** ([`src/debug_rtt.h`](https://github.com/DangerousPrototypes/BusPirate5-firmware/blob/copilot/audit-bus-pirate-firmware/src/debug_rtt.h)): Zero-cost when disabled.

---

## Priority Table

Ranked by (impact × likelihood / effort). ✅ = fixed in this PR.

| # | Finding | Severity | Impact | Status |
|---|---------|----------|--------|--------|
| 1 | [PSU DAC `==` vs `=`](https://github.com/DangerousPrototypes/BusPirate5-firmware/blob/copilot/audit-bus-pirate-firmware/src/pirate/psu.c#L156-L157) | **Critical** | Electrical damage | ✅ Fixed |
| 2 | [NAND mutex deadlock](https://github.com/DangerousPrototypes/BusPirate5-firmware/blob/copilot/audit-bus-pirate-firmware/src/nand/nand_ftl_diskio.c#L81) | **Critical** | System hang | ✅ Fixed |
| 3 | [FatFS timeout enum](https://github.com/DangerousPrototypes/BusPirate5-firmware/blob/copilot/audit-bus-pirate-firmware/src/fatfs/ffsystem.c#L135) | **Critical** | FS failures | ✅ Fixed |
| 4 | [JTAG wrong pins](https://github.com/DangerousPrototypes/BusPirate5-firmware/blob/copilot/audit-bus-pirate-firmware/src/mode/jtag.c#L48-L58) | **Critical** | Pin corruption | ✅ Fixed |
| 5 | [I2S pin leak + PIO](https://github.com/DangerousPrototypes/BusPirate5-firmware/blob/copilot/audit-bus-pirate-firmware/src/mode/i2s.c#L215-L226) | **Critical** | Resource leak | ✅ Fixed |
| 6 | [PIO SM not disabled](https://github.com/DangerousPrototypes/BusPirate5-firmware/blob/copilot/audit-bus-pirate-firmware/src/pirate/hwuart_pio.c#L83-L88) | **Critical** | GPIO damage | ✅ Fixed |
| P1 | [`system_config` races](https://github.com/DangerousPrototypes/BusPirate5-firmware/blob/copilot/audit-bus-pirate-firmware/src/system_config.h) | **Critical** | Data corruption | Open |
| P2 | [194KB static alloc](https://github.com/DangerousPrototypes/BusPirate5-firmware/blob/copilot/audit-bus-pirate-firmware/src/pirate/mem.c) | **Critical** | Stack overflow | Open |
| P3 | [`lcd_update_request` volatile](https://github.com/DangerousPrototypes/BusPirate5-firmware/blob/copilot/audit-bus-pirate-firmware/src/pirate.c) | **Critical** | LCD hangs | Open |
| P4 | [`mem_alloc` thread-safety](https://github.com/DangerousPrototypes/BusPirate5-firmware/blob/copilot/audit-bus-pirate-firmware/src/pirate/mem.c#L17) | **Critical** | Double alloc | Open |
| 7 | [`bio.c` bounds](https://github.com/DangerousPrototypes/BusPirate5-firmware/blob/copilot/audit-bus-pirate-firmware/src/pirate/bio.c#L78) | **Medium** | Memory corrupt | Open |
| 8 | [`rgb.c` pixel race](https://github.com/DangerousPrototypes/BusPirate5-firmware/blob/copilot/audit-bus-pirate-firmware/src/pirate/rgb.c#L189) | **Medium** | Visual glitch | Open |
| 9 | [Intercore no timeout](https://github.com/DangerousPrototypes/BusPirate5-firmware/blob/copilot/audit-bus-pirate-firmware/src/pirate/intercore_helpers.c#L44-L52) | **Medium** | Permanent hang | Open |
| 10 | [Core 1 init timeout](https://github.com/DangerousPrototypes/BusPirate5-firmware/blob/copilot/audit-bus-pirate-firmware/src/pirate.c#L706-L717) | **Medium** | Boot hang | Open |
| 11 | [PSU input validation](https://github.com/DangerousPrototypes/BusPirate5-firmware/blob/copilot/audit-bus-pirate-firmware/src/pirate/psu.c#L101-L131) | **Medium** | Bad voltage | Open |
| 12 | [`storage.c` writes](https://github.com/DangerousPrototypes/BusPirate5-firmware/blob/copilot/audit-bus-pirate-firmware/src/pirate/storage.c#L169-L172) | **Medium** | Silent corrupt | Open |
| 13 | [`ram_fifo.c` atomics](https://github.com/DangerousPrototypes/BusPirate5-firmware/blob/copilot/audit-bus-pirate-firmware/src/lib/pico-i2c-sniff/ram_fifo.c#L10-L13) | **Medium** | Missed data | Open |
| P5 | [Error handling](https://github.com/DangerousPrototypes/BusPirate5-firmware/tree/copilot/audit-bus-pirate-firmware/src) | **Medium** | Inconsistent | Open |
| P6 | [Platform headers](https://github.com/DangerousPrototypes/BusPirate5-firmware/tree/copilot/audit-bus-pirate-firmware/src/platform) | **Medium** | Maintenance | Open |
| P7 | [`pirate.h` god-header](https://github.com/DangerousPrototypes/BusPirate5-firmware/blob/copilot/audit-bus-pirate-firmware/src/pirate.h) | **Medium** | Slow builds | Open |
| P8 | [`modes.c` boilerplate](https://github.com/DangerousPrototypes/BusPirate5-firmware/blob/copilot/audit-bus-pirate-firmware/src/modes.c) | **Medium** | Copy-paste bugs | Open |
| P9 | [CMake flat list](https://github.com/DangerousPrototypes/BusPirate5-firmware/blob/copilot/audit-bus-pirate-firmware/CMakeLists.txt) | **Medium** | No modularity | Open |
| P12 | [Mode enum IDs](https://github.com/DangerousPrototypes/BusPirate5-firmware/blob/copilot/audit-bus-pirate-firmware/src/modes.h) | **Medium** | Protocol break | Open |
| 14 | [COBS in-place](https://github.com/DangerousPrototypes/BusPirate5-firmware/blob/copilot/audit-bus-pirate-firmware/src/binmode/bpio.c#L840) | **Low** | Fragile | Open |
| 15 | [No ACK/NAK](https://github.com/DangerousPrototypes/BusPirate5-firmware/blob/copilot/audit-bus-pirate-firmware/src/binmode/bpio.c#L803-L833) | **Low** | Unreliable | Open |
| P10 | [Naming inconsistency](https://github.com/DangerousPrototypes/BusPirate5-firmware/tree/copilot/audit-bus-pirate-firmware/src/mode) | **Low** | Navigation | Open |
| P11 | [`BP_PENDANTIC` typo](https://github.com/DangerousPrototypes/BusPirate5-firmware/blob/copilot/audit-bus-pirate-firmware/src/pirate.h) | **Low** | Cosmetic | Open |
| P13 | [10KB wasted](https://github.com/DangerousPrototypes/BusPirate5-firmware/blob/copilot/audit-bus-pirate-firmware/src/system_config.h) | **Low** | RAM waste | Open |
