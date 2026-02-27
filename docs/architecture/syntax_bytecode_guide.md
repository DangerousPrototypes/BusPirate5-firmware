+++
weight = 90406
title = 'Syntax & Bytecode Pipeline'
+++

# Syntax & Bytecode Pipeline

> Developer guide to the three-phase compile → execute → display pipeline that turns user syntax into protocol operations.

---

## Three-Phase Architecture

The syntax engine processes every interactive command through three sequential phases declared in [`src/syntax.h`](../src/syntax.h):

```c
SYNTAX_STATUS syntax_compile(void);  // Phase 1: Parse user input → bytecode
SYNTAX_STATUS syntax_run(void);      // Phase 2: Execute bytecode via mode handlers
SYNTAX_STATUS syntax_post(void);     // Phase 3: Format and display results
```

Each phase returns a `SYNTAX_STATUS`:

```c
typedef enum {
    SSTATUS_OK,
    SSTATUS_ERROR
} SYNTAX_STATUS;
```

**Phase 1 — Compile** ([`src/syntax_compile.c`](../src/syntax_compile.c)):
Parses the command line and emits an array of `struct _bytecode` instructions. Supports numbers in various formats (binary, hex, decimal), string literals, protocol operations, pin operations, and delays. Maximum 1024 bytecode instructions (`SYN_MAX_LENGTH`). Performs bounds checking and pin conflict detection.

**Phase 2 — Execute** ([`src/syntax_run.c`](../src/syntax_run.c)):
Walks the bytecode array and dispatches each instruction to the active protocol mode's handler. Results are written back into the bytecode structs (the `in_data`, `data_message`, and `error` fields). Execution stops on the first `SERR_ERROR`.

**Phase 3 — Post-process** ([`src/syntax_post.c`](../src/syntax_post.c)):
Iterates over the executed bytecode array and formats every result for the terminal, respecting `number_format` for display and printing any `data_message` or `error_message` strings.

## Pipeline Flow

```
User input string
        │
        ▼
 syntax_compile()          Phase 1 – parse & encode
        │
        ▼
 struct _bytecode[]        bytecode instruction array
        │
        ▼
  syntax_run()             Phase 2 – execute via mode handlers
        │
        ▼
 struct _bytecode[]        same array, now with results filled in
        │
        ▼
  syntax_post()            Phase 3 – format & display
        │
        ▼
 Terminal output
```

## The Bytecode Instruction Struct

Defined in [`src/bytecode.h`](../src/bytecode.h), this is the central data type of the pipeline:

```c
struct _bytecode {
    uint8_t number_format;  // Display format (binary, hex, decimal, ASCII)
    uint8_t command;        // Operation type (read, write, start, stop, etc)
    uint8_t error;          // Error severity level (SERR_NONE to SERR_ERROR)

    uint8_t read_with_write : 1;  // Read during write operation
    uint8_t has_bits : 1;         // Bit count explicitly specified
    uint8_t has_repeat : 1;       // Repeat count specified

    const char* error_message;  // Error description string
    const char* data_message;   // Data display message

    uint32_t bits;      // Bit count (0-32) or protocol-specific use
    uint32_t repeat;    // Repeat count or protocol-specific use
    uint32_t out_data;  // Data to transmit or protocol-specific use
    uint32_t in_data;   // Data received or protocol-specific use
};
```

A compile-time assertion enforces the size constraint:

```c
static_assert(
    sizeof(struct _bytecode) <= 28,
    "sizeof(struct _bytecode) has increased.  This will impact RAM.  Review to ensure this is not avoidable.");
```

### Field Reference

| Field | Type | Purpose |
|-------|------|---------|
| `number_format` | `uint8_t` | Display format: `df_bin`, `df_hex`, `df_dec`, `df_ascii` |
| `command` | `uint8_t` | Opcode (`SYN_WRITE`, `SYN_READ`, etc.) |
| `error` | `uint8_t` | Error severity (`SERR_NONE` to `SERR_ERROR`) |
| `read_with_write` | bit | Read during write operation |
| `has_bits` | bit | Bit count explicitly specified |
| `has_repeat` | bit | Repeat count specified |
| `error_message` | `const char*` | Static error description string |
| `data_message` | `const char*` | Static data display message (e.g. "ACK", "NACK") |
| `bits` | `uint32_t` | Bit count or protocol-specific |
| `repeat` | `uint32_t` | Repeat count or protocol-specific |
| `out_data` | `uint32_t` | Data to transmit |
| `in_data` | `uint32_t` | Data received |

> **Note:** The `uint32_t` fields are overloaded per protocol. For example, the HWLED protocol uses `out_data[23:0]` for RGB and `out_data[31:24]` for APA102 brightness.

## Opcodes

The `enum SYNTAX` in [`src/bytecode.h`](../src/bytecode.h) defines every operation the pipeline can encode:

```c
enum SYNTAX {
    SYN_WRITE = 0,
    SYN_READ,
    SYN_START,
    SYN_STOP,
    SYN_START_ALT,
    SYN_STOP_ALT,
    SYN_TICK_CLOCK,
    SYN_SET_CLK_HIGH,
    SYN_SET_CLK_LOW,
    SYN_SET_DAT_HIGH,
    SYN_SET_DAT_LOW,
    SYN_READ_DAT,
    SYN_DELAY_US,
    SYN_DELAY_MS,
    SYN_AUX_OUTPUT_HIGH,
    SYN_AUX_OUTPUT_LOW,
    SYN_AUX_INPUT,
    SYN_ADC,
};
```

### Opcode ↔ User Syntax Mapping

| Opcode | Value | User Syntax | Description |
|--------|-------|-------------|-------------|
| `SYN_WRITE` | 0 | `0x55`, `0b1010`, `85` | Write data |
| `SYN_READ` | 1 | `r` | Read data |
| `SYN_START` | 2 | `[` | Start condition |
| `SYN_STOP` | 3 | `]` | Stop condition |
| `SYN_START_ALT` | 4 | `{` | Alternate start (full duplex) |
| `SYN_STOP_ALT` | 5 | `}` | Alternate stop (full duplex) |
| `SYN_TICK_CLOCK` | 6 | `^` | Pulse clock |
| `SYN_SET_CLK_HIGH` | 7 | `-` | Set clock high |
| `SYN_SET_CLK_LOW` | 8 | `_` | Set clock low |
| `SYN_SET_DAT_HIGH` | 9 | `.` | Set data high |
| `SYN_SET_DAT_LOW` | 10 | `,` (in some contexts) | Set data low |
| `SYN_READ_DAT` | 11 | | Read data pin |
| `SYN_DELAY_US` | 12 | `&:N` | Delay microseconds |
| `SYN_DELAY_MS` | 13 | `&N` | Delay milliseconds |
| `SYN_AUX_OUTPUT_HIGH` | 14 | | Set auxiliary pin high |
| `SYN_AUX_OUTPUT_LOW` | 15 | | Set auxiliary pin low |
| `SYN_AUX_INPUT` | 16 | | Read auxiliary pin |
| `SYN_ADC` | 17 | | Read ADC |

## Error Severity Levels

```c
enum SYNTAX_ERRORS {
    SERR_NONE = 0,
    SERR_DEBUG,
    SERR_INFO,
    SERR_WARN,
    SERR_ERROR
};
```

| Code | Behavior |
|------|----------|
| `SERR_NONE` | No error |
| `SERR_DEBUG` | Display message, continue |
| `SERR_INFO` | Display message, continue |
| `SERR_WARN` | Display message, continue |
| `SERR_ERROR` | Display message, **halt execution** |

## Critical Rule: No printf() During Execute Phase

During Phase 2 (`syntax_run`), mode handler functions **must not** call `printf()` or produce any direct terminal output. The execute phase is designed to be side-effect-free with respect to I/O; all results are communicated through the `struct _bytecode` result fields:

- **`result->data_message`** — text decoration shown alongside the value (e.g. "ACK", "NACK")
- **`result->error`** and **`result->error_message`** — signal and describe errors
- **`result->in_data`** — read-back value from the protocol

All display formatting happens later in Phase 3 (`syntax_post`). This separation keeps the pipeline deterministic and allows the post-processor to apply consistent formatting.

### Example: Write Handler from `src/mode/dummy1.c`

```c
void dummy1_write(struct _bytecode* result, struct _bytecode* next) {
    static const char message[] = "--DUMMY1- write()";

    // your code
    for (uint8_t i = 0; i < 8; i++) {
        // user data is in result->out_data
        bio_put(BIO5, result->out_data & (0b1 << i));
    }

    // example error
    static const char err[] = "Halting: 0xff entered";
    if (result->out_data == 0xff) {
        result->error = SERR_ERROR; // mode error halts execution
        result->error_message = err;
        return;
    }

    // Can add a text decoration if you like (optional)
    // This is for passing ACK/NACK for I2C mode and similar
    result->data_message = message;
}
```

Key points demonstrated:

1. **Read input** from `result->out_data`.
2. **Report errors** by setting `result->error = SERR_ERROR` and `result->error_message`, then `return`.
3. **Annotate output** by setting `result->data_message` (optional, used for ACK/NACK-style decorations).
4. **Never call `printf()`** — the post-processor handles all display.

## How Modes Plug In

Protocol modes provide function pointers that the pipeline calls during Phase 2. The dispatch table is defined in `src/modes.h`, and each mode registers its handlers in the global `modes[]` array:

| Mode Function Pointer | Called For |
|----------------------|------------|
| `.protocol_write` | `SYN_WRITE` |
| `.protocol_read` | `SYN_READ` |
| `.protocol_start` | `SYN_START` |
| `.protocol_stop` | `SYN_STOP` |

When `syntax_run()` encounters a `SYN_WRITE` instruction, it calls the active mode's `.protocol_write(result, next)` with a pointer to the current bytecode and the next bytecode in the sequence. The `next` pointer allows look-ahead behavior (e.g., I2C uses it to decide ACK vs NACK on the last read before a stop condition).

For a complete walkthrough of implementing these handlers in a new mode, see [new_mode_guide.md](new_mode_guide.md) — specifically Step 7 (implementing protocol handlers) and Step 13 (testing syntax operations).

## Related Documentation

- [new_mode_guide.md](new_mode_guide.md) — Mode syntax handler implementation
- [new_command_guide.md](new_command_guide.md) — Adding new global commands
- [bp_cmd_data_types.md](bp_cmd_data_types.md) — Command argument data types
- [error_handling_reference.md](error_handling_reference.md) — Error handling patterns
- Source: [`src/bytecode.h`](../src/bytecode.h), [`src/syntax.h`](../src/syntax.h), [`src/syntax_compile.c`](../src/syntax_compile.c), [`src/syntax_run.c`](../src/syntax_run.c), [`src/syntax_post.c`](../src/syntax_post.c)
