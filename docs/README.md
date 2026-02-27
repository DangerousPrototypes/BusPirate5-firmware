# Bus Pirate 5 — Developer Documentation

Organized documentation for Bus Pirate 5 firmware development.

## Directory Layout

```
docs/
├── README.md                  ← you are here
├── licenses.md                ← open-source license summary
│
├── guides/                    ← How-to guides for developers
│   ├── new_command_guide.md       — Creating a new global/mode command
│   ├── new_mode_guide.md         — Creating a new protocol mode
│   ├── new_toolbar_guide.md      — Creating a bottom-of-screen toolbar
│   ├── binary_mode_guide.md      — Implementing a binary (USB) mode
│   ├── display_mode_guide.md     — Adding a display mode
│   ├── bio_pin_guide.md          — Buffered I/O pin subsystem
│   ├── storage_guide.md          — Flash/SD storage and config persistence
│   ├── system_monitor_guide.md   — Voltage, ADC, PSU monitoring
│   ├── build_system_guide.md     — CMake build system & platform targets
│   ├── translation_guide.md      — Localization / translation system
│   └── contributing.md           — Contributor rules and conventions
│
├── architecture/              ← System design and internals
│   ├── board_abstraction_guide.md — Hardware variants & pin mapping
│   ├── dual_core_guide.md         — RP2040/RP2350 dual-core design
│   ├── syntax_bytecode_guide.md   — User syntax compile→execute pipeline
│   └── usb_communication_guide.md — USB interfaces, FIFOs, data flow
│
├── api/                       ← API and type references
│   ├── bp_cmd_parsing_api.md      — Command-line parsing API
│   ├── bp_cmd_data_types.md       — bp_cmd.h type reference
│   ├── bp_cmd_patterns.md         — bp_cmd cookbook / recipes
│   ├── bp_cmd_prompting.md        — Interactive prompting API
│   ├── bp_cmd_linenoise.md        — Hint & completion bridge
│   ├── error_handling_reference.md — Error signaling patterns
│   └── system_config_reference.md  — system_config struct reference
│
├── testing/                   ← Testing documentation
│   ├── testing_guide.md           — Host-side unit testing
│   ├── hil_test_rig.md            — Hardware-in-the-loop test harness
│   └── test_fixture_pipeline.md   — HIL pipeline architecture
│
├── analysis/                  ← One-off analysis reports & reviews
│   ├── code_review.md                 — Copilot code review report
│   ├── library_integration_plan.md    — Library integration roadmap
│   ├── dead_code_analysis.md          — Dead/orphan code findings
│   ├── args_parse_migration.md        — args_parse migration scratchpad
│   ├── arg_has_arg_migration.md       — has_arg usage catalog
│   ├── vt100_toolbar_analysis.md      — VT100/toolbar system analysis
│   ├── vt100_toolbar_status.md        — Post-refactor toolbar status
│   ├── device_evaluation_report.md    — Hardware evaluation findings
│   └── translation_report.txt         — Unused translation entries
│
├── meta/                      ← Doc outlines and planning
│   ├── developer_docs_outline.md      — Full docs structure plan
│   └── bp_cmd_developer_docs_outline.md — bp_cmd docs plan
│
└── third_party_licenses/      ← License texts for bundled libraries
```

LLM agent prompts live in [`.github/prompts/`](../.github/prompts/) (not human docs).

## Quick Links

**New to the codebase?** Start with:
1. [Contributing](guides/contributing.md)
2. [Build System](guides/build_system_guide.md)
3. [New Command Guide](guides/new_command_guide.md)

**Working on commands?** See the [bp_cmd API](api/bp_cmd_parsing_api.md) and [patterns cookbook](api/bp_cmd_patterns.md).

**Running tests?** See [Testing Guide](testing/testing_guide.md) and [HIL Test Rig](testing/hil_test_rig.md).
