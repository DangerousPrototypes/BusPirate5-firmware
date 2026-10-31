# Bus Pirate Developer Documentation — Outline

> Proposed structure for a developer documentation section covering firmware
> architecture, subsystem guides, and reference material.

---

## Section 1: Getting Started

### 1.1 Build System & Targets *(new page)*
- Platform selection: `BP_PICO_PLATFORM=rp2350` vs default RP2040
- Build targets: `bus_pirate5_rev8`, `bus_pirate5_rev10`, `bus_pirate5_xl`, `bus_pirate6`
- `BP_VER` / `BP_REV` compile defines and what they control
- Adding a new build target
- PIO program compilation pipeline (`pico_generate_pio_header()`)
- Linker scripts and memory layout
- Docker build environment (`docker/`, `docker-compose.yml`)

### 1.2 Board Abstraction & Platform Porting *(new page)*
- The header cascade: `pirate.h` → platform header → board header → linker script
- `BP_HW_*` feature flags (`BP_HW_STORAGE_NAND`, `BP_HW_PSU_DAC`, `BP_HW_IOEXP_*`, etc.)
- Platform headers (`bp5_rev10.h`, `bp6_rev2.h`, etc.) — pin maps, ADC mux, display config
- Board headers (`bp5_rev10_board.h`) — flash size, SDK platform
- What to create when adding a new hardware revision

### 1.3 Dual-Core Architecture *(new page)*
- Core 0 vs Core 1 responsibilities
- SPSC queue: lock-free inter-core communication (`spsc_queue.h`)
- Memory barriers and `__dmb()` usage
- `intercore_helpers.h` — safe cross-core calls
- Data flow diagram: USB ↔ Core 1 ↔ SPSC queues ↔ Core 0 ↔ protocol engine

---

## Section 2: Implementing New Features

### 2.1 Implementing a New Command ✅ *(exists: `new_command_guide.md`)*
- Global vs mode commands
- `bp_command_def_t` definition (constraints → opts → def)
- Handler function patterns (help, actions, flags, prompting)
- Registration in `commands[]` or mode command table
- Reference implementation: `src/commands/global/dummy.c`

### 2.2 Implementing a New Mode ✅ *(exists: `new_mode_guide.md`)*
- Setup (interactive wizard + CLI flags, saved settings)
- Hardware init/teardown, pin claiming
- Syntax handlers (write, read, start, stop)
- Macros, periodic service, settings display
- Registration in `modes.c`
- Reference implementation: `src/mode/dummy1.c`

### 2.3 Implementing a New Binary Mode *(new page)*
- `binmode_t` struct: `binmode_setup()`, `binmode_service()`, `binmode_cleanup()`
- `BINMODE_USE_*` enum and dispatch table in `binmode.c`
- Terminal locking (`binmode_terminal_lock()`)
- BPIO sub-modules: adding a new per-protocol binary handler
- `bpio_mode_configuration_t` — standardized mode config struct
- Reference: `binsump.c` (SUMP logic analyzer), `binio.c` (BPIO protocol)

### 2.4 Adding a New Display Mode *(new page — short)*
- `_display` struct — function pointer table (periodic, setup, cleanup, lcd_update)
- `displays[]` dispatch table
- Scope display as reference implementation
- LCD update patterns and refresh coordination

---

## Section 3: `bp_cmd` Framework Reference

> The unified command definition, parsing, validation, prompting, help, hints,
> and completion system. Extracted from `bp_cmd_developer_docs_outline.md`,
> migration material removed.

### 3.1 Architecture Overview *(new page)*
- Single source of truth: one `bp_command_def_t` → five concerns
- Files: `bp_cmd.h`, `bp_cmd.c`, `bp_cmd_linenoise.c`
- Design principles: zero allocation, stateless re-scan, static const data

### 3.2 Data Types Reference *(new page)*
- `bp_command_def_t` — command definition (fields, lifetime, sentinel rules)
- `bp_command_opt_t` — flag/option descriptor (`BP_ARG_NONE` / `BP_ARG_REQUIRED`)
- `bp_command_positional_t` — positional argument descriptor (1-based indexing)
- `bp_val_constraint_t` — value constraint (`BP_VAL_UINT32` / `INT32` / `FLOAT` / `CHOICE`)
- `bp_val_choice_t` — named choice entry (name, alias, label, value)
- `bp_command_action_t` — action/subcommand verb (static array)
- `bp_action_delegate_t` — dynamic verb source (runtime verb resolution, sub-def support)
- `bp_cmd_status_t` — return codes (`OK` / `MISSING` / `INVALID` / `EXIT`)

### 3.3 Parsing API *(new page)*
- Action resolution: `bp_cmd_get_action()`
- Simple flag queries: `bp_cmd_find_flag()`, `bp_cmd_get_uint32()`, `bp_cmd_get_string()`, etc.
- Simple positional queries: `bp_cmd_get_positional_string()`, `bp_cmd_get_positional_uint32()`, etc.
- Remainder access: `bp_cmd_get_remainder()`
- Constraint-aware resolution: `bp_cmd_flag()`, `bp_cmd_positional()` → `bp_cmd_status_t`
- Flag syntax: `-f value`, `-f=value`, `--long value`, `--long=value`

### 3.4 Interactive Prompting *(new page)*
- `bp_cmd_prompt(constraint, &out)` — prompt loop from a constraint
- `BP_VAL_UINT32` prompts: range display, validation, retry
- `BP_VAL_CHOICE` prompts: numbered menu, name/alias/number input
- Dual-path pattern: CLI flag → fallback to interactive prompt
- Saved configuration integration (`storage_load_mode` / `storage_save_mode`)

### 3.5 Help System *(section in 3.3 or standalone)*
- `bp_cmd_help_check(def, help_flag)` — conditional help display
- `bp_cmd_help_show(def)` — unconditional help display
- Auto-generated help format: usage, flags table, actions list

### 3.6 Linenoise Integration *(new page)*
- Hint generation: `bp_cmd_hint()` — ghost text on every keystroke
- Tab completion: `bp_cmd_completion()` — completes commands, verbs, flags
- Sub-definition awareness: `m uart -<Tab>` completes UART flags
- Linenoise glue: `bp_cmd_linenoise_init()`, `collect_defs()`

### 3.7 Patterns & Recipes *(new page or appendix)*
- Simple command (help + one flag) — `monitor.c`
- Command with positionals — `w_psu.c`
- Command with actions/subcommands — `flash.c`
- Command with dynamic verbs (delegate) — `ui_mode.c`
- Mode setup (dual-path wizard + CLI) — `hwuart.c`
- Pin selection with hardware validation — `freq.c`, `pwm.c`
- Enable/disable command pairs — `W`/`w`, `G`/`g`, `P`/`p`

### 3.8 API Quick Reference *(appendix / cheat sheet)*
- Parsing functions table (signature, returns, purpose)
- Constraint-aware functions table
- Status codes table
- Constraint types table

---

## Section 4: Core Subsystem Guides

### 4.1 Syntax & Bytecode Pipeline *(new page)*
- Three-phase architecture: compile → execute → post-process
- `struct _bytecode` — the 28-byte instruction (out_data, in_data, error, message fields)
- `struct _syntax_io` — global state (out[] / in[] arrays, 1024 entries each)
- Opcode table: `SYN_WRITE`, `SYN_READ`, `SYN_START`, `SYN_STOP`, `SYN_DELAY_*`, etc.
- `syntax_compile_commands[]` — character-to-opcode mapping
- **Critical rule**: no `printf()` during execute phase — use result struct fields
- Error codes: `SERR_NONE`, `SERR_DEBUG`, `SERR_INFO`, `SERR_WARN`, `SERR_ERROR`
- How modes plug in via function pointers

### 4.2 Pin & BIO System *(new page)*
- BIO pins: `BIO0`–`BIO7`, bidirectional level-shifted buffers
- `bio_output()` / `bio_input()` — set direction (both buffer IC and GPIO)
- `bio_put()` / `bio_get()` — read/write pin state
- Pin claiming: `system_bio_update_purpose_and_label()` — claim/release, update status bar
- `enum bp_pin_func` — `BP_PIN_MODE`, `BP_PIN_PWM`, `BP_PIN_FREQ`, etc.
- Pin conflict prevention: claimed pins blocked from other subsystems
- Per-platform mappings: `bio2bufiopin[]`, `bio2bufdirpin[]`

### 4.3 Storage & Persistence *(new page)*
- FatFS on NAND flash (rev9+) or TF/microSD (rev8)
- `mode_config_t` descriptor pattern: JSON tag → config pointer → format
- `storage_save_mode()` / `storage_load_mode()` — mode settings persistence
- `storage_save_config()` / `storage_load_config()` — global system config
- `storage_save_binary_blob_rollover()` — binary data logging
- File operations: `f_open`, `f_write`, `f_read`, `f_close` (FatFS API)
- Config file naming convention (`bpuart.bp`, `bpspi.bp`, etc.)

### 4.4 Translation & Localization *(new page)*
- The `T_` enum: every UI string gets a constant in `base.h`
- `GET_T(T_CONSTANT)` — runtime string lookup with fallback
- English source of truth: `translation/en-us.c`
- Adding a new string: edit `base.h` → run `json2h.py` → use `GET_T()`
- Adding a new language: 7-step process (documented in `en-us.c` header)
- Translation toolchain: `json2h.py`, template files, JSON language files
- Placeholder convention: use `0` for T_ keys during development

### 4.5 USB & Communication *(new page)*
- TinyUSB: CDC (terminal) + MSC (mass storage) interfaces
- SPSC queues: `usb_rx_fifo` / `usb_tx_fifo` (terminal), `bin_rx_fifo` / `bin_tx_fifo` (binary mode)
- Receive API: `rx_fifo_try_get()`, `rx_fifo_peek()`, `rx_fifo_wait_for_data()`
- Transmit API: `tx_fifo_put()`, `tx_sb_fifo_put()` (status bar)
- Debug paths: UART (`debug_uart.c`) and RTT (`debug_rtt.c`)

### 4.6 System Monitor & Power Supply *(new page — short)*
- `system_monitor.c` — continuous voltage/current monitoring via AMUX
- `amux.h` — analog multiplexer control, ADC readings
- `psu.h` — programmable power supply (PWM or DAC depending on hardware)
- Character-level change detection for efficient display updates

---

## Section 5: Testing

### 5.1 Host-Side Testing *(new page)*
- Current state: `tests/test_spsc_queue.c` — the only test file
- Test framework: custom `RUN_TEST()` macros, `TEST_PASS`/`TEST_FAIL`
- SDK mocking pattern: stub headers under `tests/hardware/`, `tests/pico/`
- Building tests: direct `gcc -pthread` compilation, `run_tests.sh`
- Extending the pattern to other subsystems
- What can be tested on host (pure logic, queues, parsers) vs what needs hardware

---

## Section 6: Reference

### 6.1 `system_config` Reference *(new page)*
- `system_config.mode` — current active mode enum
- `system_config.error` — error flag for command chaining
- `system_config.psu` — PSU state
- Pin state tracking, display state, terminal config
- When to read vs write `system_config` fields

### 6.2 Command Categories & Help System *(short reference)*
- `enum cmd_category` — `CMD_CAT_IO` through `CMD_CAT_HIDDEN`
- How the `h` command groups output by category
- `ui_help_mode_commands()` — mode command help display

### 6.3 Error Handling Conventions *(short reference)*
- `system_config.error = true` — command-level error signaling
- `SERR_*` codes — bytecode pipeline error levels
- `FRESULT` — FatFS error codes
- When to `return` vs continue after errors

---

## Page Status

| Page | Status | File |
|------|--------|------|
| Implementing a New Command | ✅ Done | `new_command_guide.md` |
| Implementing a New Mode | ✅ Done | `new_mode_guide.md` |
| `bp_cmd` Outline (source material) | ✅ Done | `bp_cmd_developer_docs_outline.md` |
| Build System & Targets | 📋 Proposed | — |
| Board Abstraction & Porting | 📋 Proposed | — |
| Dual-Core Architecture | 📋 Proposed | — |
| Binary Mode Guide | 📋 Proposed | — |
| Display Mode Guide | 📋 Proposed | — |
| `bp_cmd` Architecture | 📋 Proposed (from outline §1-2) | — |
| `bp_cmd` Data Types | 📋 Proposed (from outline §3) | — |
| `bp_cmd` Parsing API | 📋 Proposed (from outline §4) | — |
| `bp_cmd` Prompting | 📋 Proposed (from outline §5) | — |
| `bp_cmd` Linenoise | 📋 Proposed (from outline §7) | — |
| `bp_cmd` Patterns | 📋 Proposed (from outline §9) | — |
| Syntax & Bytecode Pipeline | 📋 Proposed | — |
| Pin & BIO System | 📋 Proposed | — |
| Storage & Persistence | 📋 Proposed | — |
| Translation & Localization | 📋 Proposed | — |
| USB & Communication | 📋 Proposed | — |
| System Monitor & PSU | 📋 Proposed | — |
| Host-Side Testing | 📋 Proposed | — |
| `system_config` Reference | 📋 Proposed | — |
| Error Handling Conventions | 📋 Proposed | — |

---

## Suggested Priority Order

**Tier 1 — Most useful right now** (actively needed by contributors):
1. `bp_cmd` Data Types Reference *(§3.2)* — the types are new, people need a lookup
2. `bp_cmd` Parsing API *(§3.3)* — most-used API surface
3. Translation & Localization *(§4.4)* — every new string touches this, no standalone doc exists
4. Syntax & Bytecode Pipeline *(§4.1)* — the `printf()` prohibition and result struct are a constant gotcha

**Tier 2 — Important for new contributors**:
5. Build System & Targets *(§1.1)* — first thing a new contributor hits
6. Pin & BIO System *(§4.2)* — used by every mode, only shown by example today
7. Storage & Persistence *(§4.3)* — every mode with saved settings needs this
8. Binary Mode Guide *(§2.3)* — no docs exist, growing area

**Tier 3 — Useful but less urgent**:
9. Board Abstraction & Porting *(§1.2)* — needed when new hardware ships
10. Dual-Core Architecture *(§1.3)* — important but rarely touched
11. `bp_cmd` Prompting *(§3.4)* — covered adequately in the command/mode guides
12. Host-Side Testing *(§5.1)* — small today, document as it grows
13. Everything else
