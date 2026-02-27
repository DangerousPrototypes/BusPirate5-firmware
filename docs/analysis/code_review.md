# Bus Pirate 5 Firmware Code Review

**Date:** January 31, 2026  
**Reviewer:** GitHub Copilot  
**Codebase:** Bus Pirate 5/6/7 Firmware (RP2040/RP2350)

---

## Table of Contents
1. [Bugs and Potential Issues](#1-bugs-and-potential-issues)
   - [Critical Issues](#critical-issues)
   - [Memory Issues](#memory-issues)
   - [Logic Issues](#logic-issues)
2. [Refactoring Recommendations](#2-refactoring-recommendations)
3. [Open Source Library Recommendations](#3-open-source-library-recommendations)
4. [Summary Priority List](#summary-priority-list)

---

## 1. Bugs and Potential Issues

### Critical Issues

#### Format String Vulnerabilities
Several places pass user-controlled data directly to `printf()`:

| File | Issue |
|------|-------|
| `src/pirate/file.c` | File content used as format string |
| `src/lib/minmea/minmea.c` | NMEA data as format string |
| `src/commands/i2c/ddr4.c` | I2C data as format string |

**Fix:** Replace `printf(var)` with `printf("%s", var)`

#### Disabled Spinlocks in Queue (Race Conditions)
In `src/queue.c` line 15:
```c
#define DISABLE_SPINLOCK 1
```

This disables thread safety for `rx_fifo`, `tx_fifo`, `bin_rx_fifo`, and `bin_tx_fifo` which are shared between cores. This can cause:
- Lost data
- Corrupted pointers
- Buffer overflows

**Recommendation:** Re-enable spinlocks or implement a proper lock-free SPSC (Single Producer Single Consumer) queue.

#### Blocking Call in ISR
In `src/usb_rx.c` (around line 97) the UART interrupt handler uses blocking queue operations:
```c
void rx_uart_irq_handler(void) {
    // ...
    while (uart_is_readable(...)) {
        uint8_t c = uart_getc(...);
        queue2_add_blocking(&rx_fifo, &c); // BUGBUG -- blocking call from ISR!
    }
}
```

**Fix:** Use `queue2_try_add()` instead and handle the case where the queue is full.

#### ADC Busy-Wait Race Condition
In `src/pirate/amux.c` lines 49-65:
```c
void adc_busy_wait(bool enable) {
    static bool busy = false;  // NOT volatile!

    if (!enable) {
        busy = false;  // Race: another core could be in the do-while
        return;
    }
    // ...
}
```

**Issues:**
1. The `busy` variable is not `volatile`
2. Setting `busy = false` happens outside the spinlock, creating a TOCTOU race

**Fix:**
```c
void adc_busy_wait(bool enable) {
    static volatile bool busy = false;

    if (!enable) {
        spin_lock_unsafe_blocking(adc_spin_lock);
        busy = false;
        spin_unlock_unsafe(adc_spin_lock);
        return;
    }
    // ...
}
```

#### Missing `volatile` on Shared Variables

| Variable | File | Issue |
|----------|------|-------|
| `lcd_update_request` | `src/pirate/lcd.c` | Written by Core0, read by Core1 |
| `lcd_update_force` | `src/pirate/lcd.c` | Written by Core0, read by Core1 |
| `tx_sb_buf_ready` | `src/usb_tx.c` | Shared between functions |
| `tx_sb_buf_cnt` | `src/usb_tx.c` | Shared between functions |
| `tx_sb_buf_index` | `src/usb_tx.c` | Shared between functions |

---

### Memory Issues

#### Buffer Overflows (Unsafe String Operations)

| File | Line | Issue |
|------|------|-------|
| `src/commands/i2c/ddr5.c` | ~53 | `strcpy(protocol_name_upper, ...)` no bounds check on 32-byte buffer |
| `src/pirate/hw1wire_pio.c` | ~1968-1969 | `strcpy`/`strcat` without bounds checking |
| `src/pirate/hw1wire_pio.c` | ~1843 | `strcpy(cp, "uS")` no buffer verification |
| Multiple files | Various | `sprintf()` calls without buffer size validation |

**Fix:** Replace with bounds-checked alternatives:
```c
// Instead of:
strcpy(dest, src);
strcat(dest, src2);

// Use:
snprintf(dest, sizeof(dest), "%s%s", src, src2);
```

#### Missing NULL Checks After malloc

| File | Line | Issue |
|------|------|-------|
| `src/binmode/sigrok.c` | ~896 | `capture_buf = malloc()` with no NULL check |
| `src/binmode/fala.c` | ~16 | malloc result not properly validated |

**Fix:** Always check malloc return value:
```c
capture_buf = malloc(size);
if (capture_buf == NULL) {
    // Handle error appropriately
    return ERROR_OUT_OF_MEMORY;
}
```

#### Memory Leaks

| File | Issue |
|------|-------|
| `src/binmode/sigrok.c` | `malloc()` for `capture_buf` with no corresponding `free()` |
| `src/binmode/fala.c` | Similar pattern |

#### Stack Overflow Risks
Large local arrays on stack in embedded context:

| File | Size | Variable |
|------|------|----------|
| Multiple | 512 bytes | `char json[512]` |
| irtoy files | 512 bytes | `uint8_t air_buffer[512]` |
| `src/binmode/bpio.c` | 264 bytes | `uint8_t buf[256+4+4]` |

**Recommendation:** Consider static allocation or heap allocation for large buffers in embedded systems with limited stack.

---

### Logic Issues

#### Underflow in ram_fifo
In `src/binmode/fala.c` lines 49-52:
```c
uint32_t ram_fifo_get(void) {
    capture_count--;  // Decrements before checking if buffer is empty!
    // ...
}
```

#### Syntax Compiler Control Flow
`src/syntax.c` line 121 uses `goto` for control flow:
```c
goto compiler_get_attributes;
// ...
goto compile_get_string;
```

While functional, this is error-prone and hard to maintain. Consider refactoring to use functions or structured control flow.

#### Integer Overflow Risks

| File | Issue |
|------|-------|
| `src/display/lcd.c` | Array index calculation with multiplication could overflow |
| `src/pirate/hw2wire_pio.c` | `uint8_t msg_len = 2 + data_len` could overflow if data_len > 253 |

---

## 2. Refactoring Recommendations

### Command Line Parser Architecture
The current parser in `src/ui/ui_cmdln.c` (606+ lines) and `src/ui/ui_parse.c` (387+ lines) is complex with:
- Duplicated parsing logic for hex/dec/bin in multiple places
- Manual pointer arithmetic for circular buffer
- Complex state management spread across files

**Recommendation:** Create a unified tokenizer/lexer module that:
1. Separates tokenization from interpretation
2. Provides a clean iterator API over tokens
3. Centralizes number format detection

### Syntax Module Refactoring
`src/syntax.c` (772 lines) handles compilation, execution, and output formatting.

**Recommendation:** Split into:
- `syntax_compile.c` - Bytecode generation
- `syntax_execute.c` - Runtime execution  
- `syntax_format.c` - Output formatting

### Duplicated String Functions
Functions like `ui_parse_get_hex()`, `cmdln_args_get_hex()`, etc. are nearly identical.

**Recommendation:** Consolidate into a single string-to-number module.

### Error Handling
Currently using ad-hoc error reporting with raw `printf()`.

**Recommendation:** Create a centralized error reporting system:
```c
typedef struct {
    uint16_t code;
    const char* message;
    uint32_t context;
} bp_error_t;

void bp_error_report(bp_error_t* error);
```

### Pin Function Tracking
The `system_config.pin_func[]` tracking is scattered across the codebase.

**Recommendation:** Centralize into a pin manager module with clear state transitions.

### Large System Config Structure
`src/system_config.h` has a 130+ field monolithic structure.

**Recommendation:** Split into logical groups:
```c
typedef struct {
    terminal_config_t terminal;
    hardware_config_t hardware;
    mode_config_t mode;
    psu_config_t psu;
    display_config_t display;
} system_config_t;
```

---

## 3. Open Source Library Recommendations

### Terminal/CLI Replacement

#### **linenoise** (BSD-2-Clause) ⭐ Highly Recommended
- **GitHub:** [antirez/linenoise](https://github.com/antirez/linenoise)
- ~1100 lines of code
- Line editing with history
- Tab completion
- Hints/suggestions
- UTF-8 support
- VT100 compatible
- Async/multiplexing mode
- Used by Redis, MongoDB

This could replace `ui_cmdln.c` + parts of `ui_parse.c`, providing:
- Better history handling
- Tab completion for commands
- Cleaner API

### Command Argument Parsing

#### **ketopt** (MIT) from klib
- Single header file
- getopt-style argument parsing
- Much cleaner than manual flag parsing in `cmdln_args_find_flag_*()` functions

### Data Structures

#### **klib** (MIT) ⭐ Highly Recommended
- **GitHub:** [attractivechaos/klib](https://github.com/attractivechaos/klib)

| Component | Use Case |
|-----------|----------|
| `khash.h` | Generic hash table (faster than custom implementations) |
| `kvec.h` | Generic dynamic arrays |
| `kstring.h` | Safe string operations (replace unsafe `strcpy`/`strcat`) |
| `ksort.h` | Generic sorting |

### Ring Buffer/Queue

#### Lock-free SPSC Queue
Your disabled spinlock queue is concerning. Consider a proper lock-free single-producer single-consumer queue designed for dual-core:
- Atomic read/write pointers
- Memory barriers for ARM
- No locking needed for SPSC pattern

### JSON Parsing
You already have **mjson** (MIT) - good choice! It's lightweight and suitable for embedded.

Alternative if you need more features:
- **cJSON** (MIT) - More feature-complete for bidirectional JSON

### Libraries You're Already Using Well
- **printf-4.0.0** - Good embedded printf implementation
- **flatcc** - Excellent choice for binary serialization
- **nanocobs** (Unlicense/0BSD) - Good choice for COBS packet framing
- **mjson** (MIT) - Lightweight JSON parser

---

## Summary Priority List

### 🔴 Immediate Fixes (Critical)
- [ ] Fix format string vulnerabilities (`printf(var)` → `printf("%s", var)`)
- [ ] Re-enable spinlocks in `queue.c` OR implement proper SPSC queue
- [ ] Fix ADC `busy_wait` race condition
- [ ] Replace blocking call in UART ISR with non-blocking
- [ ] Add `volatile` to shared inter-core variables

### 🟠 High Priority Refactoring
- [ ] Replace `strcpy`/`strcat` with bounds-checked alternatives (kstring or snprintf)
- [ ] Add NULL checks after all malloc calls
- [ ] Split `syntax.c` into smaller modules
- [ ] Consolidate duplicated number parsing functions

### 🟡 Medium Priority Improvements
- [ ] Implement centralized error handling
- [ ] Split `system_config` into logical sub-structures
- [ ] Create pin manager module
- [ ] Review and fix potential integer overflows

### 🟢 Library Replacements to Consider
- [ ] **linenoise** for command line - would greatly improve UX
- [ ] **klib/kstring** for safe string operations
- [ ] **ketopt** for cleaner argument parsing
- [ ] Proper lock-free SPSC queue for inter-core communication

---

## Conclusion

The codebase is generally well-structured for an embedded project of this size. The main concerns are:

1. **Concurrency issues** - disabled spinlocks, shared variables without volatile
2. **Format string vulnerabilities** - security risk
3. **Buffer safety** - strcpy/strcat without bounds checking

The terminal parsing is functional but could benefit from a library like **linenoise** for a much better user experience with less code to maintain.

The good practices observed include:
- Core affinity assertions (`BP_ASSERT_CORE0`, `BP_ASSERT_CORE1`)
- Use of FlatBuffers for binary protocol
- COBS encoding for packet framing
- Structured mode system with clean interfaces
