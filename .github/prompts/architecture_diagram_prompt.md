# Bus Pirate Firmware Architecture Diagrams — Agent Prompt

## Purpose

Generate ASCII architecture diagrams of the Bus Pirate 5/6/7 firmware. Produce one
high-level overview showing all major subsystems and their relationships, then a
detailed diagram for each subsystem with accompanying text explaining basic
functioning. Use box-drawing and arrow characters similar to the style below:

```
   ┌──────────┐       ┌──────────┐
   │  Block A  │──────▶│  Block B  │
   └──────────┘       └──────────┘
        │
        ▼
   ┌──────────┐
   │  Block C  │
   └──────────┘
```

Prefer `│ ─ ┌ ┐ └ ┘ ├ ┤ ┬ ┴ ┼ ▶ ▼ ◀ ▲` characters. Keep each diagram under ~80
columns where possible. Label every arrow with the mechanism or data that flows
along it (e.g. "SPSC queue", "SPI", "function call", "ICM FIFO").

---

## 1. High-Level System Overview

Draw a single diagram showing all major subsystems and which core they run on.
Include these blocks and their primary interconnections:

### Blocks to include

| Block | Core | Key files |
|---|---|---|
| USB (TinyUSB) | Core 1 | `tusb_config.h`, `usb_descriptors.c` |
| Terminal RX/TX FIFOs | Shared | `pirate/tx_fifo.c`, `pirate/rx_fifo.c` |
| Binary Mode RX/TX FIFOs | Shared | `binmode/` |
| Command Processor | Core 0 | `ui/ui_process.c`, `commands/` |
| Syntax Pipeline (compile → run → post) | Core 0 | `syntax.c`, `bytecode.c`, `bio.c` |
| Mode Dispatch (protocol handlers) | Core 0 | `mode/`, `modes.c` |
| Hardware Abstraction | Both | `pirate/bio.c`, `pirate/amux.c`, `pirate/psu.c`, PIO drivers |
| System Monitor (ADC snapshot) | Core 1 | `system_monitor.c` |
| LCD Display | Core 1 | `display/`, `pirate/lcd.c`, `ui/ui_lcd.c` |
| VT100 Toolbar / Statusbar | Core 1 | `ui/ui_statusbar.c`, `toolbars/`, `ui/ui_toolbar.c` |
| Storage (NAND / TF) | Core 0 | `pirate/storage.c`, `nand/`, `fatfs/` |
| RGB LEDs | Core 1 (IRQ) | `pirate/rgb.c` |
| Linenoise (line editor) | Core 0 | `lib/bp_linenoise/` |
| Translation | Core 0 | `translation/` |
| System Config | Shared | `system_config.c/.h` |

### Connections to show

- USB ↔ SPSC queues ↔ Core 0 (terminal path)
- USB ↔ SPSC queues ↔ Core 0 (binary mode path)
- Command processor → syntax pipeline → mode dispatch → HW abstraction → PIO/GPIO
- System monitor → LCD update + VT100 toolbar rendering
- `amux_sweep()` feeds both monitor snapshot and `ui_pin_render` (via `hw_pin_voltage_ordered[]`)
- Inter-core messaging (ICM multicore FIFO) for synchronization
- SPI mutex shared between cores (LCD on core 1, flash on core 0)
- Storage ↔ system_config (load/save config.bp, mode .bp files)

### Accompanying text

Explain the dual-core split: Core 0 owns the user-facing command loop (read input →
parse → execute → display results). Core 1 owns USB servicing, LCD refresh, and
toolbar rendering. The two cores share data through lock-free SPSC queues (terminal
and binary mode), a multicore FIFO for synchronous ICM messages, an SPI bus mutex,
and the `system_config` struct.

---

## 2. Dual-Core Execution Loops

Draw two parallel columns showing the Core 0 and Core 1 infinite loops side by side
with the inter-core mechanisms between them.

### Core 0 loop (`core0_infinite_loop` in `pirate.c`)

State machine with four states:
1. `BP_SM_DISPLAY_MODE` — one-time VT100 color mode prompt
2. `BP_SM_GET_INPUT` — feed linenoise, call `display_periodic()` + `protocol_periodic()`
3. `BP_SM_PROCESS_COMMAND` — dispatch to command handler or syntax pipeline
4. `BP_SM_COMMAND_PROMPT` — build and display the mode prompt (e.g. `SPI>`)

Also runs `service_mode_periodic_core0()` cooperatively each iteration.

### Core 1 loop (`core1_infinite_loop` in `pirate.c`)

Sequential tasks every iteration:
1. `tud_task()` — TinyUSB device stack
2. `tud_cdc_rx_task()` — read USB CDC → SPSC queues
3. `tx_fifo_service()` — drain terminal TX queue → USB
4. `toolbar_core1_service()` — toolbar state machine (idle → rendering → draining)
5. `bin_tx_fifo_service()` — drain binary TX queue → USB
6. PSU fuse/voltage error polling
7. On timer tick (~500 ms): `monitor_update()`, LCD update, toolbar begin update

### Inter-core mechanisms to label

| Mechanism | Direction | Purpose |
|---|---|---|
| `rx_fifo` | Core 1 → Core 0 | Terminal input characters |
| `tx_fifo` | Core 0 → Core 1 | Terminal output characters |
| `bin_rx_fifo` | Core 1 → Core 0 | Binary mode input |
| `bin_tx_fifo` | Core 0 → Core 1 | Binary mode output |
| ICM (multicore FIFO) | Both directions | Sync messages (init, force update, enable/disable) |
| SPI mutex | Both | Shared SPI0 bus (LCD writes vs flash access) |
| `system_config` | Shared struct | Config flags, mode state, display settings |

### Accompanying text

The two cores never call each other's functions directly. All communication uses
lock-free queues or the hardware multicore FIFO. The SPI bus is the one shared
peripheral — Core 0 uses it for NAND flash storage, Core 1 uses it for LCD writes.
A mutex (`spi_busy_wait()`) serializes access. The LCD update timer fires on Core 1
at ~2 Hz (500 ms default) and triggers the monitor → LCD → toolbar update chain.

---

## 3. Command Processing Pipeline

Draw the flow from user keypress to command execution and output.

### Stages

```
USB CDC → rx_fifo → linenoise (line editor) → ui_process_commands()
                                                     │
                                         ┌───────────┴───────────┐
                                         ▼                       ▼
                                  Command table match     Syntax detected
                                  (commands_global[])     (starts with [ > etc.)
                                         │                       │
                                         ▼                       ▼
                                  cmd_handler()           syntax_compile()
                                  (direct execution)      → struct _bytecode[]
                                         │                       │
                                         │                       ▼
                                         │               syntax_run()
                                         │               (mode dispatch, no printf)
                                         │                       │
                                         │                       ▼
                                         │               syntax_post()
                                         │               (format results)
                                         │                       │
                                         └───────┬───────────────┘
                                                 ▼
                                           tx_fifo → USB CDC → terminal
```

### Accompanying text

User input arrives via USB CDC on Core 1, crosses to Core 0 through `rx_fifo`, and
feeds into the linenoise line editor. When the user presses Enter, `ui_process_commands()`
checks whether the input matches a global or mode-specific command, or contains syntax
tokens (like `[`, `]`, hex values). Commands execute directly via their handler function.
Syntax input goes through a three-phase pipeline:

1. **Compile** (`syntax.c`): Parse tokens into up to 1024 `_bytecode` instructions
   (28 bytes each). Supports hex (`0x55`), binary (`0b1010`), decimal, strings,
   protocol operations (`[`, `]`, `r`, `^`), delays (`&`), and repeats (`:N`).

2. **Run** (`bytecode.c`): Walk the bytecode array and dispatch each opcode to the
   active mode's handler via function pointers (e.g. `protocol_write()`,
   `protocol_start()`). **No printf allowed** — results stored in bytecode fields
   (`in_data`, `data_message`, `error`).

3. **Post** (`bio.c`/`syntax.c`): Format results for terminal display, respecting
   the user's chosen number format (hex, decimal, binary, ASCII).

---

## 4. Mode / Protocol System

Draw the mode dispatch architecture showing how the `_mode` struct's function
pointers connect syntax opcodes to hardware.

### Key elements

- `modes[]` array in `modes.c` — indexed by mode enum
- `_mode` struct with ~25 function pointers
- Syntax opcodes mapped to function pointers:
  - `SYN_START` → `protocol_start()` (i.e. I2C start condition, SPI CS low)
  - `SYN_STOP` → `protocol_stop()`
  - `SYN_WRITE` → `protocol_write()` (send byte, get response)
  - `SYN_READ` → `protocol_read()`
  - `SYN_TICK_CLOCK` → `protocol_tick_clock()`
  - etc.
- Mode-specific command tables (`mode_commands[]`)
- Setup flow: `m` command → `protocol_setup()` (interactive config) → `protocol_setup_exc()` (hardware init)
- PIO programs behind many modes (I2C, SPI, UART, 1-Wire, etc.)

### Available modes

HiZ (default), 1-Wire, UART, Half-Duplex UART, I2C, SPI, 2-Wire, 3-Wire,
LED (WS2812/APA102), DIO, Infrared, JTAG.

### Accompanying text

The Bus Pirate uses a dispatch-table pattern for protocol modes. Each mode implements
the `_mode` struct: a collection of function pointers that map protocol operations to
hardware actions. When the user types `[0x55 r]`, the syntax pipeline compiles this
to three bytecodes (`SYN_START`, `SYN_WRITE 0x55`, `SYN_READ`, `SYN_STOP`), and
`syntax_run()` calls the active mode's corresponding function pointer for each. This
design lets the same syntax work across I2C, SPI, UART, and all other protocols.

Most modes use RP2040/RP2350 PIO state machines for cycle-accurate protocol timing.
The PIO programs are in `.pio` files compiled by pioasm at build time.

Mode-specific commands (like `flash` for SPI or `scan` for I2C) are registered via
`mode_commands[]` in each mode's struct and searched after global commands.

---

## 5. Display and UI System

Draw the display update pipeline covering both LCD and VT100 terminal paths.

### Data flow

```
hw_adc_raw[] / hw_pin_voltage_ordered[]
        │
        ▼
   amux_sweep()                          (called by monitor_update on Core 1)
        │
        ▼
   monitor_snapshot_t                    (numeric ADC cache)
        │
        ├──────────────────────────────────────────────────┐
        ▼                                                  ▼
   LCD path                                         VT100 path
   ui_lcd_update(flags)                         toolbar_core1_begin_update(flags)
   ├─ reads snapshot                            ├─ toolbar_core1_service() state machine
   ├─ consumer-side shadow                      │   (TB_C1_IDLE → RENDERING → DRAINING)
   │   lcd_shadow_voltage_mv[]                  ├─ statusbar_update_core1_cb()
   ├─ per-digit diff → SPI writes               │   └─ ui_pin_render_values()
   └─ lcd_write_labels()                        │       ├─ reads hw_pin_voltage_ordered[]
                                                │       ├─ shadow_voltage_mv[] diff
                                                │       └─ snprintf to buffer
                                                └─ tx_tb_buf[1024] → tx_fifo → USB
```

### Toolbar architecture

The toolbar system is a registry of stackable bottom-of-screen panels:
- `toolbar_activate()` registers a toolbar with `draw()`, `update_core1()`, `destroy()`
- The statusbar is always the bottommost (anchored)
- Other toolbars stack above: logic analyzer bar, pin watcher, sys stats
- Core 1 renders each registered toolbar sequentially into `tx_tb_buf[]`
- VT100 escape sequences position the cursor and set scroll regions

### LCD display modes

Display modes implement the `_display` struct:
- `display_lcd_update(flags)` — called from Core 1 on timer tick
- Default display: pin labels + voltages (in `ui_lcd.c`)
- Scope display: oscilloscope waveform (in `scope.c`)
- Disabled: screen off

### `update_flags` bitmask

| Flag | Triggers |
|---|---|
| `UI_UPDATE_FORCE` | Full repaint (all cells) |
| `UI_UPDATE_ALL` | Redraw everything |
| `UI_UPDATE_LABELS` | Pin labels/function names changed |
| `UI_UPDATE_VOLTAGES` | Voltage readings changed |
| `UI_UPDATE_CURRENT` | Current reading changed |
| `UI_UPDATE_INFOBAR` | PSU/pullup/scope status changed |

### Accompanying text

Display updates run entirely on Core 1, triggered by a hardware timer (~500 ms).
The update cycle starts with `monitor_update()` which calls `amux_sweep()` to read
all ADC channels, then stores millivolt values in a `monitor_snapshot_t`. Two
independent consumers then render from this data:

**LCD path:** Reads the snapshot and compares each pin's millivolt value against a
local shadow array. For changed pins, it extracts individual digits and compares them
against the shadow's digits to minimize SPI traffic — each `lcd_write_labels()` call
pushes pixel data over SPI, so skipping unchanged digits saves significant bandwidth.

**VT100 path:** The toolbar state machine iterates registered toolbars. The statusbar
calls `ui_pin_render_values()` which reads `hw_pin_voltage_ordered[]` directly,
compares against its own `shadow_voltage_mv[]`, and writes only changed cells to a
buffer using VT100 tab-skip for unchanged positions. The buffer drains to USB via
`tx_fifo`.

Both paths do their own consumer-side change tracking. The monitor itself does NOT
track per-consumer dirty state — it only provides a stable numeric snapshot.

---

## 6. USB Communication System

Draw the USB data flow showing both terminal and binary mode paths, plus mass storage.

### Interfaces

| USB Interface | Type | Purpose |
|---|---|---|
| CDC 0 | Serial | Terminal (command line) |
| CDC 1 | Serial | Binary protocol (SUMP, BPIO, etc.) |
| MSC | Mass Storage | NAND/TF card file access |

### Data flow

```
┌─────────────────────────────────────────────────────────────┐
│                        USB Host                             │
└──────┬──────────────────┬──────────────────┬────────────────┘
       │ CDC 0            │ CDC 1            │ MSC
       │ (terminal)       │ (binary)         │ (storage)
       ▼                  ▼                  ▼
┌──────────────────────────────────────────────────────────────┐
│                  TinyUSB Device Stack (Core 1)               │
└──────┬──────────────────┬──────────────────┬────────────────┘
       │                  │                  │
  ┌────┴────┐        ┌────┴────┐        ┌────┴────┐
  │ rx_fifo │        │bin_rx_  │        │  FatFS  │
  │ tx_fifo │        │bin_tx_  │        │  + NAND │
  │ (SPSC)  │        │fifo     │        │  or TF  │
  └────┬────┘        └────┬────┘        └─────────┘
       │                  │
       ▼                  ▼
   Core 0             Core 0
   Command            Binary mode
   Processor          Handler
```

### Accompanying text

The Bus Pirate presents three USB interfaces to the host. CDC 0 is the interactive
terminal — characters flow through lock-free SPSC ring buffers (`rx_fifo` for input,
`tx_fifo` for output) between Core 1 (USB stack) and Core 0 (command processor).
CDC 1 carries binary protocol data (SUMP logic analyzer, BPIO FlatBuffers protocol,
legacy binary modes) through a parallel pair of SPSC queues.

The MSC (Mass Storage Class) interface exposes the onboard NAND flash (or TF card on
Rev 8) as a USB drive. The FatFS library provides the FAT filesystem, with the Dhara
flash translation layer handling wear leveling on NAND. Storage operations happen on
Core 0 and require the SPI mutex when accessing the flash chip.

All USB device-stack processing (`tud_task()`) runs on Core 1. The SPSC queues are
carefully designed to be lock-free: Core 1 is always the producer for RX and consumer
for TX, Core 0 is the opposite. No mutex is needed for queue operations.

Debug input can also arrive via UART (`debug_uart.c`) or SEGGER RTT (`rtt.c`),
which feed into the same `rx_fifo` as USB CDC.

---

## 7. Hardware Abstraction Layer

Draw the relationship between high-level subsystems and the hardware drivers.

### Key hardware blocks

```
┌─────────────────────────────────────────────────────────────┐
│                     RP2040 / RP2350                         │
│  ┌──────┐  ┌──────┐  ┌──────┐  ┌──────┐  ┌──────┐         │
│  │ GPIO │  │ ADC  │  │ SPI0 │  │ SPI1 │  │ PIO  │         │
│  │      │  │(4ch) │  │(bus) │  │(LCD) │  │(0,1) │         │
│  └──┬───┘  └──┬───┘  └──┬───┘  └──┬───┘  └──┬───┘         │
│     │         │         │         │         │               │
└─────┼─────────┼─────────┼─────────┼─────────┼───────────────┘
      │         │         │         │         │
      ▼         ▼         ▼         ▼         ▼
  ┌──────┐  ┌──────┐  ┌──────┐  ┌──────┐  ┌──────────────┐
  │bio.c │  │amux.c│  │Shared│  │lcd.c │  │PIO drivers   │
  │BIO0-7│  │AMUX  │  │SPI   │  │IPS   │  │hwi2c_pio.c   │
  │pins  │  │sweep │  │mutex │  │240×  │  │hwspi_pio.c   │
  │      │  │      │  │      │  │320   │  │hwuart_pio.c  │
  └──┬───┘  └──┬───┘  └──┬───┘  └──────┘  │hw1wire_pio.c │
     │         │         │                 │ws2812_pio.c  │
     │         │    ┌────┴────┐            │logicanalyzer │
     │         │    │         │            └──────────────┘
     │         │    ▼         ▼
     │         │  NAND     74HC595
     │         │  flash    shift reg
     │         │  (storage) (buf dir)
     ▼         ▼
  Buffer    Analog
  direction MUX
  control   (4051)
  + pullups
```

### Pin architecture

The Bus Pirate has 8 buffered I/O pins (BIO0–BIO7). Each pin passes through:
1. **Direction buffer** — controlled by shift register (BP5/XL) or GPIO (BP6/7)
2. **Voltage divider** — for ADC measurement via analog MUX (4051)
3. **Pull-up/down resistor** — switchable

The analog MUX (4051) multiplexes 8 pin voltages plus current sense and Vout
measurement through a single ADC input. `amux_sweep()` cycles through all channels.

### Platform abstraction

Each board revision has a platform header (`bpi5-rev10.h`, `bpi6-rev2.h`, etc.) that
defines pin numbers, ADC channels, SPI assignments, LCD dimensions, and feature flags
(`BP_HW_STORAGE_NAND`, `BP_HW_PSU_PWM`, `BP_HW_IOEXP_595`, etc.). The build system
selects the platform via CMake target.

### Accompanying text

The hardware abstraction layer in `src/pirate/` provides clean APIs over the RP2040/
RP2350 peripherals. The most critical abstraction is the buffered I/O system (`bio.c`):
all 8 user-facing pins go through external direction buffers and voltage dividers.
`bio_output(pin)` / `bio_input(pin)` configure direction, `bio_put()` / `bio_get()`
write and read.

The analog subsystem (`amux.c`) sweeps an external 4051 MUX to read all pin voltages
plus current sense through the MCU's limited ADC channels. `amux_sweep()` is called
by the system monitor on Core 1 and independently by the `v` command on Core 0.

Protocol modes use PIO (Programmable I/O) state machines for precise timing. Each
protocol has a `.pio` assembly file compiled by pioasm at build time. The PIO drivers
handle the bit-level protocol details while the mode layer manages framing and
configuration.

The SPI0 peripheral is shared: Core 0 uses it for NAND flash storage access, Core 1
uses it for LCD updates. A mutex (`spi_busy_wait()`) prevents collisions.

---

## 8. Storage System

Draw the storage stack from user commands down to flash hardware.

### Storage stack

```
   User commands (ls, cat, rm, format, etc.)
        │
        ▼
   storage.c  ←── config load/save (JSON .bp files)
        │          mode config descriptors (mode_config_t)
        │          mjson library for parse/emit
        ▼
   FatFS (fatfs/)
   FAT16/FAT32 filesystem
        │
        ├──────────────────────┐
        ▼                      ▼
   Dhara FTL (dhara/)       SD/MMC SPI
   NAND flash translation   (Rev 8 TF card only)
   wear leveling + GC
        │
        ▼
   NAND driver (nand/)
   page read/write/erase
        │
        ▼
   SPI0 (shared bus, mutex)
```

### Config persistence

Mode configs are saved as JSON files on the FAT filesystem:
- `config.bp` — global system config (display, color, language, etc.)
- `bpspi.bp`, `bpi2c.bp`, `bpuart.bp`, etc. — per-mode settings

Each mode defines a `mode_config_t` array mapping JSON paths (like `"$.speed"`) to
`uint32_t*` pointers in its static config struct. The `mjson` library handles
serialization.

### Accompanying text

The storage system supports two backends: NAND flash (Rev 9+, XL, BP6, BP7) and
TF/microSD card (Rev 8 only), selected by the `BP_HW_STORAGE_NAND` /
`BP_HW_STORAGE_TFCARD` feature flags.

For NAND flash, the Dhara flash translation layer provides wear leveling and garbage
collection over raw NAND pages. FatFS sits on top, providing a standard FAT
filesystem. The USB MSC interface also accesses FatFS, allowing the Bus Pirate to
appear as a USB drive.

Configuration persistence uses JSON files. When a user changes mode settings (e.g.
SPI speed, I2C address size), the mode's `mode_config_t` descriptor maps these values
to JSON paths. `storage_save_mode()` serializes the config struct to a `.bp` file,
and `storage_load_mode()` restores it on next boot or mode entry.

All flash access happens on Core 0 and uses SPI0, which requires the SPI mutex to
avoid collision with Core 1's LCD updates.

---

## 9. Syntax / Bytecode Pipeline

Draw the three-phase pipeline in detail showing the bytecode structure.

### Pipeline phases

```
   User input: "[0x68 0x55 r:4]"
        │
        ▼
   ┌─────────────────────────────────────────────┐
   │  Phase 1: syntax_compile()  (syntax.c)      │
   │                                              │
   │  Tokenize → parse → emit bytecodes          │
   │  Max 1024 instructions                      │
   │                                              │
   │  Output: struct _bytecode array:             │
   │  ┌────────┬─────────┬──────┬────────┐        │
   │  │SYN_    │out_data │bits  │repeat  │        │
   │  │START   │   —     │ 0   │  0     │        │
   │  ├────────┼─────────┼──────┼────────┤        │
   │  │SYN_    │ 0x68    │ 8   │  1     │        │
   │  │WRITE   │         │      │        │        │
   │  ├────────┼─────────┼──────┼────────┤        │
   │  │SYN_    │ 0x55    │ 8   │  1     │        │
   │  │WRITE   │         │      │        │        │
   │  ├────────┼─────────┼──────┼────────┤        │
   │  │SYN_    │   —     │ 8   │  4     │        │
   │  │READ    │         │      │        │        │
   │  ├────────┼─────────┼──────┼────────┤        │
   │  │SYN_    │   —     │ 0   │  0     │        │
   │  │STOP    │         │      │        │        │
   │  └────────┴─────────┴──────┴────────┘        │
   └─────────────────────┬───────────────────────┘
                         │
                         ▼
   ┌─────────────────────────────────────────────┐
   │  Phase 2: syntax_run()  (bytecode.c)        │
   │                                              │
   │  For each bytecode:                          │
   │    dispatch to modes[active].protocol_*()    │
   │    e.g. SYN_WRITE → protocol_write(0x68)     │
   │         SYN_READ  → protocol_read()          │
   │                                              │
   │  *** No printf during this phase ***         │
   │  Results stored in bytecode fields:          │
   │    .in_data, .data_message, .error           │
   │                                              │
   │  Hardware PIO executes protocol timing       │
   └─────────────────────┬───────────────────────┘
                         │
                         ▼
   ┌─────────────────────────────────────────────┐
   │  Phase 3: syntax_post()  (bio.c/syntax.c)   │
   │                                              │
   │  Format results per number_format setting:   │
   │    hex: 0x68 ACK 0x55 ACK 0xAB 0xCD ...     │
   │    dec: 104 ACK 85 ACK 171 205 ...           │
   │    bin: 0b01101000 ACK ...                   │
   │                                              │
   │  Output → tx_fifo → USB → terminal           │
   └─────────────────────────────────────────────┘
```

### Bytecode struct (28 bytes)

```c
struct _bytecode {
    uint8_t  number_format;     // HEX, DEC, BIN, ASCII
    uint8_t  command;           // SYN_WRITE, SYN_READ, SYN_START, etc.
    uint8_t  error;             // SERR_NONE, SERR_WARN, SERR_ERROR
    uint8_t  read_with_write:1; // simultaneous read+write (SPI full duplex)
    uint8_t  has_bits:1;        // custom bit width specified
    uint8_t  has_repeat:1;      // repeat count specified
    const char* error_message;
    const char* data_message;
    uint32_t bits;              // bit width (default 8)
    uint32_t repeat;            // repeat count
    uint32_t out_data;          // data to send
    uint32_t in_data;           // data received
};
```

### Accompanying text

The syntax pipeline is the heart of the Bus Pirate's protocol interaction. When the
user types a command like `[0x68 r:4]`, it flows through three strictly separated
phases:

**Compile** parses the input string into a flat array of bytecode instructions. Each
token maps to an opcode: `[` → `SYN_START`, `0x68` → `SYN_WRITE`, `r` → `SYN_READ`,
`]` → `SYN_STOP`. The `:4` suffix sets the repeat count. The compiler also handles
bit-width specifiers, delays (`&`/`&:`), and string literals.

**Run** walks the bytecode array and dispatches each instruction to the active mode's
handler. This is the only phase that touches hardware — a `SYN_WRITE` on I2C mode
sends a byte via PIO, waits for ACK, and stores the result. The critical constraint:
**no terminal output during this phase**. All results go into the bytecode struct's
`in_data`, `error`, and `data_message` fields. This separation prevents I/O timing
from being disrupted by USB/terminal latency.

**Post** iterates the completed bytecodes and formats results for display. The user's
chosen number format (hex, decimal, binary, ASCII) controls the output representation.
Results flow through `tx_fifo` to the terminal.

---

## 10. Binary Mode System

Draw the binary mode architecture showing the dispatch table and available modes.

### Binary mode flow

```
   USB CDC 1 (binary interface)
        │
        ▼
   bin_rx_fifo (SPSC)          ◀── Core 1 receives
        │
        ▼
   Core 0: binmode_process()
        │
        ▼
   binmode_t dispatch table (binmode.c)
   ┌────────────────────────────────────┐
   │ SUMP         │ Logic analyzer     │
   │ DirtyProto   │ Legacy BP4 binary  │
   │ BPIO (FBuf)  │ FlatBuffers proto  │
   │ FALA         │ Follow-along LA    │
   │ Arduino      │ CH32V003 SWIO      │
   │ IRtoy        │ IR TX/RX           │
   │ Legacy4Third │ BP4 compatibility  │
   └────────────────────────────────────┘
        │
        ▼
   Protocol handlers → hardware
        │
        ▼
   bin_tx_fifo (SPSC)          ──▶ Core 1 transmits
        │
        ▼
   USB CDC 1 → host
```

### Accompanying text

Binary modes provide machine-readable protocol access for host software like Sigrok,
flashrom, or custom scripts. Each binary mode implements `binmode_setup()`,
`binmode_service()`, and `binmode_cleanup()`. The SUMP mode implements the standard
SUMP logic analyzer protocol for compatibility with Sigrok/PulseView. The BPIO mode
uses FlatBuffers for a structured binary protocol with sub-handlers per protocol
(SPI, I2C, UART). DirtyProto provides backward compatibility with Bus Pirate v4's
binary interface.

Binary modes can optionally lock the terminal (`lock_terminal = true`), preventing
command-line interaction while a binary session is active. Some modes auto-configure
PSU and pullups (`default_psu`, `default_pullups`). The button can be configured to
exit binary mode (`button_to_exit`).

---

## 11. Initialization Sequence

Draw the boot sequence as a numbered timeline.

### Boot flow

```
   Power on / Reset
        │
        ▼
   main() (pirate.c)
        │
        ▼
   main_system_initialization()
   ├─ 1.  tx_fifo_init() / rx_fifo_init()     SPSC queues
   ├─ 2.  mcu_detect_revision()                Hardware ID
   ├─ 3.  bio_init()                           Buffered I/O pins
   ├─ 4.  SPI0 init                            On-board peripherals bus
   ├─ 5.  shift_init()                         74HC595 shift register
   ├─ 6.  mutex_init()                         SPI bus mutex
   ├─ 7.  psucmd_init()                        PSU pins
   ├─ 8.  lcd_init()                           LCD pins
   ├─ 9.  amux_init()                          ADC / analog MUX
   ├─ 10. storage_init()                       Storage GPIO
   ├─ 11. pullups_init()                       Pull-up resistors
   ├─ 12. rgb_init()                           RGB LEDs
   ├─ 13. lcd_reset()                          LCD display reset
   ├─ 14. button_init()                        Button input
   ├─ 15. system_config_defaults()             Default config values
   ├─ 16. ui_init()                            Command buffers
   ├─ 17. monitor_init()                       ADC monitor
   ├─ 18. storage_mount() + load_config()      Mount FS, restore settings
   ├─ 19. translation_set()                    Set language
   ├─ 20. multicore_launch_core1()             ──▶ Core 1 starts
   ├─ 21. lcd_configure()                      LCD parameters
   ├─ 22. Splash screen                        Logo + version
   ├─ 23. bio_init() + psucmd_disable()        Safe pin state
   ├─ 24. ICM sync (BP_ICM_INIT_CORE1)         Wait for Core 1 ready
   ├─ 25. binmode_setup()                      Binary mode init
   └─ 26. Enter core0_infinite_loop()          ──▶ Main loop
```

### Core 1 initialization (parallel after step 20)

```
   core1_entry()
   ├─ core1_initialization()
   │   ├─ USB init (tud_init)
   │   ├─ Service mode check
   │   └─ LCD backlight on
   ├─ ICM reply (BP_ICM_INIT_CORE1)            ──▶ Core 0 unblocks
   └─ Enter core1_infinite_loop()              ──▶ USB + display loop
```

### Accompanying text

The boot sequence initializes hardware peripherals in dependency order: communication
queues first, then GPIO and bus infrastructure, then high-level subsystems. Storage
mount happens before Core 1 launch so that saved config can be applied (display
settings, language, mode defaults) before the LCD and USB stack start.

Core 1 launches at step 20 and runs its own initialization (USB stack, service mode
check) in parallel. The two cores synchronize via an ICM handshake at step 24 before
Core 0 enters its main loop.

The splash screen displays on the LCD immediately after configuration, giving visual
feedback during the remaining startup steps. By the time Core 0 enters
`core0_infinite_loop()`, all hardware is initialized and the terminal is ready for
input.

---

## Output format

For each section above, produce:
1. The ASCII diagram (in a fenced code block)
2. A paragraph or two of explanatory text below it
3. Any relevant file references (source files that implement the subsystem)

The diagrams should be accurate to the actual codebase architecture. Do not invent
subsystems or connections that don't exist. When in doubt about a detail, note it
with a `(?)` marker rather than guessing.
