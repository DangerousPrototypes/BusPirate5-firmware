Find dead code and deprecation candidates

Read [](file:///home/ian/bp5fw/src/lib/bp_args/bp_args.h#1-1), lines 1 to 100

Read [](file:///home/ian/bp5fw/src/lib/bp_args/bp_args_compat.h#1-1), lines 1 to 100

Read [](file:///home/ian/bp5fw/src/ui/ui_help.h)

Read [](file:///home/ian/bp5fw/src/lib/bp_args/bp_args_compat.c#1-1), lines 1 to 200

Read [](file:///home/ian/bp5fw/src/lib/bp_args/bp_args_compat.c#200-200), lines 200 to 289

Read [](file:///home/ian/bp5fw/src/lib/bp_args/bp_args.h#100-100), lines 100 to 199

Searched for text `bp_args_init` (`**/src/**`), 6 results

Searched for text `bp_args_next` (`**/src/**`), 14 results

Searched for text `bp_args_positional` (`**/src/**`), 12 results

Searched for text `bp_args_get_string` (`**/src/**`), 4 results

Searched for text `bp_args_get_uint32` (`**/src/**`), 3 results

Searched for text `bp_args_get_int32` (`**/src/**`), 2 results

Searched for text `bp_args_get_float` (`**/src/**`), 2 results

Searched for text `bp_args_has_flag` (`**/src/**`), 2 results

Searched for text `bp_args_find_uint32` (`**/src/**`), 2 results

Searched for text `bp_args_find_string` (`**/src/**`), 2 results

Searched for text `bp_args_reset` (`**/src/**`), no results

Searched for regex `bp_arg_opt_t` (`**/src/**`), 20 results

Searched for regex `\bbp_args_t\b` (`**/src/**`), 20 results

Searched for regex `\bbp_arg_type_t\b` (`**/src/**`), 3 results

Searched for text `bp_compat_find_flag` (`**/src/**`), 9 results

Searched for text `bp_compat_find_flag_uint32` (`**/src/**`), 3 results

Searched for text `bp_compat_find_flag_string` (`**/src/**`), 3 results

Searched for text `bp_compat_uint32_by_position` (`**/src/**`), 3 results

Searched for text `bp_compat_float_by_position` (`**/src/**`), 3 results

Searched for text `bp_compat_string_by_position` (`**/src/**`), 3 results

Searched for text `bp_args_compat_init` (`**/src/**`), 1 result

Searched for text `cmdln_args_find_flag` (`**/src/**`), 115 results

Searched for text `cmdln_args_get_action` (`**/src/**`), 11 results

Searched for regex `\bcommand_var_t\b` (`**/src/**`), 44 results

Searched for text `ui_help_show` (`**/src/**`), 102 results

Searched for regex `\bui_help_options\b` (`**/src/**`), 80 results

Searched for regex `\bcmdln_action_t\b` (`**/src/**`), 14 results

Searched for text `ui_help_usage` (`**/src/**`), 3 results

Searched for text `ui_help_check_vout_vref` (`**/src/**`), 13 results

Searched for text `ui_help_sanity_check` (`**/src/**`), 20 results

Searched for text `ui_help_mode_commands` (`**/src/**`), 20 results

Searched for text `ui_help_error` (`**/src/**`), 9 results

Searched for text `cmdln_args_find_flag_float` (`**/src/**`), no results

Searched for text `cmdln_args_uint32_by_position` (`**/src/**`), 17 results

Searched for text `cmdln_args_float_by_position` (`**/src/**`), 7 results

Searched for text `cmdln_args_string_by_position` (`**/src/**`), 20 results

Read [](file:///home/ian/bp5fw/src/lib/bp_args/bp_args_compat.h#100-100), lines 100 to 127

Read [](file:///home/ian/bp5fw/src/ui/ui_cmdln.h#1-1), lines 1 to 100

Read [](file:///home/ian/bp5fw/src/ui/ui_cmdln.h#100-100), lines 100 to 131

Searched for text `cmdln_find_next_command` (`**/src/**`), 8 results

Searched for text `ui_help_mode_commands_exec` (`**/src/**`), 4 results

Read [](file:///home/ian/bp5fw/src/ui/ui_help.c)

Searched for text `cmdln_args_remainder` (`**/src/**`), 3 results

Searched for text `bp_args_find_flag` (`**/src/**`), no results

Searched for text `bp_args_find_flag_uint32` (`**/src/**`), no results

Searched for text `bp_args_find_flag_int32` (`**/src/**`), no results

Searched for text `bp_args_find_flag_float` (`**/src/**`), no results

Searched for text `bp_args_find_flag_string` (`**/src/**`), no results

Searched for text `bp_args_find_action` (`**/src/**`), no results

Searched for text `ui_help_sanity_check` (`**/src/**`), 26 results

Searched for regex `\b_command_info_t\b` (`**/src/**`), 6 results

Searched for text `bp_args_compat_any_flag` (`**/src/**`), 1 result

Read [](file:///home/ian/bp5fw/src/lib/bp_args/bp_cmd.h#1-1), lines 1 to 50

Read [](file:///home/ian/bp5fw/src/lib/bp_args/bp_cmd.h#50-50), lines 50 to 247

Searched for regex `bp_cmd_` (`**/src/**`), 41 results

Searched for regex `bp_command_def_t|bp_command_opt_t|bp_command_action_t` (`**/src/**`), 42 results

Read [](file:///home/ian/bp5fw/src/lib/bp_args/bp_cmd.c#1-1), lines 1 to 70

Searched for regex `bp_args_|bp_arg_opt_t|bp_arg_type_t|bp_args_t` (`**/src/lib/bp_args/bp_cmd.c`), no results

Searched for regex `bp_args|bp_cmd|cmdln_args|ui_help` (`**/tests/**`), no results

Searched for text `bp_args_positional_uint32` (`**/src/**`), 2 results

Searched for text `bp_args_positional_float` (`**/src/**`), 2 results

Searched for text `bp_args_positional_string` (`**/src/**`), 2 results

Read [](file:///home/ian/bp5fw/src/lib/bp_args/bp_args.h)

Good. Now let me verify the critical dependency — bp_cmd.h includes bp_args.h only for `bp_arg_type_t`: 

Searched for regex `bp_args\.h|bp_arg_type_t|BP_ARG_NONE|BP_ARG_REQUIRED|BP_ARG_OPTIONAL` (`**/src/lib/bp_args/bp_cmd.h`), 6 results

Now let me also quickly check ui_help.h for completeness: 

Read [](file:///home/ian/bp5fw/src/ui/ui_help.h#1-1), lines 1 to 60

Read [](file:///home/ian/bp5fw/src/ui/ui_help.h#60-60), lines 60 to 79

Here's the full analysis:

---

## Dead Code — Fully Unused (safe to remove now)

### 1. bp_args.c / bp_args.h — The getopt-loop parser (~700 lines combined)

**100% dead.** Not a single production command file calls any function from this module. The only callers are:
- Internal (within bp_args.c itself)
- bp_args_compat.c (which is also dead — see below)

**One dependency to preserve**: the `bp_arg_type_t` enum (`BP_ARG_NONE` / `BP_ARG_REQUIRED` / `BP_ARG_OPTIONAL`) is actively used by bp_cmd.h. Before removing bp_args.h, that enum needs to be moved into bp_cmd.h directly.

### 2. bp_args_compat.c / bp_args_compat.h — The compat bridge (~420 lines combined)

**100% dead.** The `bp_compat_*` wrapper functions (`bp_compat_find_flag`, `bp_compat_find_flag_uint32`, etc.) have zero callers anywhere. The `#define` macros that would have redirected old API → compat wrappers are all commented out. `bp_args_compat_any_flag[]` is declared `extern` but never even defined.

---

## Deprecation Candidates — Active but replaceable after full migration

Once all ~50 command files are converted to `bp_cmd_*()`, these can be removed:

| Module | Symbol | Call sites | Files | Replacement |
|--------|--------|-----------|-------|-------------|
| `ui/ui_cmdln` | `cmdln_args_find_flag()` | ~40 | ~25 | `bp_cmd_find_flag()` |
| `ui/ui_cmdln` | `cmdln_args_find_flag_uint32()` | ~28 | ~16 | `bp_cmd_get_uint32()` |
| `ui/ui_cmdln` | `cmdln_args_find_flag_string()` | ~13 | ~11 | `bp_cmd_get_string()` |
| `ui/ui_cmdln` | `cmdln_args_get_action()` | 9 | 9 | `bp_cmd_get_action()` |
| `ui/ui_cmdln` | `cmdln_args_uint32_by_position()` | 13 | 12 | `bp_cmd_get_uint32()` (positional) |
| `ui/ui_cmdln` | `cmdln_args_float_by_position()` | 3 | 2 | `bp_cmd_get_float()` |
| `ui/ui_cmdln` | `cmdln_args_string_by_position()` | ~18 | ~13 | `bp_cmd_get_string()` (positional) |
| `ui/ui_cmdln` | `cmdln_args_remainder()` | 1 | 1 | (custom) |
| `ui/ui_cmdln` | `command_var_t` (type) | ~26 | ~19 | eliminated entirely |
| `ui/ui_cmdln` | `struct cmdln_action_t` | 12 | 10 | `bp_command_action_t` |
| `ui/ui_help` | `ui_help_show()` | ~95 | ~50 | `bp_cmd_help_check()` / `bp_cmd_help_show()` |
| `ui/ui_help` | `ui_help_options()` (func) | internal only | 1 | absorbed into `bp_cmd_help_show()` |
| `ui/ui_help` | `ui_help_usage()` | internal only | 1 | absorbed into `bp_cmd_help_show()` |
| `ui/ui_help` | `struct ui_help_options` (type) | ~55 | ~50 | `bp_command_opt_t` + `bp_command_action_t` |

### Functions in ui_help.h that are **NOT** deprecation candidates (keep):

| Symbol | Why keep |
|--------|---------|
| `ui_help_check_vout_vref()` | Hardware sanity check — orthogonal to arg parsing |
| `ui_help_sanity_check()` | Hardware sanity check — orthogonal |
| `ui_help_mode_commands()` | Displays mode command list in help — could be enhanced but not replaced |
| `ui_help_mode_commands_exec()` | Mode command dispatch — different concern |
| `ui_help_error()` | Error display — different concern |

### Note on positional argument support

The new `bp_cmd` system currently has no explicit "get positional arg by index" API. Old calls to `cmdln_args_uint32_by_position()`, `cmdln_args_string_by_position()`, and `cmdln_args_float_by_position()` (~34 sites across ~27 files) would need either:
- A new `bp_cmd_get_positional_uint32(def, index, &val)` function added to `bp_cmd`, or
- Those commands restructured to use flags instead of positional args

---

## Summary

| Category | Lines | Action |
|----------|-------|--------|
| Dead code (bp_args + compat) | **~1,117** | Can delete now (move `bp_arg_type_t` to bp_cmd.h first) |
| Old API to deprecate after migration | **~250** call sites across **~50** files | Migrate file-by-file to `bp_cmd_*()`, then delete `ui_cmdln` arg functions + `ui_help_show`/`ui_help_options` |