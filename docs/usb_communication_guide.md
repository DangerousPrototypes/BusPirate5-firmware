+++
weight = 90413
title = 'USB & Communication'
+++

# USB & Communication

> Developer guide to USB interfaces, FIFO queues, and data flow between cores on Bus Pirate.

---

## TinyUSB Interfaces

Bus Pirate exposes three USB interfaces via TinyUSB, running on Core 1:

| Interface   | CDC   | Purpose                              |
|-------------|-------|--------------------------------------|
| Terminal    | CDC 0 | User terminal (command line, output)  |
| Binary mode | CDC 1 | Binary protocol communication        |
| MSC         | —     | Mass storage (flash/SD card)         |

CDC 0 carries interactive terminal traffic (commands and responses). CDC 1 carries the FlatBuffers-based binary protocol for programmatic access. The MSC interface exposes onboard flash or SD card storage.

---

## SPSC Queue Instances

Lock-free single-producer/single-consumer queues bridge Core 1 (USB) and Core 0 (application logic). Declared in `src/usb_rx.h`:

```c
extern spsc_queue_t rx_fifo;
extern spsc_queue_t bin_rx_fifo;
```

| Queue          | Direction      | Producer                        | Consumer                         |
|----------------|----------------|---------------------------------|----------------------------------|
| `rx_fifo`      | USB → Core 0   | Core 1 (USB/UART/RTT input)    | Core 0 (command processor)       |
| `tx_fifo`      | Core 0 → USB   | Core 0 (printf output)          | Core 1 (USB transmit)            |
| `bin_rx_fifo`  | USB → Core 0   | Core 1 (binary USB input)       | Core 0 (binary mode handler)     |
| `bin_tx_fifo`  | Core 0 → USB   | Core 0 (binary mode output)     | Core 1 (USB transmit)            |

---

## Data Flow

```
Terminal:  USB Host ↔ CDC0 ↔ TinyUSB (Core 1) ↔ rx_fifo/tx_fifo ↔ Core 0 ↔ Command Processor
Binary:   USB Host ↔ CDC1 ↔ TinyUSB (Core 1) ↔ bin_rx_fifo/bin_tx_fifo ↔ Core 0 ↔ Binary Mode Handler
```

Core 1 owns the TinyUSB stack and services USB transfers. Received bytes are pushed into the appropriate SPSC queue. Core 0 pulls bytes out for processing and pushes responses back through the transmit queues.

---

## Receive API

Defined in `src/usb_rx.h`. Terminal receive functions operate on `rx_fifo`:

```c
void rx_fifo_init(void);
void rx_fifo_add(char* c);
bool rx_fifo_try_get(char* c);
void rx_fifo_get_blocking(char* c);
bool rx_fifo_try_peek(char* c);
void rx_fifo_peek_blocking(char* c);
```

| Function               | Returns | Blocking | Purpose                                |
|------------------------|---------|----------|----------------------------------------|
| `rx_fifo_try_get`      | `bool`  | No       | Try to get character from terminal FIFO |
| `rx_fifo_get_blocking` | `void`  | Yes      | Get character, wait if empty            |
| `rx_fifo_try_peek`     | `bool`  | No       | Peek without removing                   |
| `rx_fifo_peek_blocking`| `void`  | Yes      | Peek, wait if empty                     |

### Binary Mode Receive API

Binary mode receive functions operate on `bin_rx_fifo`:

```c
void bin_rx_fifo_add(char* c);
void bin_rx_fifo_get_blocking(char* c);
void bin_rx_fifo_available_bytes(uint16_t* cnt);
bool bin_rx_fifo_try_get(char* c);
```

`bin_rx_fifo_available_bytes()` reports how many bytes are waiting, which is useful for framed binary protocol reads.

---

## Transmit API

Defined in `src/usb_tx.h`. Terminal transmit functions operate on `tx_fifo`:

```c
void tx_fifo_init(void);
void tx_fifo_service(void);
void tx_fifo_put(char* c);
void tx_fifo_try_put(char* c);
void tx_sb_start(uint32_t valid_characters_in_status_bar);
```

`tx_fifo_service()` is called by Core 1 to drain the queue into USB. `tx_fifo_put()` blocks if the queue is full; `tx_fifo_try_put()` drops the character instead.

### Binary Mode Transmit API

```c
void bin_tx_fifo_put(const char c);
void bin_tx_fifo_service(void);
bool bin_tx_not_empty(void);
bool bin_tx_fifo_try_get(char* c);
extern char tx_sb_buf[1024];
```

`bin_tx_fifo_service()` is the Core 1 counterpart that drains binary transmit data into CDC 1.

---

## Status Bar

The status bar uses a dedicated path to avoid interleaving with command output:

- `tx_sb_buf[1024]` — dedicated buffer for status bar content
- `tx_sb_start()` — initiates status bar transmission with the number of valid characters

Status bar updates are rendered into `tx_sb_buf` and sent as a single block, keeping them visually separate from streaming command output on the terminal.

---

## Queue Configuration

From `system_config.h`, binary mode queues can be bypassed:

```c
bool binmode_usb_rx_queue_enable;  // Enable binmode RX queue
bool binmode_usb_tx_queue_enable;  // Enable binmode TX queue
```

When disabled, binary mode handles USB directly with TinyUSB functions, bypassing SPSC queues. This is useful for binary mode implementations that need low-latency or custom USB transfer handling.

---

## Debug Paths

Alternative input/output paths for development and debugging:

| Path       | File                         | Purpose                    |
|------------|------------------------------|----------------------------|
| UART debug | `src/debug_uart.c`           | Debug output via UART      |
| RTT debug  | `src/debug_rtt.c`            | Debug output via SEGGER RTT|
| RTT input  | `rx_from_rtt_terminal()`     | Terminal input via RTT     |

RTT input feeds into `rx_fifo` alongside USB input, so the command processor handles both sources transparently.

---

## Related Documentation

- [dual_core_guide.md](dual_core_guide.md) — SPSC queue design and memory barriers
- [binary_mode_guide.md](binary_mode_guide.md) — Binary mode implementation
- Source: `src/usb_rx.h`, `src/usb_tx.h`, `src/spsc_queue.h`
