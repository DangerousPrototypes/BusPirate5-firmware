# Bus Pirate Firmware Architecture Diagrams

> Comprehensive architecture reference for the Bus Pirate 5/6/7 firmware.
> Covers RP2040/RP2350 dual-core embedded C firmware.

---

## Table of Contents

1. [High-Level System Overview](#1-high-level-system-overview)
2. [Dual-Core Execution Loops](#2-dual-core-execution-loops)
3. [Command Processing Pipeline](#3-command-processing-pipeline)
4. [Mode / Protocol System](#4-mode--protocol-system)
5. [Display and UI System](#5-display-and-ui-system)
6. [USB Communication System](#6-usb-communication-system)
7. [Hardware Abstraction Layer](#7-hardware-abstraction-layer)
8. [Storage System](#8-storage-system)
9. [Syntax / Bytecode Pipeline](#9-syntax--bytecode-pipeline)
10. [Binary Mode System](#10-binary-mode-system)
11. [Initialization Sequence](#11-initialization-sequence)

---

## 1. High-Level System Overview

```
┌─────────────────────────────────────────────────────────────────────────┐
│                           USB Host (PC)                                │
└───────────┬──────────────────┬──────────────────┬───────────────────────┘
            │ CDC 0            │ CDC 1            │ MSC
            │ (terminal)       │ (binary)         │ (mass storage)
╔═══════════╪══════════════════╪══════════════════╪═══════════════════════╗
║           ▼                  ▼                  ▼      TinyUSB         ║
║  ┌─────────────────────────────────────────────────────────────┐       ║
║  │               USB Device Stack  (Core 1)                    │       ║
║  │               tusb_config.h, usb_descriptors.c              │       ║
║  └────┬──────────────┬──────────────────────────┬──────────────┘       ║
║       │              │                          │                      ║
║       ▼              ▼                          ▼                      ║
║  ┌─────────┐   ┌──────────┐               ┌─────────┐                 ║
║  │ rx_fifo │   │bin_rx_fifo│              │ FatFS   │                 ║
║  │ (SPSC)  │   │ (SPSC)   │               │ + NAND  │                 ║
║  │ C1→C0   │   │ C1→C0    │               └────┬────┘                 ║
║  └────┬────┘   └─────┬────┘                    │                      ║
║ ······│··············│·····  CORE BOUNDARY  ····│······················ ║
║       ▼              ▼                          ▼                      ║
║  ┌──────────┐  ┌───────────┐              ┌─────────┐                 ║
║  │Linenoise │  │ Binary    │              │Storage  │                 ║
║  │(line ed.)│  │ Mode Svc  │              │ (NAND/  │                 ║
║  │ Core 0   │  │ Core 0    │              │  TF)    │                 ║
║  └────┬─────┘  └───────────┘              └────┬────┘                 ║
║       ▼                                        │ load/save            ║
║  ┌─────────────────────┐    ┌──────────────────▼──────┐               ║
║  │  Command Processor  │    │   System Config         │               ║
║  │  ui/ui_process.c    │◀──▶│   system_config.c/.h    │               ║
║  │  commands/           │    │   (shared struct)       │               ║
║  └────────┬────────────┘    └─────────────────────────┘               ║
║           │ dispatch                                                   ║
║           ▼                                                            ║
║  ┌─────────────────────────────────────┐                               ║
║  │  Syntax Pipeline  (Core 0)          │                               ║
║  │  compile → run → post               │                               ║
║  │  syntax_compile.c, syntax_run.c,    │                               ║
║  │  syntax_post.c, bytecode.h          │                               ║
║  └────────┬────────────────────────────┘                               ║
║           │ protocol calls                                             ║
║           ▼                                                            ║
║  ┌─────────────────────────────────────┐                               ║
║  │  Mode Dispatch  (Core 0)            │                               ║
║  │  modes.c, mode/*                    │                               ║
║  │  _mode struct (function pointers)   │                               ║
║  └────────┬────────────────────────────┘                               ║
║           │ hw calls                                                   ║
║           ▼                                                            ║
║  ┌─────────────────────────────────────────────────────────────┐       ║
║  │  Hardware Abstraction  (Both Cores)                         │       ║
║  │  bio.c │ amux.c │ psu.c │ PIO drivers │ SPI mutex          │       ║
║  └────────┼────────┼───────┼─────────────┼─────────────────────┘       ║
║           │        │       │             │                             ║
║           ▼        ▼       ▼             ▼                             ║
║       GPIO/BIO   ADC    PSU FET    PIO 0/1 state machines              ║
║                                                                        ║
║  ┌──────────────────────────────────────────────┐                      ║
║  │  Core 1 Services                              │                     ║
║  │  ┌──────────────┐ ┌────────────┐ ┌─────────┐ │                     ║
║  │  │System Monitor│ │ LCD Display│ │RGB LEDs │ │                     ║
║  │  │monitor()     │ │ ui_lcd.c   │ │rgb.c    │ │                     ║
║  │  │amux_sweep()  │ │ lcd.c      │ │(IRQ)    │ │                     ║
║  │  └──────┬───────┘ └─────▲──────┘ └─────────┘ │                     ║
║  │         │ snapshot       │ update              │                     ║
║  │         └────────────────┘                     │                     ║
║  │  ┌──────────────────────────────────┐          │                     ║
║  │  │ VT100 Toolbar/Statusbar          │          │                     ║
║  │  │ ui_statusbar.c, ui_toolbar.c     │          │                     ║
║  │  └──────────────────────────────────┘          │                     ║
║  └────────────────────────────────────────────────┘                    ║
║                                                                        ║
║  ┌──────────────────────────────────────────────┐                      ║
║  │  Translation  (Core 0)                        │                     ║
║  │  translation/en-us.h, zh-cn.h, pl-pl.h        │                     ║
║  └──────────────────────────────────────────────┘                      ║
║                                                                        ║
║  Inter-Core Sync:  ICM (multicore FIFO) ◀──▶ intercore_helpers.h       ║
║                    SPI mutex (SPI0 shared bus)                          ║
║                    SPSC lock-free queues (usb_tx.c, usb_rx.c)          ║
╚════════════════════════════════════════════════════════════════════════╝
```

The Bus Pirate firmware runs on a dual-core RP2040 or RP2350 microcontroller.
**Core 0** owns the command loop: it reads user input via linenoise, dispatches
commands through the syntax pipeline, calls protocol mode handlers, and drives
the hardware abstraction layer. **Core 1** owns USB (TinyUSB device stack), LCD
display updates, VT100 toolbar rendering, RGB LED output, and the system
monitor that periodically samples voltages and currents via `amux_sweep()`.

The two cores communicate through four mechanisms: **SPSC lock-free queues**
(`rx_fifo`, `tx_fifo`, `bin_rx_fifo`, `bin_tx_fifo`) for USB data, the
**inter-core messaging (ICM)** system built on the RP2040 multicore FIFO
(`intercore_helpers.h`), a **SPI mutex** that arbitrates the shared SPI0 bus
between LCD access (Core 1) and flash/NAND access (Core 0), and the
**`system_config`** shared struct that both cores read.

**Key source files:**
`src/pirate.c`, `src/system_config.c`, `src/system_config.h`,
`src/usb_tx.c`, `src/usb_rx.c`, `src/system_monitor.c`,
`src/pirate/intercore_helpers.h`, `src/modes.c`

---

## 2. Dual-Core Execution Loops

### Core 0 State Machine

```
┌───────────────────────────────────────────────────────────────────┐
│                  core0_infinite_loop()  (pirate.c)                │
│                                                                   │
│  ┌─────────────────────┐                                          │
│  │ BP_SM_DISPLAY_MODE  │  Show VT100 color/mode prompt            │
│  └─────────┬───────────┘                                          │
│            │ prompt sent                                           │
│            ▼                                                       │
│  ┌──────────────────────────┐                                     │
│  │ BP_SM_DISPLAY_MODE_WAIT  │  Wait for terminal type selection   │
│  └─────────┬────────────────┘                                     │
│            │ selection received                                    │
│            ▼                                                       │
│  ┌──────────────────┐    service_mode_periodic_core0()            │
│  │ BP_SM_GET_INPUT   │◀─────────────────────────────┐             │
│  │  • linenoise()    │    display_periodic()         │             │
│  │  • protocol_      │    protocol_periodic()        │             │
│  │    periodic()     │                               │             │
│  └─────────┬─────────┘                               │             │
│            │ line ready (Enter)                       │             │
│            ▼                                          │             │
│  ┌───────────────────────┐                           │             │
│  │ BP_SM_PROCESS_COMMAND │                           │             │
│  │  ui_process_commands()│──▶ cmd handler            │             │
│  │                       │──▶ OR syntax pipeline     │             │
│  └─────────┬─────────────┘                           │             │
│            │ done                                     │             │
│            ▼                                          │             │
│  ┌────────────────────────┐                          │             │
│  │ BP_SM_COMMAND_PROMPT   │  Build mode prompt       │             │
│  │  (e.g. "SPI> ")       │──────────────────────────┘             │
│  └────────────────────────┘                                       │
└───────────────────────────────────────────────────────────────────┘
```

### Core 1 Task Loop

```
┌───────────────────────────────────────────────────────────────────┐
│                  core1_infinite_loop()  (pirate.c)                │
│                                                                   │
│  ┌──── Runs continuously ────────────────────────────────────┐    │
│  │                                                            │    │
│  │  1. tud_task()              TinyUSB device processing      │    │
│  │         │                                                  │    │
│  │  2. tud_cdc_rx_task()       USB CDC → rx_fifo / bin_rx     │    │
│  │         │                                                  │    │
│  │  3. tx_fifo_service()       tx_fifo → USB CDC 0 out        │    │
│  │         │                                                  │    │
│  │  4. bin_tx_fifo_service()   bin_tx_fifo → USB CDC 1 out    │    │
│  │         │                                                  │    │
│  │  5. toolbar_core1_service() Toolbar state machine          │    │
│  │         │                                                  │    │
│  │  6. PSU fuse/error poll     Check voltage/current faults   │    │
│  │         │                                                  │    │
│  │  ┌──── Every ~500ms (timer tick) ─────────────────────┐    │    │
│  │  │  7. monitor()            ADC snapshot via amux      │    │    │
│  │  │  8. LCD update           ui_lcd_update()            │    │    │
│  │  │  9. Toolbar begin        Trigger toolbar redraw     │    │    │
│  │  │ 10. RGB update           rgb_irq()                  │    │    │
│  │  └────────────────────────────────────────────────────┘    │    │
│  │                                                            │    │
│  │  ICM handler: process incoming intercore FIFO messages     │    │
│  └────────────────────────────────────────────────────────────┘    │
└───────────────────────────────────────────────────────────────────┘
```

### Inter-Core Communication Mechanisms

```
┌──────────────┐                              ┌──────────────┐
│   Core 0     │                              │   Core 1     │
│  (commands)  │                              │  (USB/UI)    │
│              │         rx_fifo (SPSC)       │              │
│  linenoise ◀─┼──────────────────────────────┼─ CDC RX      │
│              │                              │              │
│  printf ─────┼──────────────────────────────┼─▶ CDC TX     │
│              │         tx_fifo (SPSC)       │              │
│              │                              │              │
│  binmode ◀───┼──────────────────────────────┼─ CDC1 RX     │
│              │       bin_rx_fifo (SPSC)     │              │
│              │                              │              │
│  binmode ────┼──────────────────────────────┼─▶ CDC1 TX    │
│              │       bin_tx_fifo (SPSC)     │              │
│              │                              │              │
│         ◀════╪══════════════════════════════╪═▶            │
│              │    ICM (multicore FIFO)      │              │
│              │    intercore_helpers.h        │              │
│              │                              │              │
│  NAND/TF ────┼──── SPI mutex (SPI0) ───────┼─ LCD         │
│  (storage)   │                              │ (display)    │
│              │                              │              │
│         ◀────┼── system_config (shared) ────┼─▶            │
└──────────────┘                              └──────────────┘
```

### ICM Message Constants

| Constant                      | Value  | Direction   | Purpose                     |
|-------------------------------|--------|-------------|-----------------------------|
| `BP_ICM_INIT_CORE1`          | `0xA5` | C0 ↔ C1    | Core synchronization        |
| `BP_ICM_UPDATE_TOOLBARS`     | `0xC0` | C0 → C1    | Request toolbar redraw      |
| `BP_ICM_DISABLE_LCD_UPDATES` | `0xF0` | C0 → C1    | Pause LCD refresh           |
| `BP_ICM_ENABLE_LCD_UPDATES`  | `0xF1` | C0 → C1    | Resume LCD refresh          |
| `BP_ICM_FORCE_LCD_UPDATE`    | `0xF2` | C0 → C1    | Force immediate LCD redraw  |
| `BP_ICM_DISABLE_RGB_UPDATES` | `0xF3` | C0 → C1    | Pause RGB LED updates       |
| `BP_ICM_ENABLE_RGB_UPDATES`  | `0xF4` | C0 → C1    | Resume RGB LED updates      |

Core 0 runs a five-state state machine in `core0_infinite_loop()`. After
initial VT100 mode detection (`BP_SM_DISPLAY_MODE` / `_WAIT`), it enters
the main loop: `BP_SM_GET_INPUT` feeds characters to linenoise and
cooperatively calls `display_periodic()` and `protocol_periodic()`. When
a complete line is ready, the state advances to `BP_SM_PROCESS_COMMAND`
where `ui_process_commands()` dispatches to either a command handler or the
syntax pipeline. After execution, `BP_SM_COMMAND_PROMPT` rebuilds the
prompt string and returns to `BP_SM_GET_INPUT`.

Core 1 runs a tight polling loop. On every iteration it services TinyUSB,
drains/fills the SPSC queues, and runs the toolbar state machine. On a
periodic timer tick (~500 ms), it invokes `monitor()` to sample ADC values
via `amux_sweep()`, updates the LCD, and triggers toolbar and RGB refreshes.
ICM messages from Core 0 control whether LCD and RGB updates are enabled or
disabled — this is used during flash operations that need exclusive SPI0
access.

**Key source files:**
`src/pirate.c`, `src/usb_tx.c`, `src/usb_rx.c`,
`src/pirate/intercore_helpers.h`, `src/system_monitor.c`,
`src/ui/ui_toolbar.c`

---

## 3. Command Processing Pipeline

```
┌──────────┐    ┌──────────┐    ┌───────────────┐    ┌──────────────────┐
│ USB CDC  │───▶│ rx_fifo  │───▶│  Linenoise    │───▶│ ui_process_      │
│ (Core 1) │    │ (SPSC)   │    │  (line editor)│    │ commands()       │
└──────────┘    └──────────┘    │  lib/bp_      │    │ (ui/ui_process.c)│
                                │  linenoise/   │    └───────┬──────────┘
                                └───────────────┘            │
                                                    ┌────────┴─────────┐
                                                    │                  │
                                              ┌─────▼──────┐  ┌───────▼───────┐
                                              │  Command   │  │ Syntax token  │
                                              │  table     │  │ detected      │
                                              │  match     │  │ [ { > etc.    │
                                              └─────┬──────┘  └───────┬───────┘
                                                    │                 │
                                              ┌─────▼──────┐  ┌──────▼────────┐
                                              │cmd_handler │  │syntax_compile │
                                              │  (direct)  │  │ (bytecode gen)│
                                              └─────┬──────┘  └──────┬────────┘
                                                    │                 │
                                                    │          ┌──────▼────────┐
                                                    │          │ syntax_run    │
                                                    │          │ (execute BC)  │
                                                    │          └──────┬────────┘
                                                    │                 │
                                                    │          ┌──────▼────────┐
                                                    │          │ syntax_post   │
                                                    │          │ (format out)  │
                                                    │          └──────┬────────┘
                                                    │                 │
                                                    └────────┬────────┘
                                                             │
                                                    ┌────────▼────────┐
                                                    │    tx_fifo      │
                                                    │    (SPSC)       │
                                                    └────────┬────────┘
                                                             │
                                                    ┌────────▼────────┐
                                                    │   USB CDC out   │
                                                    │   (Core 1)      │
                                                    └─────────────────┘
```

User input arrives from USB CDC 0 on Core 1, which pushes bytes into the
`rx_fifo` SPSC queue. Core 0 feeds these bytes into linenoise, the line
editor that provides history, tab-completion, and editing. When the user
presses Enter, the completed line is handed to `ui_process_commands()` in
`src/ui/ui_process.c`.

The command processor first checks whether the input matches an entry in
the global command table (140+ commands organized by category). If a match
is found, the corresponding `cmd_handler()` is called directly. If the
input begins with a syntax token (`[`, `{`, `>`, etc.), it enters the
three-phase syntax pipeline: `syntax_compile()` converts the text into
bytecode instructions, `syntax_run()` executes them by calling protocol
mode function pointers, and `syntax_post()` formats the results for
display. All output is pushed into `tx_fifo`, which Core 1 drains to the
USB CDC endpoint.

**Key source files:**
`src/ui/ui_process.c`, `src/commands/commands.c`, `src/commands/commands.h`,
`src/syntax_compile.c`, `src/syntax_run.c`, `src/syntax_post.c`,
`src/bytecode.h`, `src/lib/bp_linenoise/`

---

## 4. Mode / Protocol System

```
┌─────────────────────────────────────────────────────────────────┐
│                    modes[] array  (modes.c)                      │
│                    Indexed by mode enum                           │
│                                                                  │
│  [0] HiZ    [1] 1-Wire  [2] UART   [3] HD-UART  [4] I2C        │
│  [5] SPI    [6] 2-Wire  [7] 3-Wire [8] LED      [9] DIO        │
│  [10] IR    [11] JTAG   [12] USB-PD [13] I2S     ...            │
└──────────────────────────┬──────────────────────────────────────┘
                           │ each entry is a _mode struct
                           ▼
┌─────────────────────────────────────────────────────────────────┐
│                      _mode struct  (modes.h)                     │
│                                                                  │
│  Protocol lifecycle:                                             │
│    protocol_setup()          Initial config (interactive)        │
│    protocol_setup_exc()      Apply config                        │
│    protocol_start()          Open bus / start                    │
│    protocol_start_alt()      Alternate start (e.g. {)            │
│    protocol_stop()           Close bus / stop                    │
│    protocol_stop_alt()       Alternate stop (e.g. })             │
│    protocol_cleanup()        Teardown                            │
│                                                                  │
│  Data transfer:                                                  │
│    protocol_write()          Send data                           │
│    protocol_read()           Receive data                        │
│    protocol_bitr()           Read single bit                     │
│                                                                  │
│  Pin/clock manipulation:                                         │
│    protocol_clkh()           Clock high                          │
│    protocol_clkl()           Clock low                           │
│    protocol_dath()           Data high                           │
│    protocol_datl()           Data low                            │
│    protocol_dats()           Data state (read)                   │
│    protocol_tick_clock()     Toggle clock                        │
│                                                                  │
│  Housekeeping:                                                   │
│    protocol_periodic()       Periodic service                    │
│    protocol_macro()          Macro execution                     │
│    protocol_wait_done()      Wait for async complete             │
│    protocol_get_speed()      Report bus speed                    │
│    protocol_preflight_sanity_check()  Pre-op validation          │
│    protocol_command()        Mode-specific command parser        │
│                                                                  │
│  UI:                                                             │
│    protocol_settings()       Display current settings            │
│    protocol_help()           Mode help text                      │
│    protocol_name[10]         Short name (e.g. "SPI")             │
│    mode_commands_count       Number of mode-specific commands    │
│    setup_def                 Setup definitions                   │
└─────────────────────────────────────────────────────────────────┘

┌─────────────────────────────────────────────────────────────────┐
│                    PIO State Machines                             │
│                                                                  │
│  Mode         PIO File            Protocol                       │
│  ─────────    ──────────────────  ─────────────────              │
│  1-Wire       hw1wire.pio         Dallas 1-Wire                  │
│  UART         hwuart.pio          Async serial                   │
│  I2C          hwi2c.pio           I2C master                     │
│  SPI          (hardware SPI1)     SPI master                     │
│  2-Wire       hw2wire.pio         Generic 2-wire                 │
│  3-Wire       hw3wire.pio         Generic 3-wire                 │
│  LED          ws2812.pio          WS2812B addressable LEDs       │
│               apa102.pio          APA102/SK9822 LEDs             │
│  IR           rc5.pio             RC5 decode                     │
│               irio.pio            IR modulation                  │
│  JTAG         (bit-bang / PIO)    IEEE 1149.1                    │
└─────────────────────────────────────────────────────────────────┘
```

The protocol mode system is the central extensibility mechanism of Bus Pirate
firmware. The `modes[]` array in `modes.c` holds one `_mode` struct per
supported protocol, indexed by a mode enum. Each struct contains ~20 function
pointers that the syntax engine and command processor call to interact with the
bus. This dispatch table pattern allows adding new protocols by implementing the
interface and registering in the array.

Most hardware protocols use the RP2040/RP2350 PIO (Programmable I/O)
subsystem for precise timing. Each PIO program is defined in a `.pio` file
that gets compiled into C headers at build time. The PIO state machines
handle the low-level signal timing while the mode C code manages framing,
addressing, and user interaction.

**Key source files:**
`src/modes.h`, `src/modes.c`, `src/mode/hw1wire.c`, `src/mode/hwuart.c`,
`src/mode/hwi2c.c`, `src/mode/hwspi.c`, `src/mode/hw2wire.c`,
`src/mode/hw3wire.c`, `src/mode/hwled.c`, `src/mode/dio.c`,
`src/mode/infrared.c`, `src/mode/jtag.c`

---

## 5. Display and UI System

```
┌──────────────────────────────────────────────────────────────────┐
│                      ADC Hardware                                 │
│                 hw_adc_raw[]  (4 channels)                        │
└──────────────────────┬───────────────────────────────────────────┘
                       │ raw samples
                       ▼
              ┌────────────────┐
              │  amux_sweep()  │  4051 MUX scan across BIO pins
              │  amux.c        │  Fills hw_pin_voltage_ordered[]
              └────────┬───────┘
                       │ voltage snapshot
                       ▼
              ┌────────────────┐
              │   monitor()    │  system_monitor.c
              │   (Core 1)     │  Converts to ASCII, tracks changes
              └───┬────────┬───┘
                  │        │
        ┌─────────▼─┐  ┌──▼──────────────────────────┐
        │  LCD path  │  │  VT100 Toolbar path          │
        │            │  │                              │
        ▼            │  ▼                              │
  ┌───────────┐      │  ┌──────────────────────────┐   │
  │ui_lcd_    │      │  │ toolbar_core1_service()  │   │
  │update()   │      │  │ (state machine)          │   │
  │(Core 1)   │      │  │                          │   │
  └─────┬─────┘      │  │  TB_C1_IDLE              │   │
        │            │  │    │ timer tick            │   │
        ▼            │  │    ▼                      │   │
  ┌───────────┐      │  │  TB_C1_RENDERING         │   │
  │ lcd.c     │      │  │    │ draw to tx_fifo      │   │
  │ IPS 240×  │      │  │    ▼                      │   │
  │ 320 SPI1  │      │  │  TB_C1_DRAINING          │   │
  └───────────┘      │  │    │ wait for USB flush   │   │
                     │  │    └──▶ TB_C1_IDLE        │   │
                     │  └──────────────────────────┘   │
                     │                                  │
                     │  ┌──────────────────────────┐   │
                     │  │ ui_statusbar.c            │   │
                     │  │ (pin labels, voltages,    │   │
                     │  │  current, mode info)      │   │
                     │  └──────────────────────────┘   │
                     │                                  │
                     └──────────────────────────────────┘
```

### Display Modes

```
┌─────────────────────────────────────────────────────┐
│              Display Mode Registry                   │
│                                                      │
│  ┌─────────────┐  ┌───────────┐  ┌──────────────┐  │
│  │  default.c  │  │  scope.c  │  │  disabled.c  │  │
│  │  Pin labels │  │  Oscillo- │  │  LCD off     │  │
│  │  + voltages │  │  scope    │  │              │  │
│  └─────────────┘  └───────────┘  └──────────────┘  │
└─────────────────────────────────────────────────────┘
```

### Toolbar Registry

```
toolbar_registry[TOOLBAR_MAX_COUNT]
  ┌──────────────────────────────────────────────────┐
  │ slot 0: bottom toolbar (statusbar)               │
  │ slot 1: ...                                      │
  │ slot 2: ...                                      │
  │ slot 3: ...                                      │
  └──────────────────────────────────────────────────┘

  toolbar_activate() → registers draw/update/destroy
                     → applies VT100 scroll region
                     → triggers full redraw
```

### Update Flags

| Flag                  | Purpose                              |
|-----------------------|--------------------------------------|
| `UI_UPDATE_FORCE`     | Force complete redraw                |
| `UI_UPDATE_ALL`       | Update all fields                    |
| `UI_UPDATE_LABELS`    | Pin label text changed               |
| `UI_UPDATE_VOLTAGES`  | Pin voltage readings changed         |
| `UI_UPDATE_CURRENT`   | PSU current reading changed          |
| `UI_UPDATE_INFOBAR`   | Info bar content changed             |

The display system has two output paths that share a common data source.
The `amux_sweep()` function scans the analog multiplexer across all BIO
pins, filling `hw_pin_voltage_ordered[]`. The `monitor()` function on
Core 1 reads this snapshot, converts values to ASCII strings, and tracks
which digits changed to generate minimal update masks.

The LCD path sends pixel data to the IPS 240×320 display over SPI1.
The VT100 toolbar path uses a three-state cooperative state machine
(`TB_C1_IDLE` → `TB_C1_RENDERING` → `TB_C1_DRAINING`) to render
ANSI-escaped status lines into the terminal TX FIFO without blocking
Core 1. The toolbar registry holds up to `TOOLBAR_MAX_COUNT` slots;
`toolbar_activate()` registers a toolbar's draw/update/destroy callbacks
and configures the VT100 scroll region.

**Key source files:**
`src/system_monitor.c`, `src/pirate/amux.c`, `src/pirate/lcd.c`,
`src/ui/ui_lcd.c`, `src/ui/ui_toolbar.c`, `src/ui/ui_statusbar.c`,
`src/display/default.c`, `src/display/scope.c`, `src/display/disabled.c`

---

## 6. USB Communication System

```
┌──────────────────────────────────────────────────────────────────┐
│                         USB Host (PC)                             │
└─────┬────────────────────┬───────────────────┬───────────────────┘
      │                    │                   │
      │ CDC 0              │ CDC 1             │ MSC
      │ (terminal)         │ (binary proto)    │ (mass storage)
      ▼                    ▼                   ▼
┌───────────────────────────────────────────────────────────────────┐
│                   TinyUSB Device Stack  (Core 1)                  │
│                   CFG_TUD_CDC = 2                                 │
│                   CFG_TUD_MSC = 1                                 │
│                   tusb_config.h, usb_descriptors.c                │
└─────┬────────────────────┬───────────────────┬───────────────────┘
      │                    │                   │
      ▼                    ▼                   ▼
┌───────────┐        ┌───────────┐       ┌───────────┐
│ CDC 0     │        │ CDC 1     │       │ MSC       │
│ Terminal  │        │ Binary    │       │ Storage   │
│           │        │ Protocol  │       │           │
│ rx_fifo   │        │bin_rx_fifo│       │ FatFS     │
│ (C1→C0)   │        │ (C1→C0)  │       │    │      │
│           │        │           │       │    ▼      │
│ tx_fifo   │        │bin_tx_fifo│       │ NAND/TF   │
│ (C0→C1)   │        │ (C0→C1)  │       │           │
└─────┬─────┘        └─────┬────┘       └───────────┘
      │                    │
      ▼                    ▼
  Core 0               Core 0
  linenoise            binmode
  + commands           service

┌───────────────────────────────────────────────────────────────────┐
│  Debug Input (optional)                                           │
│  ┌──────────────┐    ┌────────────────┐                          │
│  │  UART debug  │    │  SEGGER RTT    │                          │
│  │  (hardware)  │    │  (SWD probe)   │                          │
│  └──────────────┘    └────────────────┘                          │
└───────────────────────────────────────────────────────────────────┘
```

The USB subsystem exposes three interfaces to the host. **CDC 0** is the
interactive terminal — keystrokes flow through `rx_fifo` to Core 0's
linenoise editor, and responses flow back through `tx_fifo`. **CDC 1** is
the binary protocol interface used by client libraries (DirtyProto,
SUMP logic analyzer, etc.) via `bin_rx_fifo`/`bin_tx_fifo`. **MSC** exposes
the internal NAND flash or TF card as a USB mass storage device through
FatFS.

All FIFO queues are lock-free single-producer/single-consumer (SPSC)
ring buffers. Core 1 is always the USB-side producer/consumer; Core 0
is always the application-side producer/consumer. This avoids any need
for mutexes on the data path. Debug input can alternatively arrive via
a hardware UART or SEGGER RTT over an SWD probe.

**Key source files:**
`src/tusb_config.h`, `src/usb_descriptors.c`, `src/usb_tx.c`,
`src/usb_rx.c`, `src/binmode/`

---

## 7. Hardware Abstraction Layer

```
┌─────────────────────────────────────────────────────────────────┐
│                    Application Layer                              │
│         Mode drivers, commands, syntax engine                    │
└──────────┬──────────┬───────────┬──────────┬────────────────────┘
           │          │           │          │
     ┌─────▼────┐ ┌───▼───┐ ┌────▼───┐ ┌───▼────┐
     │  bio.c   │ │amux.c │ │ psu.c  │ │ PIO    │
     │ BIO0-7   │ │ 4051  │ │ PSU    │ │drivers │
     │ pin I/O  │ │ MUX   │ │ FET    │ │        │
     └─────┬────┘ └───┬───┘ └────┬───┘ └───┬────┘
           │          │          │          │
     ┌─────▼──────────▼──────────▼──────────▼────────────────────┐
     │               RP2040 / RP2350 Peripherals                  │
     │                                                            │
     │  ┌────────┐  ┌────────┐  ┌────────┐  ┌─────────────────┐ │
     │  │  GPIO  │  │  ADC   │  │  SPI   │  │      PIO        │ │
     │  │ 30 pins│  │ 4 ch   │  │        │  │  ┌───┐  ┌───┐  │ │
     │  │        │  │        │  │ SPI0   │  │  │SM0│  │SM0│  │ │
     │  │        │  │        │  │(shared)│  │  │SM1│  │SM1│  │ │
     │  │        │  │        │  │        │  │  │SM2│  │SM2│  │ │
     │  │        │  │        │  │ SPI1   │  │  │SM3│  │SM3│  │ │
     │  │        │  │        │  │ (LCD)  │  │  │PIO0│ │PIO1│ │ │
     │  └────────┘  └────────┘  └────┬───┘  │  └───┘  └───┘  │ │
     │                               │      └─────────────────┘ │
     │                          ┌────▼───┐                      │
     │                          │SPI     │                      │
     │                          │ mutex  │                      │
     │                          │C0: NAND│                      │
     │                          │C1: LCD │                      │
     │                          └────────┘                      │
     └────────────────────────────────────────────────────────────┘
```

### BIO Pin Architecture

```
              BIO Pin (BIO0–BIO7)
                     │
                     ▼
            ┌────────────────┐
            │ Direction      │
            │ Buffer (74LVC) │
            │ input/output   │
            └───────┬────────┘
                    │
           ┌────────┴────────┐
           │                 │
           ▼                 ▼
  ┌────────────────┐  ┌──────────────┐
  │ Voltage Divider│  │ Pull-up/     │
  │ (to ADC via    │  │ Pull-down    │
  │  4051 MUX)     │  │ resistors    │
  └────────────────┘  └──────────────┘
```

### Platform Header Mapping

| Board Target       | Platform Header     | Key Differences         |
|---------------------|--------------------|-----------------------------|
| Bus Pirate 5 rev8  | `bpi5-rev8.h`     | RP2040, original pinout     |
| Bus Pirate 5 rev10 | `bpi5-rev10.h`    | RP2040, updated pinout      |
| Bus Pirate 5 XL    | `bpi5xl-rev10.h`  | RP2350, extended I/O        |
| Bus Pirate 6       | `bpi6-rev2.h`     | RP2350, new board layout    |

The hardware abstraction layer isolates protocol mode drivers from the
specifics of the RP2040/RP2350 silicon. `bio.c` manages the eight buffered
I/O pins (BIO0–BIO7), handling direction control and voltage level
translation. `amux.c` drives the 4051 analog multiplexer to sweep ADC
readings across all pins — this feeds both the system monitor and the
`hw_pin_voltage_ordered[]` array used for display. `psu.c` controls the
programmable power supply FET and monitors current draw.

SPI0 is shared between Core 0 (NAND flash / TF card storage) and Core 1
(LCD display) via a mutex. SPI1 is dedicated to the LCD. PIO 0 and PIO 1
each provide four state machines that host the protocol-specific timing
programs (I2C, UART, 1-Wire, etc.). Platform headers define the pin
assignments and hardware capabilities for each board revision.

**Key source files:**
`src/pirate/bio.c`, `src/pirate/amux.c`, `src/pirate/psu.c`,
`src/pirate/lcd.c`, `src/platform/bpi5-rev10.c`,
`src/platform/bpi5-rev10.h`, `src/platform/bpi6-rev2.h`

---

## 8. Storage System

```
┌──────────────────────────────┐
│     User Commands            │
│  storage_save_config()       │
│  storage_load_config()       │
│  storage_save_mode()         │
│  storage_load_mode()         │
└──────────────┬───────────────┘
               │ JSON read/write
               ▼
┌──────────────────────────────┐
│     storage.c                │
│     mjson library            │
│     (JSON parse/emit)        │
└──────────────┬───────────────┘
               │ file I/O
               ▼
┌──────────────────────────────┐
│         FatFS                │
│     FAT12/16/32 filesystem   │
└──────┬───────────────┬───────┘
       │               │
       ▼               ▼
┌────────────┐   ┌───────────┐
│  NAND      │   │  TF Card  │
│  (Dhara    │   │  (SD/MMC  │
│   FTL)     │   │   SPI)    │
└──────┬─────┘   └─────┬─────┘
       │               │
       ▼               ▼
┌──────────────────────────────┐
│     SPI0 (shared bus)        │
│     (mutex protected)        │
└──────────────────────────────┘
```

### Configuration Files

```
bpconfig.bp          Global config (terminal, language, LED, etc.)
bpspi.bp             SPI mode saved settings
bpi2c.bp             I2C mode saved settings
bpuart.bp            UART mode saved settings
bp1wire.bp           1-Wire mode saved settings
...                  (one per mode)
```

All configuration is stored as JSON `.bp` files on the internal NAND flash
or removable TF card. The `mjson` library handles JSON parsing and emission.
`storage_save_config()` and `storage_load_config()` persist the global
`system_config` struct, while `storage_save_mode()` and
`storage_load_mode()` handle per-mode settings using tag-based JSON paths
(e.g., `"$.terminal_language"`).

The NAND flash uses the **Dhara** flash translation layer (FTL) to provide
wear leveling and bad-block management on top of raw NAND pages. The TF
card uses standard SD/MMC SPI protocol. Both storage backends connect
through FatFS and share SPI0, which is mutex-protected to coordinate with
Core 1's LCD access.

**Key source files:**
`src/pirate/storage.c`, `src/nand/`, `src/fatfs/`,
`src/lib/mjson/mjson.h`

---

## 9. Syntax / Bytecode Pipeline

```
                     User Input
                  "[ 0x55 r:4 ]"
                        │
                        ▼
┌────────────────────────────────────────────────────────┐
│  Phase 1: COMPILE  (syntax_compile.c)                  │
│                                                        │
│  Tokenize input string                                 │
│  Generate array of _bytecode structs                   │
│                                                        │
│  Input: "[ 0x55 r:4 ]"                                 │
│  Output: BC_START, BC_WRITE(0x55),                     │
│          BC_READ(repeat=4), BC_STOP                    │
└───────────────────────┬────────────────────────────────┘
                        │ _bytecode[]
                        ▼
┌────────────────────────────────────────────────────────┐
│  Phase 2: RUN  (syntax_run.c)                          │
│                                                        │
│  Walk bytecode array                                   │
│  Call mode function pointers:                          │
│    BC_START  → protocol_start()                        │
│    BC_WRITE  → protocol_write()                        │
│    BC_READ   → protocol_read()  (×repeat)              │
│    BC_STOP   → protocol_stop()                         │
│  Store results in _bytecode.in_data                    │
└───────────────────────┬────────────────────────────────┘
                        │ _bytecode[] with results
                        ▼
┌────────────────────────────────────────────────────────┐
│  Phase 3: POST  (syntax_post.c)                        │
│                                                        │
│  Format results for display                            │
│  Apply number_format (hex/dec/bin/ASCII)                │
│  Write to terminal via tx_fifo                         │
│                                                        │
│  Output: "TX: 0x55  RX: 0x01 0x02 0x03 0x04"          │
└────────────────────────────────────────────────────────┘
```

### `_bytecode` Struct (28 bytes)

```
struct _bytecode {
    ┌─────────────────────────────────────────┐
    │  number_format   (uint8_t)  display fmt │
    │  command         (uint8_t)  opcode      │
    │  error           (uint8_t)  error code  │
    │  read_with_write (1 bit)    duplex flag │
    │  has_bits        (1 bit)    bit count?  │
    │  has_repeat      (1 bit)    repeat?     │
    │  error_message   (char *)   error text  │
    │  data_message    (char *)   data label  │
    │  bits            (uint32_t) bit count   │
    │  repeat          (uint32_t) repeat cnt  │
    │  out_data        (uint32_t) TX payload  │
    │  in_data         (uint32_t) RX payload  │
    └─────────────────────────────────────────┘
};
```

The syntax pipeline converts human-readable bus commands into bytecode,
executes them, and formats results. **Phase 1** (`syntax_compile.c`)
tokenizes the input string and generates an array of `_bytecode` structs.
Each struct is 28 bytes and encodes one bus operation — a start condition,
data write, data read, delay, etc. — along with metadata like repeat
count, bit width, and number display format.

**Phase 2** (`syntax_run.c`) walks the bytecode array and dispatches each
instruction to the current protocol mode's function pointers. For example,
`BC_WRITE` calls `protocol_write()`, which in turn drives the PIO state
machine or bit-bangs the appropriate signals. Read results are stored back
into the `_bytecode.in_data` field.

**Phase 3** (`syntax_post.c`) formats the completed bytecode array for
terminal display, applying the user's preferred number format (hex,
decimal, binary, or ASCII) and writing the formatted output to `tx_fifo`.

**Key source files:**
`src/syntax_compile.c`, `src/syntax_run.c`, `src/syntax_post.c`,
`src/bytecode.h`

---

## 10. Binary Mode System

```
┌──────────────────────────────────────────────────────────────────┐
│                    USB CDC 1  (binary protocol)                   │
│                    bin_rx_fifo / bin_tx_fifo                      │
└──────────────────────────┬───────────────────────────────────────┘
                           │
                           ▼
┌──────────────────────────────────────────────────────────────────┐
│                    Binary Mode Dispatcher                         │
│                    binmode/binmode.c                              │
│                                                                  │
│  Selects active binmode via binmode_t struct:                    │
│                                                                  │
│  ┌──────────────┐  ┌──────────────┐  ┌──────────────────────┐   │
│  │  SUMP LA     │  │  DirtyProto  │  │  Arduino CH32V003    │   │
│  │  sump.c      │  │  (BPIO)      │  │  SWIO programmer     │   │
│  │  logic       │  │  bpio_*.c    │  │                      │   │
│  │  analyzer    │  │              │  │                      │   │
│  └──────────────┘  └──────────────┘  └──────────────────────┘   │
│                                                                  │
│  ┌──────────────┐  ┌──────────────┐  ┌──────────────────────┐   │
│  │ Legacy4Third │  │    FALA      │  │  IRToy / IRMan       │   │
│  │ legacy4      │  │  (fast LA)   │  │  irtoy-*.c           │   │
│  │ third.c      │  │  fala.c      │  │  IR transceiver      │   │
│  └──────────────┘  └──────────────┘  └──────────────────────┘   │
│                                                                  │
│  ┌──────────────┐                                               │
│  │ IRToy AIR    │                                               │
│  │ (advanced    │                                               │
│  │  IR remote)  │                                               │
│  └──────────────┘                                               │
└──────────────────────────────────────────────────────────────────┘
```

### `binmode_t` Struct

```
typedef struct {
    ┌──────────────────────────────────────────────┐
    │  lock_terminal     (bool)   Lock CDC 0       │
    │  can_save_config   (bool)   Persist settings │
    │  reset_to_hiz      (bool)   HiZ on exit      │
    │  pullup_enabled    (bool)   Pull-ups active   │
    │  button_to_exit    (bool)   Button exits mode │
    │  psu_en_voltage    (float)  PSU voltage       │
    │  psu_en_current    (float)  PSU current limit │
    │  binmode_name      (char*)  Display name      │
    │                                              │
    │  binmode_setup()     Init handler             │
    │  binmode_service()   Polling handler          │
    │  binmode_cleanup()   Teardown handler         │
    │  binmode_hook_mode_exc()  Mode change hook    │
    └──────────────────────────────────────────────┘
} binmode_t;
```

The binary mode system provides programmatic access to Bus Pirate hardware
over USB CDC 1. Each binary mode implements the `binmode_t` interface with
setup, service (polling), and cleanup callbacks. The dispatcher selects
the active mode and calls `binmode_service()` on each Core 0 iteration.

**SUMP** implements the Openbench Logic Analyzer protocol for use with
tools like PulseView/sigrok. **DirtyProto (BPIO)** uses FlatBuffers
(schema: `bpio.fbs`) for structured binary communication. **FALA** is a
fast logic analyzer mode. **Legacy4Third** provides backward compatibility
with Bus Pirate v3/v4 binary protocols. **IRToy/IRMan** modes emulate USB
infrared transceiver devices.

**Key source files:**
`src/binmode/binmode.c`, `src/binmode/sump.c`, `src/binmode/fala.c`,
`src/binmode/legacy4third.c`, `src/binmode/bpio_helpers.c`,
`src/binmode/irtoy-*.c`, `bpio.fbs`

---

## 11. Initialization Sequence

```
┌───────────────────────────────────────────────────────────────────┐
│                         BOOT (main())                             │
│                         pirate.c                                  │
└───────────────────────────┬───────────────────────────────────────┘
                            │
                            ▼
           main_system_initialization()
                            │
  ┌─────────────────────────┼──────────────────────────────────┐
  │  Step  │ Call                │ Purpose                      │
  │────────┼─────────────────────┼──────────────────────────────│
  │   1    │ tx_fifo_init()      │ Terminal TX queue             │
  │   2    │ rx_fifo_init()      │ Terminal RX queue             │
  │   3    │ bio_init()          │ Buffered I/O pins             │
  │   4    │ spi_init()          │ SPI0 peripheral               │
  │   5    │ shift_init()        │ 595 shift register (IOEXP)    │
  │   6    │ psucmd_init()       │ PSU control pins              │
  │   7    │ lcd_init()          │ LCD controller init           │
  │   8    │ lcd_reset()         │ LCD hardware reset            │
  │   9    │ amux_init()         │ Analog MUX + ADC              │
  │  10    │ storage_init()      │ NAND/TF pin setup             │
  │  11    │ pullups_init()      │ Pull-up resistors             │
  │  12    │ rgb_init()          │ RGB LED controller             │
  │  13    │ button_init()       │ User button                   │
  │  14    │ system_init()       │ System config defaults         │
  │  15    │ ui_init()           │ UI state machine               │
  │  16    │ monitor_init()      │ Voltage/current monitor        │
  │  17    │ storage_mount()     │ Mount filesystem               │
  │  18    │ storage_load_config │ Load bpconfig.bp               │
  │  19    │ lcd_configure()     │ Apply LCD settings             │
  │  20    │ multicore_launch_   │ ──── CORE 1 STARTS ────       │
  │        │   core1(core1_entry)│                                │
  │  21    │ binmode_setup()     │ Binary mode init               │
  └────────┴─────────────────────┴──────────────────────────────┘
                            │
                            ▼
  ┌─────────────────────────────────────────────────────────────┐
  │  Core 1 Entry (core1_entry)                                  │
  │                                                              │
  │  1. tusb_init()           Initialize TinyUSB                 │
  │  2. Wait for BP_ICM_INIT_CORE1 (0xA5) from Core 0           │
  │  3. Send BP_ICM_INIT_CORE1 (0xA5) back to Core 0            │
  │  4. core1_infinite_loop() Enter main service loop            │
  └─────────────────────────────────────────────────────────────┘
                            │
                            ▼
  ┌─────────────────────────────────────────────────────────────┐
  │  Core 0 continues (after ICM sync)                           │
  │                                                              │
  │  22. Send BP_ICM_INIT_CORE1 (0xA5) to Core 1                │
  │  23. Wait for BP_ICM_INIT_CORE1 (0xA5) from Core 1          │
  │  24. ──── CORES SYNCHRONIZED ────                            │
  │  25. core0_infinite_loop() Enter command state machine       │
  └─────────────────────────────────────────────────────────────┘
```

### Synchronization Detail

```
    Core 0                              Core 1
      │                                   │
      │  multicore_launch_core1()         │
      │──────────────────────────────────▶│
      │                                   │ tusb_init()
      │                                   │
      │   send 0xA5 ─────────────────────▶│
      │                                   │ receive 0xA5
      │                                   │
      │◀───────────────────────── send 0xA5│
      │ receive 0xA5                      │
      │                                   │
      │  ═══ SYNCHRONIZED ═══            │
      │                                   │
      ▼                                   ▼
  core0_infinite_loop()          core1_infinite_loop()
```

The initialization sequence runs entirely on Core 0 before the main
loop begins. Hardware peripherals are initialized in dependency order:
FIFOs first (needed for early debug output), then GPIO, SPI, LCD,
analog, storage, and finally the user interface subsystems. After
mounting the filesystem and loading saved configuration, Core 1 is
launched with `multicore_launch_core1()`.

Core 1's entry point initializes TinyUSB and then performs a
handshake with Core 0 using the `BP_ICM_INIT_CORE1` (0xA5) message
over the multicore FIFO. Both cores send and wait for this magic
value, ensuring they are synchronized before entering their respective
infinite loops. This prevents Core 1 from servicing USB or display
before Core 0 has finished hardware initialization.

**Key source files:**
`src/pirate.c` (`main_system_initialization()`, `core1_entry()`,
`core0_infinite_loop()`, `core1_infinite_loop()`),
`src/pirate/intercore_helpers.h`
