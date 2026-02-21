




































# Dead Code & Rarely Used Code Analysis — Agent Prompt

## Agent Instructions

You are a Claude Opus 4.6 agent performing a comprehensive dead code and rarely-used code analysis on the Bus Pirate 5/6/7 firmware codebase at `/home/ian/bp5fw`. This is an embedded C firmware for RP2040/RP2350 microcontrollers. Your goal is to produce a detailed report with actionable recommendations.

## Context

This codebase is undergoing an active migration from a legacy command/argument parsing system (`ui_prompt`, `ui_cmdln_*`, `cmdln_args_*`) to a modern `bp_cmd` system in `src/lib/bp_args/bp_cmd.c/.h`. There is already a `src/deprecated/` folder containing known deprecated files. The project uses conditional compilation (`#define BP_USE_*` in `src/pirate.h`) to enable/disable protocol modes per target board.

## Analysis Tasks

### Phase 1: Identify Dead Code

Systematically search for and categorize the following:

#### 1.1 — Unreferenced Functions
- Search all `.c` files under `src/` for function definitions (non-static and static)
- For each function, check if it is referenced anywhere else in the codebase (called, assigned to a function pointer, or registered in a dispatch table)
- Pay special attention to `src/ui/`, `src/pirate/`, and `src/commands/` — these are the areas most affected by the migration
- **Exclude** third-party bundled libraries from analysis: `src/flatcc/`, `src/fatfs/`, `src/printf-4.0.0/`, `src/mjson/`, `src/lib/nanocobs/`, `src/dhara/`, `src/nand/`, `src/font/`
- **Include** but flag separately: `src/deprecated/` (these are expected dead code but verify nothing still depends on them)

#### 1.2 — Unreferenced Header Declarations
- Find functions declared in `.h` files that have no corresponding implementation in any `.c` file
- Find functions declared in `.h` files whose implementations exist but are never called

#### 1.3 — `#if 0` Blocks and Commented-Out Code
- Search for `#if 0` ... `#endif` blocks in project source (not third-party)
- Search for large blocks of commented-out code (3+ consecutive lines of `//` comments that look like disabled code, not documentation)
- Assess whether these blocks are vestigial or intentional feature flags

#### 1.4 — Orphaned Files
- Check if any `.c` files under `src/` are NOT listed in any `CMakeLists.txt` (and therefore never compiled)
- Check if any `.h` files under `src/` are never `#include`d by any `.c` file

#### 1.5 — Dead Translation Keys
- In `src/translation/en-us.h`, identify translation key IDs (`T_*`) that are defined but never referenced in any `.c` or `.h` file outside of the translation files themselves

### Phase 2: Identify Rarely-Used / Thin-Wrapper Code

#### 2.1 — Functions Called Only Once
- Find non-static functions that are called from exactly one location
- Assess whether these are meaningful abstractions or unnecessary indirection that could be inlined

#### 2.2 — Thin Wrappers
- Identify functions whose body is a single call to another function (possibly with trivial argument transformation)
- Especially look in `src/ui/` and `src/pirate/` for wrappers around lower-level APIs

#### 2.3 — Legacy API Surface Still in Use
- Count remaining callers of these legacy APIs (the migration targets):
  - `ui_prompt_bool()`
  - `ui_prompt_uint32()`
  - `ui_prompt_float()`
  - `ui_prompt_mode_settings_int()`
  - `ui_prompt_mode_settings_string()`
  - `ui_prompt_user_input()`
  - `ui_prompt_vt100_mode_start()` / `ui_prompt_vt100_mode_feed()`
  - `cmdln_args_find_flag()`
  - `cmdln_args_find_flag_uint32()`
  - `cmdln_args_find_flag_string()`
  - `cmdln_args_string_by_position()`
  - `file_get_args()` (in `src/pirate/file.c`)
  - `ui_hex_get_args_config()` (in `src/ui/ui_hex.c`)
- For each, list the file and function where the call occurs
- Identify which callers are in code that has a `bp_command_def_t` (partially migrated) vs. code that has no `bp_cmd` integration yet

#### 2.4 — Duplicate / Near-Duplicate Functionality
- Look for functions that do substantially the same thing but exist in different files
- Specifically check:
  - `src/ui/ui_parse.c` vs `src/deprecated/ui_parse.c` — is there duplication?
  - `src/ui/ui_format.c` vs `src/deprecated/ui_format.c` — same question
  - `src/deprecated/bp_args.c` vs `src/lib/bp_args/bp_cmd.c` — overlap?
  - Multiple file-open/error-report patterns across commands

### Phase 3: Analyze the `src/deprecated/` Folder

- For each file in `src/deprecated/`:
  - List all functions it exports
  - Check if any code outside `src/deprecated/` still calls those functions
  - Determine if the file can be safely deleted, or if it still has active dependents
  - If it has dependents, list them

### Phase 4: Conditional Compilation Analysis

- Examine `src/pirate.h` for all `BP_USE_*` defines
- For each protocol mode in `src/mode/`, verify it is guarded by a `BP_USE_*` flag
- Identify any code that is compiled unconditionally but only makes sense when a specific mode is enabled
- Check for orphaned `BP_USE_*` flags that are defined but whose guarded code no longer exists

## Report Format

Produce the report in Markdown with the following structure:

```
# Dead Code & Rarely-Used Code Analysis Report

## Executive Summary
- Total dead functions found
- Total orphaned files found  
- Total legacy API call sites remaining
- Key risk areas
- Estimated lines removable

## 1. Dead Code

### 1.1 Unreferenced Functions
| File | Function | Static? | Notes |
|------|----------|---------|-------|

### 1.2 Orphaned Declarations
| Header | Declaration | Status |
|--------|-------------|--------|

### 1.3 Disabled Code Blocks (#if 0 / commented out)
| File | Lines | Description | Recommendation |
|------|-------|-------------|----------------|

### 1.4 Orphaned Files
| File | In CMakeLists? | Included anywhere? | Recommendation |
|------|----------------|---------------------|----------------|

### 1.5 Dead Translation Keys
| Key | Defined in | Referenced? | Recommendation |
|-----|------------|-------------|----------------|

## 2. Rarely-Used Code

### 2.1 Single-Caller Functions  
| File | Function | Single caller location | Inline candidate? |
|------|----------|----------------------|-------------------|

### 2.2 Thin Wrappers
| File | Wrapper | Wraps | Recommendation |
|------|---------|-------|----------------|

### 2.3 Legacy API Migration Status
| Legacy Function | Remaining callers | Callers with bp_cmd_def | Callers without |
|-----------------|-------------------|------------------------|-----------------|

### 2.4 Duplicate Functionality
| Function A | Function B | Similarity | Recommendation |
|------------|------------|------------|----------------|

## 3. Deprecated Folder Analysis
| File | Exported functions | External dependents | Safe to delete? |
|------|-------------------|---------------------|-----------------|

## 4. Conditional Compilation
| BP_USE_* Flag | Mode/Feature | Status | Issues |
|---------------|-------------|--------|--------|

## 5. Recommendations

### 5.1 Safe Immediate Deletions
Files and functions that can be removed right now with zero risk.

### 5.2 Migration-Dependent Deletions  
Code that becomes dead once specific migration steps complete. 
Present as scenarios:
- **Scenario A**: Complete `ui_hex` migration → unlocks deletion of...
- **Scenario B**: Complete `file_get_args` migration → unlocks deletion of...
- **Scenario C**: Finish mode setup migration → unlocks deletion of...
- **Scenario D**: Remove all `ui_prompt` callers → unlocks deletion of...

### 5.3 Refactoring Opportunities
Single-caller functions or thin wrappers worth consolidating.

### 5.4 Risk Assessment
Code that *looks* dead but may be referenced via:
- Function pointers in dispatch tables (`modes.c`, `commands.c`)
- Conditional compilation (#ifdef paths not currently active)
- External tools or build scripts
Flag these as "verify before deleting."

### 5.5 Prioritized Action Plan
Ordered list of recommended actions by impact/effort ratio.
```

## Important Notes

- This is an **embedded firmware** — some functions may appear uncalled but are invoked via function pointer tables (`_mode` struct dispatch tables in `modes.c`, command handler registrations in `commands.c`). Always check these dispatch tables before flagging a function as dead.
- The `src/commands.c` file registers command handlers — a function listed there is NOT dead even if `grep` finds no direct call.  
- Similarly, `src/modes.c` registers mode functions via struct initialization.
- PIO (Programmable IO) `.pio` files generate headers — don't flag generated PIO headers as orphaned.
- Files in `build/generated/` are auto-generated and should be excluded from analysis.
- The `src/display/` and `src/toolbars/` directories contain LCD rendering code that runs on Core 1 — it may have unusual call patterns.
- Static functions that are used within their own file are NOT dead — only flag statics that are defined but never called within their own translation unit.
- Be thorough: use `grep_search` and `semantic_search` extensively. When uncertain, read the actual file to verify before flagging something as dead.
- **Do not make any code changes.** This is a research and reporting task only.
