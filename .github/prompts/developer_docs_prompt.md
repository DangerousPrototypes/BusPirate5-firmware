# Prompt: Generate Bus Pirate Developer Documentation

> For Claude Opus 4.6 in VS Code with full workspace access.

---

## Your Task

Generate the remaining markdown documentation pages described in `docs/developer_docs_outline.md` for the Bus Pirate 5/6/7 firmware. Two pages already exist and are **approved** — do NOT recreate them. Use them as your style template. Read source code from the workspace to extract real struct definitions, function signatures, code excerpts, and examples.

---

## Already Complete — DO NOT Recreate

These two guides are finished, approved, and checked in. **Do not modify or regenerate them.** Use them only as style references:

1. **`docs/new_command_guide.md`** — Step-by-step guide to implementing a new global or mode command. Uses code excerpts from `src/commands/global/dummy.c`. 10 numbered steps, tables for every struct/enum, checklist at the end, API quick reference section.

2. **`docs/new_mode_guide.md`** — Step-by-step guide to implementing a new protocol mode. Uses code excerpts from `src/mode/dummy1.c`. 13 numbered steps, covers setup (dual-path interactive + CLI), syntax handlers, macros, periodic service, registration in `modes.c`, checklist at the end.

---

## Style Rules (Match the Existing Guides)

Read both `docs/new_command_guide.md` and `docs/new_mode_guide.md` in full before writing anything. Then follow these conventions exactly:

- **Title**: `# Topic Name` with a `> One-line description` blockquote beneath
- **Separator**: `---` between the overview and the first section
- **Section headers**: `## Step N: Name` for how-to guides, `## Section Name` for reference pages
- **Code excerpts**: Real code from the codebase in fenced ```c blocks, with contextual comments. Never invent fake code — read the actual source files.
- **Tables**: Use tables for struct fields, enum values, function signatures, and comparisons. Every struct/type gets a field table.
- **Key points**: Bold for emphasis, `code backticks` for identifiers, *italics* sparingly
- **Cross-references**: Relative markdown links: `[other_doc.md](other_doc.md)`, source file references as inline code
- **Checklist**: End how-to guides with a `## Checklist` section using `- [ ]` items
- **Related Documentation**: End every page with a `## Related Documentation` section linking to relevant sibling docs and source files
- **No migration content**: The outline includes migration material from `bp_cmd_developer_docs_outline.md`. Skip all of §10 (Migration Guide) and §11 (What's NOT Replaced). These are internal working documents, not developer docs.
- **Concise**: These are reference docs for working developers, not tutorials. Be direct. Every sentence should teach something.

---

## Source Material

You have three source documents to work from. Read them all before starting:

1. **`docs/developer_docs_outline.md`** — The master outline defining all pages, their sections, and bullet points. This is your table of contents. Follow the section structure.

2. **`docs/bp_cmd_developer_docs_outline.md`** — Detailed outline for the `bp_cmd` framework (§3 in the master outline). Contains type descriptions, API signatures, pattern descriptions, and architectural notes. Use the non-migration sections (§1–9, §12) as source material for the `bp_cmd` reference pages.

3. **The codebase itself** — Read the actual source files to extract real struct definitions, function signatures, code excerpts, and usage examples. Key files:
   - `src/lib/bp_args/bp_cmd.h` — All `bp_cmd` types and function declarations
   - `src/lib/bp_args/bp_cmd.c` — Implementation (parsing, validation, prompting, help)
   - `src/lib/bp_args/bp_cmd_linenoise.c` — Hint/completion callbacks
   - `src/commands.c` — Global command registration (`commands[]` array, `_global_command_struct`)
   - `src/command_struct.h` — `_global_command_struct`, `_mode_command_struct`, `command_result`, `cmd_category`
   - `src/modes.c` — Mode registration (`modes[]` array)
   - `src/pirate.h` — Master config, platform selection, `BP_HW_*` flags, mode enable defines
   - `src/system_config.h` — `system_config` struct
   - `src/syntax.h`, `src/syntax_struct.h`, `src/syntax_internal.h` — Syntax pipeline
   - `src/bytecode.h` — `struct _bytecode`, opcodes, error codes
   - `src/pirate/bio.h` — BIO pin functions
   - `src/pirate/storage.h` — Storage API, `mode_config_t`
   - `src/usb_rx.h`, `src/usb_tx.h` — USB communication
   - `src/spsc_queue.h` — Lock-free inter-core queue
   - `src/binmode/binmode.h` — Binary mode struct and dispatch
   - `src/displays.h` — Display mode struct
   - `src/translation/base.h` — T_ enum
   - `src/translation/en-us.c` — English string table + language-adding guide
   - `src/system_monitor.h` — System monitor
   - `src/pirate/amux.h` — Analog multiplexer
   - `src/boards/` — Board and platform headers
   - `CMakeLists.txt`, `src/CMakeLists.txt` — Build system
   - `docker/` — Docker build environment
   - `tests/` — Test infrastructure
   - Reference implementations: `src/commands/global/dummy.c`, `src/mode/dummy1.c`, `src/mode/hwuart.c`, `src/commands/global/w_psu.c`, `src/commands/spi/flash.c`, `src/binmode/binsump.c`, `src/binmode/binio.c`

---

## Pages to Generate

Generate each page as a separate markdown file in `docs/`. Follow the priority order below. For each page, read the relevant source files first, then write the doc with real code excerpts.

### Tier 1 — Generate These First

**File: `docs/bp_cmd_data_types.md`**
- Source: `bp_cmd_developer_docs_outline.md` §3, plus `src/lib/bp_args/bp_cmd.h`
- Read `bp_cmd.h` and extract the actual struct definitions for: `bp_command_def_t`, `bp_command_opt_t`, `bp_command_positional_t`, `bp_val_constraint_t`, `bp_val_choice_t`, `bp_command_action_t`, `bp_action_delegate_t`, `bp_cmd_status_t`, `bp_arg_type_t`
- For each type: show the real struct definition in a code block, then a field table explaining each field
- Include the sentinel convention (`{ 0 }` termination for opts and positionals arrays)
- Include lifetime rules (always `static const` or file-scope `const` if `extern`)
- Show real usage examples from `dummy.c`, `dummy1.c`, `hwuart.c`, `flash.c`, `ui_mode.c`

**File: `docs/bp_cmd_parsing_api.md`**
- Source: `bp_cmd_developer_docs_outline.md` §4 + §6, plus `src/lib/bp_args/bp_cmd.h`
- Read `bp_cmd.h` for the actual function declarations
- Cover: action resolution, simple flag queries, simple positional queries, remainder access, constraint-aware resolution
- Include the help system here (§6): `bp_cmd_help_check()`, `bp_cmd_help_show()`
- Show real examples from `dummy.c`, `w_psu.c`, `flash.c`
- End with a complete function signature table (like the one in `new_command_guide.md` "Parsing API Quick Reference")

**File: `docs/translation_guide.md`**
- Source: outline §4.4, plus `src/translation/base.h`, `src/translation/en-us.c`, `check_translations.py`
- Read `en-us.c` header comments for the 7-step language-adding guide
- Read `base.h` for the `T_` enum structure
- Read `check_translations.py` for the toolchain
- Cover: how T_ constants work, `GET_T()`, adding a new string, adding a new language, the `0` placeholder convention for development, the `json2h.py` pipeline

**File: `docs/syntax_bytecode_guide.md`**
- Source: outline §4.1, plus `src/syntax.h`, `src/syntax_struct.h`, `src/syntax_internal.h`, `src/bytecode.h`, `src/syntax_compile.c`, `src/syntax_run.c`, `src/syntax_post.c`
- Read `bytecode.h` for the actual `struct _bytecode` definition and opcodes
- Read `syntax_struct.h` for `struct _syntax_io`
- Emphasize the **critical rule**: no `printf()` during execute phase
- Show how modes plug in via function pointers (cross-ref `new_mode_guide.md` Step 7)
- Include opcode table, error code table, result struct field table

### Tier 2 — Generate These Next

**File: `docs/build_system_guide.md`**
- Source: outline §1.1, plus `CMakeLists.txt`, `src/CMakeLists.txt`, `docker/Dockerfile`, `docker-compose.yml`
- Read both CMakeLists.txt files for actual target definitions, PIO compilation commands, platform selection logic
- Cover: how to build, platform selection, adding a new target, PIO pipeline, Docker environment

**File: `docs/bio_pin_guide.md`**
- Source: outline §4.2, plus `src/pirate/bio.h`, `src/pirate/bio.c`, `src/system_config.h`
- Read `bio.h` for actual function declarations and the `bp_bio_pins` enum
- Read a platform header (e.g. `src/boards/bp5_rev10.h`) for `bio2bufiopin[]` mappings
- Show the pin claiming pattern from `dummy1.c` setup_exc/cleanup
- Cross-ref `new_mode_guide.md` Step 5

**File: `docs/storage_guide.md`**
- Source: outline §4.3, plus `src/pirate/storage.h`
- Read `storage.h` for actual API: `storage_save_mode()`, `storage_load_mode()`, `mode_config_t`
- Show the complete storage descriptor pattern from `dummy1.c` Step 4a
- Cover: config file naming, FatFS operations, global system config
- Cross-ref `new_mode_guide.md` Step 4

**File: `docs/binary_mode_guide.md`**
- Source: outline §2.3, plus `src/binmode/binmode.h`, `src/binmode/binmode.c`, `src/binmode/binsump.c`, `src/binmode/binio.c`
- Read `binmode.h` for the `binmode_t` struct definition
- Read `binsump.c` as a clean example implementation
- Style: step-by-step like `new_command_guide.md` and `new_mode_guide.md`
- Include checklist at the end

### Tier 3 — Generate These Last

**File: `docs/board_abstraction_guide.md`**
- Source: outline §1.2, plus `src/pirate.h`, board/platform headers in `src/boards/`
- Read `pirate.h` for the `BP_VER`/`BP_REV` cascade and `BP_HW_*` flags
- Read one platform header (e.g. `bp5_rev10.h`) and its matching board header as examples
- Cover: what to create when adding a new hardware revision

**File: `docs/dual_core_guide.md`**
- Source: outline §1.3, plus `src/spsc_queue.h`, `src/usb_rx.h`, `src/usb_tx.h`
- Read `spsc_queue.h` — it's well-commented, extract the API and design rationale
- Cover: Core 0 vs Core 1 responsibilities, SPSC queue API, memory barrier usage
- Describe the data flow: USB ↔ Core 1 ↔ SPSC queues ↔ Core 0 ↔ protocol engine

**File: `docs/bp_cmd_prompting.md`**
- Source: `bp_cmd_developer_docs_outline.md` §5, plus `src/lib/bp_args/bp_cmd.c` (the `bp_cmd_prompt` function)
- Show the dual-path pattern with real examples from `hwuart.c` and `w_psu.c`
- Cover: `BP_VAL_UINT32` prompts, `BP_VAL_CHOICE` prompts, saved config integration

**File: `docs/bp_cmd_linenoise.md`**
- Source: `bp_cmd_developer_docs_outline.md` §7, plus `src/lib/bp_args/bp_cmd_linenoise.c`
- Read the actual callback implementations
- Cover: hint generation, tab completion, sub-definition awareness, `collect_defs()`

**File: `docs/bp_cmd_patterns.md`**
- Source: `bp_cmd_developer_docs_outline.md` §9
- For each pattern: name it, cite the example file, show a minimal code excerpt from that file
- Read the actual source files: `monitor.c`, `w_psu.c`, `flash.c`, `ui_mode.c`, `hwuart.c`, `freq.c`, `pwm.c`
- This should be a quick-reference "cookbook" page

**File: `docs/usb_communication_guide.md`**
- Source: outline §4.5, plus `src/usb_rx.h`, `src/usb_tx.h`, `src/spsc_queue.h`, `src/tusb_config.h`
- Cover: TinyUSB interfaces, SPSC queues, RX/TX APIs, binary mode data path, debug paths

**File: `docs/display_mode_guide.md`** *(short page)*
- Source: outline §2.4, plus `src/displays.h`, `src/displays.c`
- Read the `_display` struct and `displays[]` table
- Show how to add a new display mode

**File: `docs/system_config_reference.md`** *(reference page)*
- Source: outline §6.1, plus `src/system_config.h`
- Read the actual `system_config` struct and document each field
- Cover: when to read vs write, the `.error` flag for command chaining, `.mode`, `.psu`

**File: `docs/error_handling_reference.md`** *(short reference page)*
- Source: outline §6.3
- Cover: `system_config.error`, `SERR_*` codes, `FRESULT`, conventions

**File: `docs/testing_guide.md`**
- Source: outline §5.1, plus `tests/test_spsc_queue.c`, `tests/run_tests.sh`, `tests/hardware/`, `tests/pico/`
- Read the test file for the framework pattern
- Cover: SDK mocking, building tests, extending to other subsystems

**File: `docs/system_monitor_guide.md`** *(short page)*
- Source: outline §4.6, plus `src/system_monitor.h`, `src/system_monitor.c`, `src/pirate/amux.h`
- Cover: voltage/current monitoring, AMUX, PSU control

---

## Methodology

For each page:

1. **Read the outline section** for that page to know what to cover
2. **Read the source files** listed — extract real struct definitions, real function signatures, real code excerpts
3. **Read the approved guides** (`new_command_guide.md`, `new_mode_guide.md`) if you haven't already to match style
4. **Write the page** following the style rules above
5. **Cross-reference**: link to sibling docs and source files in the Related Documentation section
6. **Never invent code** — every code block should come from an actual source file in the workspace

Work through the tiers in order. After each page, verify it follows the style rules before moving to the next.

---

## File Naming Convention

All files go in `docs/`. Use lowercase with underscores:

```
docs/bp_cmd_data_types.md
docs/bp_cmd_parsing_api.md
docs/bp_cmd_prompting.md
docs/bp_cmd_linenoise.md
docs/bp_cmd_patterns.md
docs/translation_guide.md
docs/syntax_bytecode_guide.md
docs/build_system_guide.md
docs/bio_pin_guide.md
docs/storage_guide.md
docs/binary_mode_guide.md
docs/board_abstraction_guide.md
docs/dual_core_guide.md
docs/usb_communication_guide.md
docs/display_mode_guide.md
docs/system_config_reference.md
docs/error_handling_reference.md
docs/testing_guide.md
docs/system_monitor_guide.md
```

---

## What NOT to Do

- Do NOT recreate `new_command_guide.md` or `new_mode_guide.md`
- Do NOT include migration material (old API → new API tables, migration checklists)
- Do NOT invent fake code examples — always read the real source
- Do NOT write long prose paragraphs — use tables, code blocks, and bullet points
- Do NOT duplicate content between pages — cross-reference instead
- Do NOT modify any source code files — this is a documentation-only task
