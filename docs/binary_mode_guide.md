+++
weight = 90417
title = 'Implementing a New Binary Mode'
+++

# Implementing a New Binary Mode

> Step-by-step guide to adding a new binary mode for programmatic USB access to the Bus Pirate.

---

## Step 1: Understand Binary Modes

Binary modes provide programmatic USB access to the Bus Pirate, bypassing the terminal interface. Each mode handles raw binary protocol communication over the second CDC USB interface.

Binary data flows through the system as follows:

```
USB Host → CDC Interface 1 → bin_rx_fifo → binmode_service() → processing → bin_tx_fifo → USB Host
```

All binary modes are managed through a dispatch table of `binmode_t` structs. The global API drives the lifecycle:

```c
void binmode_setup(void);
void binmode_service(void);
void binmode_cleanup(void);
void binmode_load_save_config(bool save);
```

## Step 2: Learn the `binmode_t` Struct

Each binary mode is described by a `binmode_t` struct defined in `src/binmode/binmodes.h`:

```c
typedef struct _binmode {
    bool lock_terminal;
    bool can_save_config;
    bool reset_to_hiz;
    bool pullup_enabled;
    bool button_to_exit;
    float psu_en_voltage;
    float psu_en_current;
    const char* binmode_name;
    void (*binmode_setup)(void);
    void (*binmode_setup_message)(void);
    void (*binmode_service)(void);
    void (*binmode_cleanup)(void);
    void (*binmode_hook_mode_exc)(void);
} binmode_t;
```

| Field | Type | Purpose |
|-------|------|---------|
| `lock_terminal` | `bool` | Lock terminal while binary mode active |
| `can_save_config` | `bool` | Allow saving configuration |
| `reset_to_hiz` | `bool` | Reset to HiZ mode on cleanup |
| `pullup_enabled` | `bool` | Enable pull-up resistors |
| `button_to_exit` | `bool` | Allow button press to exit mode |
| `psu_en_voltage` | `float` | PSU voltage to enable (0 = disabled) |
| `psu_en_current` | `float` | PSU current limit |
| `binmode_name` | `const char*` | Friendly name for display |
| `binmode_setup` | function ptr | Called once to initialize the mode |
| `binmode_setup_message` | function ptr | Display setup message to terminal |
| `binmode_service` | function ptr | Called repeatedly in main loop |
| `binmode_cleanup` | function ptr | Called when mode exits |
| `binmode_hook_mode_exc` | function ptr | Hook into mode execution |

When `lock_terminal` is set to `true`, the terminal CDC interface is disabled while the binary mode is active. The user interacts only through the binary protocol.

## Step 3: Create Source Files

Create `src/binmode/mymode.c` and `src/binmode/mymode.h` for your new mode.

**Header file** (`src/binmode/mymode.h`):

```c
#ifndef MYMODE_H
#define MYMODE_H

void mymode_setup(void);
void mymode_setup_message(void);
void mymode_service(void);
void mymode_cleanup(void);

#endif
```

**Source file** (`src/binmode/mymode.c`):

```c
#include <stdio.h>
#include "pico/stdlib.h"
#include "pirate.h"
#include "usb_rx.h"
#include "usb_tx.h"
#include "pirate/bio.h"
#include "system_config.h"
#include "mymode.h"

void mymode_setup(void) {
    // One-time initialization for your mode
}

void mymode_setup_message(void) {
    // Display setup info on the terminal
}

void mymode_service(void) {
    uint8_t c;
    // Process incoming data from the binary interface
    if (bin_rx_fifo_try_get(&c)) {
        // Handle received byte
        // Send response
        bin_tx_fifo_put(c);
    }
}

void mymode_cleanup(void) {
    // Release resources when mode exits
}
```

For reference, the SUMP Logic Analyzer (`src/binmode/sump.c`) is a clean example of a complete binary mode implementation. It includes:

```c
#include "pirate.h"
#include "usb_rx.h"
#include "usb_tx.h"
#include "pirate/bio.h"
#include "system_config.h"
```

## Step 4: Add Enum Entry

Add a new entry to the binary mode enum in `src/binmode/binmodes.h`, before `BINMODE_MAXPROTO`:

```c
enum {
    BINMODE_USE_SUMPLA = 0,
    BINMODE_USE_DIRTYPROTO,
    BINMODE_USE_ARDUINO_CH32V003_SWIO,
    BINMODE_USE_LEGACY4THIRD,
    BINMODE_USE_FALA,
    BINMODE_USE_IRTOY_IRMAN,
    BINMODE_USE_IRTOY_AIR,
    BINMODE_USE_MYMODE,       // <-- add your entry here
    BINMODE_MAXPROTO
};
```

`BINMODE_MAXPROTO` must always be the last entry — it is used to size the dispatch table.

## Step 5: Register in Dispatch Table

Add your mode to the `binmodes[]` dispatch table in `src/binmode/binmodes.c`:

```c
extern const binmode_t binmodes[BINMODE_MAXPROTO];
```

Insert an entry at the index matching your enum value:

```c
[BINMODE_USE_MYMODE] = {
    .lock_terminal = true,
    .can_save_config = false,
    .reset_to_hiz = true,
    .pullup_enabled = false,
    .button_to_exit = true,
    .psu_en_voltage = 0,
    .psu_en_current = 0,
    .binmode_name = "My Mode",
    .binmode_setup = mymode_setup,
    .binmode_setup_message = mymode_setup_message,
    .binmode_service = mymode_service,
    .binmode_cleanup = mymode_cleanup,
    .binmode_hook_mode_exc = NULL,
},
```

## Step 6: Add to CMakeLists.txt

Add your new source files to `src/CMakeLists.txt` alongside the other binary mode sources:

```cmake
binmode/mymode.c
binmode/mymode.h
```

## Step 7: Implement Communication

Use the USB RX/TX FIFO APIs to communicate with the host over the binary CDC interface:

| Function | Purpose |
|----------|---------|
| `bin_rx_fifo_try_get(&c)` | Non-blocking read of one byte from host |
| `bin_rx_fifo_get_blocking(&c)` | Blocking read of one byte from host |
| `bin_tx_fifo_put(c)` | Send one byte to host |

A typical `service()` loop reads incoming bytes, processes commands, and writes responses:

```c
void mymode_service(void) {
    uint8_t c;
    if (bin_rx_fifo_try_get(&c)) {
        switch (c) {
            case 0x01:
                // Handle command 0x01
                bin_tx_fifo_put(0x01); // ACK
                break;
            default:
                bin_tx_fifo_put(0x00); // NACK
                break;
        }
    }
}
```

## Step 8: Build and Test

Build the firmware for your target to verify your mode compiles:

```bash
cmake -S . -B build_rp2040 -DPICO_SDK_FETCH_FROM_GIT=TRUE
cmake --build ./build_rp2040 --parallel --target bus_pirate5_rev10
```

Flash the resulting `.uf2` file and verify your mode appears in the binary mode selection menu.

## Checklist

- [ ] Create `src/binmode/mymode.c` and `src/binmode/mymode.h`
- [ ] Implement `setup()`, `service()`, `cleanup()` functions
- [ ] Add `BINMODE_USE_MYMODE` enum entry in `binmodes.h`
- [ ] Register in `binmodes[]` array in `binmodes.c`
- [ ] Add source files to `src/CMakeLists.txt`
- [ ] Build and verify

## Related Documentation

- [usb_communication_guide.md](usb_communication_guide.md) — USB RX/TX APIs
- [new_command_guide.md](new_command_guide.md) — Global command guide
- Source: `src/binmode/binmodes.h`, `src/binmode/binmodes.c`, `src/binmode/sump.c`
