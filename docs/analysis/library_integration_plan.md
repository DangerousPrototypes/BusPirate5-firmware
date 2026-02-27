# Library Integration Plan for Bus Pirate Firmware

**Date:** January 31, 2026  
**Status:** Ready for Implementation

---

## Executive Summary

This document outlines a plan to integrate four recommended libraries into the Bus Pirate firmware. However, due to the unique architecture of the Bus Pirate user interface, several design decisions require additional information gathering before implementation.

---

## Key Bus Pirate Architecture Constraints

Before implementing any library, we must account for these unique aspects:

### 1. Unified Line Editing → Dispatch Model
```
┌─────────────────────────────────────────────────────────────┐
│                    User Input                                │
│                 (Same line editor for all)                   │
├─────────────────────────────────────────────────────────────┤
│                         │                                    │
│                    Line Editor                               │
│              (history, editing, completion)                  │
│                         │                                    │
│                    ┌────┴────┐                               │
│                    │ Dispatch │                              │
│                    └────┬────┘                               │
│           ┌─────────────┴─────────────┐                      │
│           ▼                           ▼                      │
│  Starts with {[]} or >        Otherwise (command)            │
│  ─────────────────────        ───────────────────            │
│  [0x55 r:5]                   m 1                            │
│  {0xA0 0x00 r:2]              W 3.3                          │
│  >0x55.8:10                   i2c scan                       │
│           │                           │                      │
│           ▼                           ▼                      │
│     syntax.c                    commands[]                   │
│   (compile+execute)           (dispatch to handler)          │
└─────────────────────────────────────────────────────────────┘
```

**Key insight:** Line editing is SHARED between syntax and command mode. 
Dispatch happens AFTER the complete line is received.

### 2. Memory Approach
- **Static allocation preferred** - avoid malloc/free where possible
- Reserved RAM pool: `reserve_for_future_mode_specific_allocations[10 * 1024]` (10KB) in `src/pirate.c` line 65
- This buffer can be reduced/repurposed as libraries are integrated
- Estimated need: ~6KB for linenoise with history (fits within 10KB reserve)

### 3. Dual-Core Architecture (RP2040/RP2350)
```
┌─────────────────────┐     ┌─────────────────────┐
│       CORE 0        │     │       CORE 1        │
├─────────────────────┤     ├─────────────────────┤
│ Command Processing  │     │ USB/UART Service    │
│ Mode Execution      │◄────│ RX FIFO Management  │
│ Syntax Execution    │     │ TX FIFO Management  │
│ PSU/ADC Control     │────►│ LCD Updates         │
│                     │     │ LED Animations      │
└─────────────────────┘     └─────────────────────┘
        │                           │
        └───────── Shared ──────────┘
              rx_fifo, tx_fifo
              system_config
              pin states
```

### 4. VT100/ANSI Terminal Features
- Status bar (separate from main output)
- Cursor positioning
- Color support
- Must coexist with any line editing library

---

## Design Decisions (Answered)

| Question | Answer |
|----------|--------|
| RAM constraints? | **No concern** - reserves available |
| Syntax mode line editing? | **Yes** - identical to command mode, shared functions |
| Dispatch logic? | After line complete: `{[]}` or `>` → syntax, else → command |
| History persistence? | **No** - RAM only |
| Tab completion scope? | **Commands and flags/arguments** |
| History shared? | **Yes** - single history for all input |

---

## Proposed Library Integration Order

Based on dependencies and risk assessment:

```
Phase 1: SPSC Queue (Foundation - Lowest Risk)
    │
    ▼
Phase 2: kstring (String Safety - Medium Risk)
    │
    ▼
Phase 3: ketopt (Argument Parsing - Medium Risk)
    │
    ▼
Phase 4: linenoise (Line Editing - Highest Risk/Reward)
```

---

## Phase 1: Lock-Free SPSC Queue

### Goal
Replace the disabled-spinlock queue with a proper lock-free implementation.

### Why First?
- Foundation for reliable inter-core communication
- Lowest risk - internal change, no API changes
- Fixes a critical bug without touching UI

### Recommended Implementation

**Option A: Pico SDK `pico_util/queue`**
```c
// Already in SDK, designed for RP2040
#include "pico/util/queue.h"

queue_t rx_queue;
queue_init(&rx_queue, sizeof(char), 128);

// Thread-safe by design
queue_try_add(&rx_queue, &c);
queue_try_remove(&rx_queue, &c);
```

**Option B: Custom SPSC Ring Buffer**
```c
// Minimal lock-free SPSC for known single-producer single-consumer
typedef struct {
    volatile uint32_t head;  // Written by producer only
    volatile uint32_t tail;  // Written by consumer only
    uint8_t buffer[QUEUE_SIZE];  // Power of 2
} spsc_queue_t;

// No locks needed - atomic operations + memory barriers
static inline bool spsc_try_push(spsc_queue_t* q, uint8_t data) {
    uint32_t head = q->head;
    uint32_t next = (head + 1) & (QUEUE_SIZE - 1);
    if (next == q->tail) return false;  // Full
    q->buffer[head] = data;
    __dmb();  // ARM memory barrier
    q->head = next;
    return true;
}
```

### Integration Steps
1. [ ] Create `src/spsc_queue.h` with static-allocation API
2. [ ] Add unit tests for single-core correctness
3. [ ] Test with both cores running
4. [ ] Replace `rx_fifo` and `tx_fifo`
5. [ ] Remove old queue.c or keep for non-SPSC uses

### Memory Impact
- Minimal: Same buffer sizes, slightly different structure

---

## Phase 2: kstring Safe String Operations

### Goal
Replace unsafe `strcpy`/`strcat`/`sprintf` with bounds-checked alternatives.

### Why Second?
- Prerequisite for safer command parsing
- Can be adopted incrementally
- No architectural changes

### Adaptation for No-Malloc

The standard kstring uses dynamic allocation. Create a static wrapper:

```c
// bp_string.h - Static allocation wrapper for kstring concepts

typedef struct {
    char* s;          // Points to external buffer
    size_t len;       // Current length
    size_t capacity;  // Buffer capacity (fixed)
} bp_string_t;

// Initialize with static buffer
#define BP_STRING_INIT(buf) { .s = (buf), .len = 0, .capacity = sizeof(buf) }

// Safe operations (never allocate)
int bp_string_cat(bp_string_t* str, const char* src);
int bp_string_catf(bp_string_t* str, const char* fmt, ...);
int bp_string_set(bp_string_t* str, const char* src);

// Example usage:
char buffer[64];
bp_string_t str = BP_STRING_INIT(buffer);
bp_string_cat(&str, "Hello ");
bp_string_catf(&str, "value: %d", 42);
// str.s now contains "Hello value: 42"
```

### Integration Steps
1. [ ] Create `src/lib/bp_string.h` and `bp_string.c`
2. [ ] Add comprehensive tests
3. [ ] Identify all unsafe string operations (grep for strcpy, strcat, sprintf)
4. [ ] Replace incrementally, starting with most critical paths
5. [ ] Update coding guidelines to require bp_string for new code

### Files to Update (High Priority)
- `src/commands/i2c/ddr5.c`
- `src/pirate/hw1wire_pio.c`
- `src/ui/ui_term.c`
- `src/binmode/bpio.c`

---

## Phase 3: ketopt Argument Parsing

### Goal
Standardize command argument parsing with clean, consistent API.

### Why Third?
- Builds on string safety from Phase 2
- Simplifies command implementations
- Prerequisite for cleaner linenoise integration

### Current Argument Parsing Analysis

```c
// Current approach (from ui_cmdln.c):
bool cmdln_args_find_flag(char flag);
bool cmdln_args_find_flag_uint32(char flag, command_var_t* arg, uint32_t* value);
bool cmdln_args_find_flag_string(char flag, command_var_t* arg, uint32_t max_len, char* str);
bool cmdln_args_uint32_by_position(uint32_t pos, uint32_t* value);
bool cmdln_args_float_by_position(uint32_t pos, float* value);
```

### Proposed ketopt Integration

ketopt is a single-header library (~200 lines). Adapt for static buffers:

```c
// bp_args.h - Argument parsing for Bus Pirate commands

#include "ketopt.h"

typedef struct {
    ketopt_t opt;
    int argc;
    char* argv[16];  // Static allocation, max 16 args
    char arg_buf[256];  // Storage for parsed arguments
} bp_args_t;

// Initialize from command buffer (no malloc)
void bp_args_init(bp_args_t* args, const char* cmdline);

// Parse with getopt-style options
// Example: "W -v 3.3 -c 500" 
//   bp_args_init(&args, cmdline);
//   while ((c = bp_args_getopt(&args, "v:c:d")) >= 0) {
//       switch (c) {
//           case 'v': voltage = atof(args.opt.arg); break;
//           case 'c': current = atoi(args.opt.arg); break;
//           case 'd': debug = true; break;
//       }
//   }
```

### Integration Steps
1. [ ] Add `src/lib/ketopt.h` (single header, MIT license)
2. [ ] Create `src/lib/bp_args.c` wrapper for static allocation
3. [ ] Create example command using new system
4. [ ] Document migration guide for existing commands
5. [ ] Migrate commands incrementally (priority: most complex first)

### Commands to Migrate First
- `W` (PSU) - has voltage, current, flags
- `G` (PWM) - has frequency, duty cycle, units
- `P` (Pullups) - has value, pins, direction flag
- `i2c`/`spi` subcommands

---

## Phase 4: linenoise Line Editing

### Goal
Replace custom line editing with linenoise for better UX.

### Why Last?
- Highest risk - touches core UI loop
- Benefits from all previous phases
- Most visible improvement

### Critical Adaptation Requirements

#### 1. Static Allocation Mode
linenoise uses malloc internally. Options:

**Option A: Pre-allocated Pool**
```c
// Override malloc/free for linenoise
#define LINENOISE_MALLOC(sz) bp_pool_alloc(&linenoise_pool, sz)
#define LINENOISE_FREE(ptr) bp_pool_free(&linenoise_pool, ptr)

static uint8_t linenoise_pool_memory[4096];
static bp_pool_t linenoise_pool;
```

**Option B: Fully Static Fork**
Create a modified linenoise that uses only static buffers:
```c
// Static linenoise configuration
#define LINENOISE_MAX_LINE 256
#define LINENOISE_HISTORY_MAX 20
#define LINENOISE_COMPLETIONS_MAX 16

static char linenoise_buf[LINENOISE_MAX_LINE];
static char linenoise_history[LINENOISE_HISTORY_MAX][LINENOISE_MAX_LINE];
```

#### 2. Non-Blocking/Async Mode Integration
Bus Pirate needs to service USB/UART while waiting for input:

```c
// Current approach (simplified):
void core0_main_loop(void) {
    while(1) {
        // Wait for complete line
        get_user_input(cmdln.buf);
        
        // Process
        process_command();
    }
}

// With linenoise async API:
void core0_main_loop(void) {
    struct linenoiseState ls;
    char buf[256];
    linenoiseEditStart(&ls, -1, -1, buf, sizeof(buf), prompt);
    
    while(1) {
        // Non-blocking check for input
        if (input_available()) {
            char* line = linenoiseEditFeed(&ls);
            if (line != linenoiseEditMore) {
                // Line complete
                linenoiseEditStop(&ls);
                process_command(line);
                linenoiseEditStart(&ls, ...);  // Restart for next
            }
        }
        
        // Service other tasks (ADC monitoring, etc.)
        service_background_tasks();
    }
}
```

#### 3. Status Bar Coexistence
linenoise and status bar both manipulate cursor:

```c
// Before updating status bar:
linenoiseHide(&ls);

// Update status bar with VT100 sequences
update_status_bar();

// Restore linenoise display:
linenoiseShow(&ls);
```

#### 4. Unified Input → Post-Dispatch
Syntax and command mode share the SAME line editor:

```c
char* get_input(void) {
    // All input goes through linenoise
    char* line = linenoise(prompt);
    if (line == NULL) return NULL;
    
    // Add to shared history
    linenoiseHistoryAdd(line);
    
    return line;
}

void process_input(char* line) {
    // Dispatch based on first character(s)
    if (line[0] == '[' || line[0] == '{' || line[0] == '>') {
        // Syntax mode - compile and execute
        syntax_compile(line);
        syntax_run();
        syntax_post();
    } else {
        // Command mode - find and execute command
        dispatch_command(line);
    }
}
```

### Integration Steps
1. [ ] Fork linenoise, create `src/lib/linenoise/`
2. [ ] Modify for static allocation
3. [ ] Create BP-specific wrapper `src/ui/ui_linenoise.c`
4. [ ] Implement completion callback for commands
5. [ ] Implement hints callback for command help
6. [ ] Integrate with status bar system
7. [ ] Test all input scenarios:
   - [ ] Command mode
   - [ ] Syntax mode
   - [ ] Binary mode transitions
   - [ ] History navigation
   - [ ] Tab completion
8. [ ] Performance testing on RP2040

### Completion Implementation
```c
void bp_completion(const char* buf, linenoiseCompletions* lc) {
    size_t len = strlen(buf);
    
    // Find first space to determine if we're completing command or argument
    const char* space = strchr(buf, ' ');
    
    if (space == NULL) {
        // Completing command name
        for (int i = 0; i < commands_count; i++) {
            if (strncmp(buf, commands[i].command, len) == 0) {
                linenoiseAddCompletion(lc, commands[i].command);
            }
        }
        
        // Mode-specific commands
        if (system_config.mode != HIZ) {
            const struct _mode_command_struct* mode_cmds = modes[system_config.mode].mode_commands;
            uint32_t mode_cmds_count = modes[system_config.mode].mode_commands_count;
            for (int i = 0; i < mode_cmds_count; i++) {
                if (strncmp(buf, mode_cmds[i].command, len) == 0) {
                    linenoiseAddCompletion(lc, mode_cmds[i].command);
                }
            }
        }
    } else {
        // Completing argument/flag after command
        // Extract command name
        char cmd[16];
        size_t cmd_len = space - buf;
        if (cmd_len < sizeof(cmd)) {
            strncpy(cmd, buf, cmd_len);
            cmd[cmd_len] = '\0';
            
            // Find command and offer its flags
            // TODO: Commands need flag metadata for this to work
            // For now, could hardcode common patterns like:
            if (strcmp(cmd, "W") == 0) {
                // PSU command flags
                if (strstr(buf, "-v") == NULL) linenoiseAddCompletion(lc, "-v");
                if (strstr(buf, "-c") == NULL) linenoiseAddCompletion(lc, "-c");
            }
        }
    }
}
```

### Hints Implementation
```c
char* bp_hints(const char* buf, int* color, int* bold) {
    *color = 90;  // Gray
    *bold = 0;
    
    // Skip if line starts with syntax characters
    if (buf[0] == '[' || buf[0] == '{' || buf[0] == '>') {
        return NULL;  // No hints for syntax mode
    }
    
    // Find matching command (exact match)
    for (int i = 0; i < commands_count; i++) {
        size_t cmd_len = strlen(commands[i].command);
        if (strncmp(buf, commands[i].command, cmd_len) == 0 &&
            (buf[cmd_len] == '\0' || buf[cmd_len] == ' ')) {
            // Return usage hint from help_text if available
            if (commands[i].help_text) {
                return (char*)GET_T(commands[i].help_text);
            }
        }
    }
    return NULL;
}
```

---

## Memory Budget Estimate

| Component | Current | After Integration |
|-----------|---------|-------------------|
| SPSC Queues | ~512B | ~512B (same) |
| String Buffers | ~2KB | ~2KB (same) |
| Argument Parsing | ~1KB | ~1.5KB |
| Line Editing | ~2KB | ~6KB (with history) |
| **Total Delta** | - | **+3.5KB** |

*Note: These are rough estimates. Actual measurement needed.*

---

## Risk Assessment

| Phase | Risk | Mitigation |
|-------|------|------------|
| 1. SPSC Queue | Low | SDK has proven implementation |
| 2. kstring | Low | Wrapper approach, incremental adoption |
| 3. ketopt | Medium | May need command API changes |
| 4. linenoise | High | Core UI changes, static alloc challenges |

---

## Remaining Questions

1. **Binary Mode Transition**: How should line editing behave when switching to binary mode?

2. **Multi-line Input**: Is there ever a need for multi-line command input?

3. **Completion Context**: Should completion be context-aware?
   - e.g., after `i2c` show `scan`, `sniff`, etc.
   - e.g., after `W` show `-v`, `-c` flags

---

## Next Steps

1. [x] ~~Review this plan and provide feedback~~
2. [x] ~~Answer open questions~~ (RAM: OK, Shared editing: Yes, History: RAM only, Completion: commands+flags)
3. [ ] Prototype SPSC queue (Phase 1) - lowest risk starting point
4. [ ] Test linenoise in isolated environment with RP2040
5. [ ] Design completion callback structure for commands + flags

---

## Appendix: Library Licenses

| Library | License | Compatibility |
|---------|---------|---------------|
| linenoise | BSD-2-Clause | ✅ Compatible |
| klib/kstring | MIT | ✅ Compatible |
| ketopt | MIT | ✅ Compatible |
| Pico SDK queue | BSD-3-Clause | ✅ Compatible |

All libraries are compatible with Bus Pirate's MIT license.
