# Dead Code & Rarely-Used Code Analysis Report

## Executive Summary

| Metric | Count |
|--------|-------|
| Dead functions found | ~8 confirmed unreferenced non-static functions |
| Orphaned files (not compiled) | 7 mode files + 1 sigrok file + ~15 test/example files |
| Deprecated folder files (fully isolated) | 12 files (~4,000 lines) |
| `#if 0` disabled code blocks | ~37 blocks in non-third-party code (~1,850–2,200 lines) |
| Dead/orphaned headers | 2 confirmed (background_image_v4-orig.h, robot16.h) |
| Legacy API call sites remaining | ~80+ across modes and commands |
| Commands migrated to bp_cmd system | ~87 commands |
| Estimated lines safely removable | ~7,500–8,500 lines |

**Key risk areas:**
- `src/deprecated/` is fully isolated — safe to delete entirely
- 7 orphaned mode files (`SW3W.c`, `SW2W.c`, `SWI2C.c`, `LCDSPI.c`, `ST7735.c`, `HD44780.c`, `usbpd.c`) are not compiled
- Large `#if 0` blocks in `hw1wire_pio.c` (~400+ lines), `flat.c`, `ddr4.c`, `usbpdo.c`
- Legacy prompt/cmdln API has ~80+ remaining call sites blocking full migration

---

## 1. Dead Code

### 1.1 Unreferenced Functions

| File | Function | Static? | Notes |
|------|----------|---------|-------|
| `src/pirate/bio.c` | `bio_buf_test()` | No | Declared in header, defined in .c, never called anywhere |
| `src/debug_uart.c` | `rx_uart_disable()` | No | Empty function body, never called |
| `src/ui/ui_prompt.c` | `ui_prompt_menu_int()` | No | Declared in header but no external callers found |
| `src/ui/ui_prompt.c` | `ui_prompt_prompt_int()` | No | Declared in header but no external callers found |
| `src/ui/ui_cmdln.c` | `cmdln_consume_white_space()` | No (should be static) | Only called within ui_cmdln.c — not truly dead but should be static |
| `src/ui/ui_cmdln.c` | `cmdln_args_get_string()` | No (should be static) | Only called within ui_cmdln.c |
| `src/ui/ui_cmdln.c` | `cmdln_args_get_hex()` | No (should be static) | Only called within ui_cmdln.c |
| `src/ui/ui_cmdln.c` | `cmdln_args_get_bin()` | No (should be static) | Only called within ui_cmdln.c |
| `src/ui/ui_cmdln.c` | `cmdln_args_get_dec()` | No (should be static) | Only called within ui_cmdln.c |
| `src/ui/ui_cmdln.c` | `cmdln_args_find_flag_internal()` | No (should be static) | Only called within ui_cmdln.c |

**Note:** The 6 `ui_cmdln.c` functions are used internally within that file but have unnecessarily broad linkage. They should be marked `static` for encapsulation, though they are not truly dead.

### 1.2 Orphaned Declarations

| Header | Declaration | Status |
|--------|-------------|--------|
| `src/pirate/bio.h` | `bio_buf_test()` | Declared but function never called — dead declaration |
| `src/ui/ui_prompt.h` | `ui_prompt_menu_int()` | Declared but never called externally |
| `src/ui/ui_prompt.h` | `ui_prompt_prompt_int()` | Declared but never called externally |
| `src/ui/ui_prompt.h` | `ui_prompt_validate_int()` | Declared, thin wrapper only used internally |

### 1.3 Disabled Code Blocks (`#if 0` / commented out)

**37 `#if 0` blocks found in non-third-party code:**

| File | Approx Lines | Description | Recommendation |
|------|-------------|-------------|----------------|
| `src/pirate/hw1wire_pio.c` | ~400 (6 blocks) | ROM search test, DS18B20 demo, `onewire_temp_app()` temperature monitoring — all moved to `commands/1wire/` | **Remove** — functionality migrated |
| `src/commands/global/flat.c` | ~130 (4 blocks) | FlatBuffer test/debug functions (`read_flat`, `read_modes`, builder tests) | **Remove** — development test code |
| `src/commands/i2c/ddr4.c` | ~50 (4 blocks) | Polling idle status, DDR5 SPD reading logic (wrong file) | **Remove** — vestigial/misplaced code |
| `src/commands/i2c/usbpdo.c` | ~90 (3 blocks) | Alternative `fusb_read_source_capabilities()`, PDO request error handling | **Remove** — replaced by active implementations |
| `src/commands/2wire/sle4442.c` | ~30 (2 blocks) | Disabled SLE4442 smartcard operations | Review — may be WIP |
| `src/commands/eeprom/eeprom_i2c.c` | ~20 (2 blocks) | Disabled EEPROM I2C code paths | Review |
| `src/commands/i2c/i2c.c` | ~20 (2 blocks) | Disabled I2C helper code | Review |
| `src/mode/dio.c` | ~20 (2 blocks) | Disabled DIO mode code | Review |
| `src/ui/ui_process.c` | ~15 (2 blocks) | Disabled UI processing code | Review |
| `src/system_config.c` | ~5 (1 block) | Small disabled block | Review |
| `src/system_config.h` | ~5 (1 block) | Small disabled block | Review |
| `src/pirate/shift.c` | ~10 (1 block) | Disabled shift register code | Review |
| `src/pirate/pullup.c` | ~5 (1 block) | Disabled pull-up code | Review |
| `src/pirate/irio_pio.c` | ~5 (1 block) | Disabled IR I/O code | Review |
| `src/mode/hw3wire.c` | ~10 (1 block) | Disabled 3-wire mode code | Review |
| `src/mode/hwspi.c` | ~10 (1 block) | Disabled SPI mode code | Review |
| `src/mode/hw2wire.c` | ~10 (1 block) | Disabled 2-wire mode code | Review |
| `src/json_parser.c` | ~5 (1 block) | Disabled JSON parser code | Review |
| `src/commands/i2c/usbpd.c` | ~10 (1 block) | Disabled USB PD code | Review |
| `src/commands/i2c/ddr5.c` | ~10 (1 block) | Disabled DDR5 code | Review |
| `src/commands/i2c/sniff.c` | ~10 (1 block) | Disabled I2C sniff code | Review |
| `src/commands/i2s/sine.c` | ~10 (1 block) | Disabled I2S sine wave code | Review |
| `src/commands/global/macro.c` | ~10 (1 block) | Disabled macro code | Review |
| `src/commands/global/hex.c` | ~10 (1 block) | Disabled hex viewer code | Review |
| `src/commands/eeprom/eeprom_1wire.c` | ~10 (1 block) | Disabled 1-wire EEPROM code | Review |
| `src/binmode/irtoy-air.c` | ~10 (1 block) | Disabled IR toy air code | Review |

### 1.4 Orphaned Files

| File | In CMakeLists? | Included/Referenced? | Recommendation |
|------|----------------|---------------------|----------------|
| `src/mode/SW3W.c` (436 lines) | No | Referenced in modes.c conditionally | **Orphaned mode** — not compiled, likely superseded by hw3wire |
| `src/mode/SW2W.c` (237 lines) | No | Referenced in modes.c conditionally | **Orphaned mode** — not compiled, likely superseded by hw2wire |
| `src/mode/SWI2C.c` (212 lines) | No | Referenced in modes.c conditionally | **Orphaned mode** — not compiled, likely superseded by hwi2c |
| `src/mode/LCDSPI.c` (96 lines) | No | Referenced in modes.c conditionally | **Orphaned mode** — disabled feature (BP_USE_LCDSPI commented out) |
| `src/mode/ST7735.c` (315 lines) | No | Referenced by LCDSPI.c | **Orphaned** — dependency of orphaned LCDSPI |
| `src/mode/HD44780.c` (194 lines) | No | Referenced by LCDSPI.c | **Orphaned** — dependency of orphaned LCDSPI |
| `src/mode/usbpd.c` (82 lines) | No | Referenced in modes.c conditionally | **Orphaned mode** — disabled (BP_USE_USBPD commented out) |
| `src/lib/sigrok/pico_sdk_sigrok.c` (1,387 lines) | No | Only in own header | **Orphaned** — sigrok integration never compiled |
| `src/display/background_image_v4-orig.h` | N/A | Never included | **Orphaned header** — superseded asset |
| `src/display/robot16.h` | N/A | Only in commented-out #include | **Orphaned header** — commented-out include in pirate.c |
| `src/deprecated/*.c` (7 files) | No | Only internal cross-references | **Expected** — deprecated folder (see Section 3) |

**Not flagged (test/example files in third-party libs):**
- `src/lib/minmea/example.c`, `tests.c` — library test files
- `src/lib/rtt/Examples/*` — RTT example code
- `src/lib/sfud/inc/sfud_port-stm32.c`, `app.c` — wrong platform / example
- `src/lib/bp_expr/bp_expr_test.c` — expression library test
- `src/lib/ap33772s/main.c` — library example
- `src/lib/arduino-ch32v003-swio/main.c` — library example
- `src/lib/nanocobs/tests/*` — nanocobs test files

### 1.5 Dead Translation Keys

**No dead translation keys found.** All 624 translation key enum values defined in `src/translation/base.h` are referenced in `.c` files outside of `src/translation/`. The translation system appears well-maintained.

---

## 2. Rarely-Used Code

### 2.1 Single-Caller Functions

| File | Function | Single Caller Location | Inline Candidate? |
|------|----------|----------------------|-------------------|
| `src/pirate/lcd.c` | `lcd_reset()` | `src/pirate.c` → `pirate_startup()` | No — hardware init, keep separate |
| `src/pirate/hw3wire_pio.c` | `pio_hw3wire_cleanup()` | `src/mode/hw3wire.c` → cleanup | No — mode dispatch pattern |
| `src/pirate/hw2wire_pio.c` | `pio_hw2wire_cleanup()` | `src/mode/hw2wire.c` → cleanup | No — mode dispatch pattern |
| `src/pirate/hwuart_pio.c` | `hwuart_pio_deinit()` | `src/mode/hwhduart.c` → cleanup | No — mode dispatch pattern |
| `src/pirate/hw1wire_pio.c` | `onewire_cleanup()` | `src/mode/hw1wire.c` → cleanup | No — mode dispatch pattern |
| `src/pirate/hwi2c_pio.c` | `pio_i2c_cleanup()` | `src/mode/hwi2c.c` → cleanup | No — mode dispatch pattern |
| `src/pirate/rc5_pio.c` | `rc5_drain_fifo()` | `src/binmode/irtoy-irman.c` | No — hardware abstraction |
| `src/pirate/rc5_pio.c` | `rc5_rx_deinit()` | `src/binmode/irtoy-irman.c` | No — cleanup function |
| `src/pirate/psu.c` | `psu_clear_error_flag()` | `src/commands/global/w_psu.c` | Maybe — very thin |
| `src/pirate/lsb.c` | `lsb_get()` | `src/commands/global/cmd_convert.c` | Maybe — single use utility |
| `src/pirate/rgb.c` | `rgb_put()` | `src/mode/hwled.c` | No — hardware abstraction |
| `src/ui/ui_help.c` | `ui_help_pager_disable()` | `src/commands/global/h_help.c` | Maybe — very thin |
| `src/pirate/shift.c` | `shift_output_enable()` | `src/pirate.c` → startup | No — hardware init |
| `src/pirate/storage.c` | `storage_unmount()` | `src/pirate/storage.c` → mount | No — cleanup pair |

**Assessment:** Most single-caller functions follow valid embedded patterns (hardware init/cleanup pairs, mode dispatch). Only `psu_clear_error_flag()`, `lsb_get()`, and `ui_help_pager_disable()` are minor inline candidates.

### 2.2 Thin Wrappers

| File | Wrapper | Wraps | Recommendation |
|------|---------|-------|----------------|
| `src/ui/ui_prompt.c` | `ui_prompt_invalid_option()` | `ui_help_error()` | Keep — semantic clarity |
| `src/ui/ui_prompt.c` | `ui_prompt_mode_settings_int()` | `printf()` | Keep — used by many modes |
| `src/ui/ui_prompt.c` | `ui_prompt_mode_settings_string()` | `printf()` | Keep — used by many modes |
| `src/ui/ui_parse.c` | `ui_parse_get_colon()` | `ui_parse_get_delimited_sequence()` | Keep — readability |
| `src/ui/ui_parse.c` | `ui_parse_get_dot()` | `ui_parse_get_delimited_sequence()` | Keep — readability |
| `src/pirate/bio.c` | `bio_buf_output()` | `gpio_put()` | **Keep** — hardware abstraction layer |
| `src/pirate/bio.c` | `bio_buf_input()` | `gpio_put()` | **Keep** — hardware abstraction layer |
| `src/pirate/bio.c` | `bio_put()` | `gpio_put()` | **Keep** — hardware abstraction layer |
| `src/pirate/bio.c` | `bio_get()` | `gpio_get()` | **Keep** — hardware abstraction layer |
| `src/pirate/bio.c` | `bio_set_function()` | `gpio_set_function()` | **Keep** — hardware abstraction layer |
| `src/pirate/psu.c` | `psu_measure_vreg()` | `hw_adc_to_volts_x2(amux_read())` | **Keep** — meaningful abstraction |
| `src/pirate/psu.c` | `psu_measure_vout()` | `hw_adc_to_volts_x2(amux_read())` | **Keep** — meaningful abstraction |
| `src/pirate/hwspi.c` | `hwspi_read()` | `hwspi_write_read(0xff)` | **Keep** — SPI read semantics |
| `src/pirate/hw2wire_pio.c` | `pio_hw2wire_check_error()` | `pio_interrupt_get()` | **Keep** — hardware abstraction |
| `src/pirate/hw2wire_pio.c` | `pio_hw2wire_get()` | `pio_sm_get()` | **Keep** — hardware abstraction |

**Assessment:** All thin wrappers serve meaningful roles in the embedded architecture (hardware abstraction, semantic clarity, or mode dispatch). None are recommended for removal — the wrapper pattern is an intentional design choice for this firmware.

### 2.3 Legacy API Migration Status

| Legacy Function | Remaining Callers | Callers with bp_cmd_def | Callers without |
|-----------------|-------------------|------------------------|-----------------|
| `ui_prompt_bool()` | ~17 | Most mode files have bp_cmd (setup defs) | `legacy4third.c` |
| `ui_prompt_uint32()` | ~3 | `ui_display.c`, `ui_mode.c`, `ui_config.c` | All 3 lack bp_cmd |
| `ui_prompt_float()` | ~3 | `smps.c` has bp_cmd | `legacy4third.c` (2 calls) |
| `ui_prompt_mode_settings_int()` | ~18 | All mode setup files have bp_cmd | `glitch.c` (7 calls) |
| `ui_prompt_mode_settings_string()` | ~9 | All mode setup files have bp_cmd | `glitch.c` |
| `ui_prompt_user_input()` | 2 | `bp_cmd.c` (part of new system) | `ui_mode.c` |
| `ui_prompt_vt100_mode_start/feed()` | 3 | — | `pirate.c` (core system, not command) |
| `cmdln_args_find_flag()` | 4 | `eeprom_base.c` has bp_cmd | `ui_hex.c` (2), `ui_process.c` (2) |
| `cmdln_args_find_flag_uint32()` | 4 | `sine.c` has bp_cmd | `ui_hex.c` (2), `eeprom_1wire.c` |
| `cmdln_args_find_flag_string()` | 1 | — | `file.c` |
| `cmdln_args_string_by_position()` | 3 | — | `scope.c`, `ui_cmdln.c`, `ui_process.c` |
| `file_get_args()` | 5 | `i2c.c`, `ddr4.c`, `ddr5.c` have bp_cmd | `eeprom_1wire.c`, `eeprom_i2c.c` |
| `ui_hex_get_args_config()` | 8 | `spiflash.c`, `hex.c`, `ddr4.c`, `ddr5.c`, `i2c.c` have bp_cmd | `sle4442.c`, `eeprom_base.c` |

**Total remaining legacy API call sites: ~80+**

### 2.4 Duplicate Functionality

| Function A | Function B | Similarity | Recommendation |
|------------|------------|------------|----------------|
| `src/ui/ui_parse.c` (all 16 functions) | `src/deprecated/ui_parse.c` (all 16 functions) | **Identical** — complete duplicate | Delete deprecated version |
| `src/ui/ui_format.c` (5 functions) | `src/deprecated/ui_format.c` (5 matching + 1 extra) | **Near-identical** — active is superset | Delete deprecated version |
| `src/deprecated/bp_args.c` | `src/lib/bp_args/bp_cmd.c` | **Different API** — old vs new argument parsing | bp_cmd.c is the replacement |

---

## 3. Deprecated Folder Analysis

| File | Exported Functions | External Dependents | Safe to Delete? |
|------|-------------------|---------------------|-----------------|
| `bp_args.c` (516 lines) | `bp_args_init`, `bp_args_next`, `bp_args_positional`, `bp_args_get_*` (6 variants), `bp_args_has_flag`, `bp_args_find_*` (3 variants), `bp_args_positional_*` | **None** — no code outside deprecated/ calls these | ✅ Yes |
| `bp_args.h` (198 lines) | Header declarations for bp_args.c | **None** | ✅ Yes |
| `bp_args_compat.c` (288 lines) | `bp_compat_find_flag`, `bp_compat_find_flag_uint32`, `bp_compat_find_flag_string`, `bp_compat_*_by_position` | **None** | ✅ Yes |
| `bp_args_compat.h` (126 lines) | Header declarations for bp_args_compat.c | **None** | ✅ Yes |
| `bp_linenoise.c` (760 lines) | `bp_cmdln_*`, `bp_linenoise_*`, `edit_*`, history management | **None** | ✅ Yes |
| `bp_linenoise.h` (311 lines) | Header declarations for bp_linenoise.c | **None** | ✅ Yes |
| `queue.c` (171 lines) | `queue2_init*`, `queue2_try_*`, `queue2_*_blocking`, `queue_*` | **None** | ✅ Yes |
| `queue.h` (183 lines) | Header declarations for queue.c | **None** | ✅ Yes |
| `syntax.c` (771 lines) | `syntax_compile`, `syntax_run`, `syntax_post`, `syntax_run_*` (I/O operations) | **None** — active versions exist in `src/syntax_compile.c`, `src/syntax_run.c`, `src/syntax_post.c` | ✅ Yes |
| `ui_parse.c` (387 lines) | `ui_parse_get_*` (16 functions) | **None** — identical active version in `src/ui/ui_parse.c` | ✅ Yes |
| `ui_format.c` (294 lines) | `ui_format_*` (6 functions) | **None** — active version in `src/ui/ui_format.c` | ✅ Yes |
| `ui_format_old.h` (51 lines) | Header for deprecated ui_format | **None** | ✅ Yes |

**Conclusion: The entire `src/deprecated/` folder (12 files, ~4,000 lines) is fully isolated and can be safely deleted.**

---

## 4. Conditional Compilation

| BP_USE_* Flag | Mode/Feature | Status | Issues |
|---------------|-------------|--------|--------|
| `BP_USE_HW1WIRE` | 1-Wire protocol | ✅ Active | None |
| `BP_USE_HWUART` | UART protocol | ✅ Active | None |
| `BP_USE_HWHDUART` | Half-duplex UART | ✅ Active | None |
| `BP_USE_HWI2C` | I2C protocol | ✅ Active | None |
| `BP_USE_HWSPI` | SPI protocol | ✅ Active | None |
| `BP_USE_HW2WIRE` | 2-Wire protocol | ✅ Active | None |
| `BP_USE_HW3WIRE` | 3-Wire protocol | ✅ Active | ⚠️ Guard may be nested inside HW2WIRE guard in modes.c |
| `BP_USE_HWLED` | LED (WS2812/APA102) | ✅ Active | None |
| `BP_USE_DIO` | Digital I/O | ✅ Active | None |
| `BP_USE_INFRARED` | Infrared TX/RX | ✅ Active | None |
| `BP_USE_JTAG` | JTAG protocol | ✅ Active | None |
| `BP_USE_LCDSPI` | LCD SPI mode | ❌ Commented out | Orphaned mode files (LCDSPI.c, ST7735.c, HD44780.c) |
| `BP_USE_LCDI2C` | LCD I2C mode | ❌ Commented out | Mode exists in modes.c, guarded correctly |
| `BP_USE_SCOPE` | Logic analyzer/scope | ❌ Commented out | ⚠️ Flag defined but **no corresponding mode entry** in modes.c |
| `BP_USE_DUMMY1` | Dummy/test mode 1 | ❌ Commented out | Mode exists in modes.c, guarded correctly |
| `BP_USE_DUMMY2` | Dummy/test mode 2 | ❌ Commented out | Not referenced in modes.c |
| `BP_USE_BINLOOPBACK` | Binary loopback | ❌ Commented out | Referenced in modes.c but flag not defined in pirate.h |
| `BP_USE_USBPD` | USB Power Delivery | ❌ Commented out | Orphaned mode file (usbpd.c) |
| `BP_USE_I2S` | I2S audio | ❌ Commented out | Mode exists in modes.c, guarded correctly |

**Issues found:**
1. **`BP_USE_SCOPE`** — Flag exists in `pirate.h` (commented out) but no corresponding mode entry in `modes.c`. Either an incomplete feature or orphaned flag.
2. **`BP_USE_BINLOOPBACK`** — Referenced in `modes.c` but the flag definition in `pirate.h` is commented out. Mismatch.
3. **`BP_USE_DUMMY2`** — Commented out in `pirate.h` and no corresponding mode found in `modes.c`.
4. **HW3WIRE guard nesting** — The `[HW3WIRE]` mode entry in `modes.c` may have an incorrect guard nesting (inside `BP_USE_HW2WIRE` block). Verify the `#ifdef` structure.

---

## 5. Recommendations

### 5.1 Safe Immediate Deletions

These can be removed right now with zero risk:

1. **Delete `src/deprecated/` entirely** (~4,000 lines) — fully isolated, no external dependents
2. **Remove `#if 0` blocks in `src/pirate/hw1wire_pio.c`** (~400 lines) — code already migrated to `commands/1wire/`
3. **Remove `#if 0` blocks in `src/commands/global/flat.c`** (~130 lines) — development test code
4. **Remove `#if 0` blocks in `src/commands/i2c/usbpdo.c`** (~90 lines) — replaced by active implementations
5. **Remove `#if 0` blocks in `src/commands/i2c/ddr4.c`** (~50 lines) — vestigial code
6. **Delete `src/display/background_image_v4-orig.h`** — never included anywhere
7. **Delete `src/display/robot16.h`** — only referenced in commented-out include
8. **Remove `bio_buf_test()`** from `src/pirate/bio.c` and its declaration from `bio.h`
9. **Remove `rx_uart_disable()`** from `src/debug_uart.c` — empty, never called
10. **Delete `src/lib/sigrok/pico_sdk_sigrok.c`** (1,387 lines) — never compiled or referenced

**Estimated immediate savings: ~6,000+ lines**

### 5.2 Migration-Dependent Deletions

Code that becomes dead once specific migration steps complete:

- **Scenario A**: Complete `ui_hex_get_args_config` migration → unlocks simplification of `src/ui/ui_hex.c` argument handling (8 callers using legacy pattern)
- **Scenario B**: Complete `file_get_args` migration → unlocks simplification of `src/pirate/file.c` argument handling (5 callers)
- **Scenario C**: Finish mode setup migration (remove `ui_prompt_mode_settings_int/string` from all modes) → unlocks deletion of `ui_prompt_mode_settings_int()` and `ui_prompt_mode_settings_string()` (~18 + 9 call sites in mode files)
- **Scenario D**: Remove all `ui_prompt_bool/uint32/float` callers → unlocks deletion of interactive prompt functions in `ui_prompt.c` (~23 call sites)
- **Scenario E**: Remove all `cmdln_args_*` callers → unlocks deletion of entire legacy cmdln argument parsing system in `ui_cmdln.c` (~12 call sites remaining)
- **Scenario F**: Migrate `legacy4third.c` binary mode → removes last legacy prompt callers outside of mode setup

### 5.3 Refactoring Opportunities

1. **Make internal helpers static in `ui_cmdln.c`**: 6 functions (`cmdln_consume_white_space`, `cmdln_args_get_string/hex/bin/dec`, `cmdln_args_find_flag_internal`) are only used within the file — mark them `static`
2. **Remove `ui_prompt_menu_int()` and `ui_prompt_prompt_int()`**: Declared but appear unused
3. **Consider removing orphaned mode files**: 7 mode files (SW3W.c, SW2W.c, SWI2C.c, LCDSPI.c, ST7735.c, HD44780.c, usbpd.c) are not compiled. If the software implementations have been superseded by hardware PIO implementations, they can be archived or deleted
4. **Clean up `BP_USE_SCOPE` and `BP_USE_DUMMY2` flags**: Remove from `pirate.h` if no corresponding code exists
5. **Fix `BP_USE_HW3WIRE` guard nesting** in `modes.c`: Verify it has its own independent `#ifdef` block

### 5.4 Risk Assessment

**Verify before deleting — these may be referenced via indirect mechanisms:**

| Item | Risk | Verification Needed |
|------|------|-------------------|
| Orphaned mode files (SW3W.c, etc.) | Medium | Check if `modes.c` conditionally references them via commented-out `BP_USE_*` flags. If the flags are ever re-enabled, these files would be needed |
| `tusb_config.h` (flagged as orphaned header) | **False positive** | TinyUSB SDK auto-discovers this via include path — DO NOT DELETE |
| Display robot*.h headers | Low | These are conditionally included via `BP_SPLASH_FILE` macro per-platform. `robot5x16.h`, `robot6x16.h`, `robot5xx16.h` are **active** — only `robot16.h` is orphaned |
| `background.h` and `background_image_v4.h` | **False positive** | Actively included in `src/ui/ui_lcd.c` — DO NOT DELETE |
| Functions in `src/display/` and `src/toolbars/` | Low | These run on Core 1 and may have unusual call patterns via function pointer tables in `displays.c` |
| `ui_prompt_vt100_mode_start/feed()` | Low | Called from `pirate.c` core init — part of system bootstrap, not command migration |
| `src/lib/sigrok/pico_sdk_sigrok.c` | Low | Verify no build targets reference it before deleting |

### 5.5 Prioritized Action Plan

Ordered by impact/effort ratio (highest first):

| Priority | Action | Impact | Effort | Lines Saved |
|----------|--------|--------|--------|-------------|
| 1 | Delete `src/deprecated/` folder | High | Trivial | ~4,000 |
| 2 | Remove `#if 0` blocks in `hw1wire_pio.c` | Medium | Low | ~400 |
| 3 | Remove `#if 0` blocks in `flat.c`, `usbpdo.c`, `ddr4.c` | Medium | Low | ~270 |
| 4 | Delete orphaned display headers (`robot16.h`, `background_image_v4-orig.h`) | Low | Trivial | ~50 |
| 5 | Delete orphaned `src/lib/sigrok/pico_sdk_sigrok.c` | Medium | Low | ~1,387 |
| 6 | Remove dead functions (`bio_buf_test`, `rx_uart_disable`) | Low | Trivial | ~20 |
| 7 | Make `ui_cmdln.c` internal helpers `static` | Low | Low | 0 (quality) |
| 8 | Clean up `#if 0` blocks across remaining files | Medium | Medium | ~500 |
| 9 | Archive orphaned mode files (SW3W.c, etc.) | Medium | Low | ~1,474 |
| 10 | Clean up orphaned `BP_USE_*` flags (`SCOPE`, `DUMMY2`) | Low | Trivial | ~5 |
| 11 | Continue legacy API migration (Scenarios A–F) | High | High | ~500+ (incremental) |

**Total estimated removable: ~7,500–8,500 lines** across all actions.
