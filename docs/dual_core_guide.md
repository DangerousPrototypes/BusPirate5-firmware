# Dual-Core Architecture

> Developer guide to the RP2040/RP2350 dual-core design in Bus Pirate firmware: core responsibilities, inter-core communication via lock-free SPSC queues, and memory barrier conventions.

---

## Core Responsibilities

| Core | Responsibilities |
|------|-----------------|
| **Core 0** | UI/command processing, protocol engine, mode handlers, syntax pipeline |
| **Core 1** | LCD/display updates, USB service, TinyUSB stack |

Core 0 runs the main command loop — parsing user input, compiling syntax bytecode, and dispatching protocol operations through the mode function-pointer table. Core 1 services USB interrupts via TinyUSB and drives the LCD refresh loop.

---

## SPSC Queue

Lock-free single-producer single-consumer ring buffer for inter-core communication, defined in `src/spsc_queue.h`.

```c
typedef struct {
    volatile uint32_t head;      // Write position (producer only)
    volatile uint32_t tail;      // Read position (consumer only)
    uint8_t* buffer;             // Pointer to data buffer
    uint32_t capacity;           // Total buffer size (must be power of 2)
    uint32_t mask;               // capacity - 1, for fast modulo
} spsc_queue_t;
```

| Field | Purpose |
|-------|---------|
| `head` | Next write position (modified only by producer) |
| `tail` | Next read position (modified only by consumer) |
| `buffer` | Caller-provided data buffer |
| `capacity` | Buffer size (MUST be power of 2) |
| `mask` | `capacity - 1` for bitmask modulo |

**Invariants:**

- Queue empty: `head == tail`
- Queue full: `(head + 1) & mask == tail`

---

## SPSC Queue API

```c
// Init
static inline void spsc_queue_init(spsc_queue_t* q, uint8_t* buffer, uint32_t capacity);

// Producer (one core only)
static inline bool spsc_queue_try_add(spsc_queue_t* q, uint8_t data);
static inline void spsc_queue_add_blocking(spsc_queue_t* q, uint8_t data);

// Consumer (one core only)
static inline bool spsc_queue_try_remove(spsc_queue_t* q, uint8_t* data);
static inline void spsc_queue_remove_blocking(spsc_queue_t* q, uint8_t* data);
static inline bool spsc_queue_try_peek(spsc_queue_t* q, uint8_t* data);
static inline void spsc_queue_peek_blocking(spsc_queue_t* q, uint8_t* data);

// Either core (result may be stale)
static inline uint32_t spsc_queue_level(spsc_queue_t* q);
static inline uint32_t spsc_queue_free(spsc_queue_t* q);
static inline bool spsc_queue_is_empty(spsc_queue_t* q);
static inline bool spsc_queue_is_full(spsc_queue_t* q);
```

The `try_` variants return `false` immediately when the queue is full (producer) or empty (consumer). The `_blocking` variants spin until the operation succeeds.

---

## Memory Barriers

From `src/spsc_queue.h` — uses `__dmb()` for ARM data memory barrier to ensure correct visibility of data across cores.

**Producer side (`try_add`):**

```c
q->buffer[head] = data;
__dmb();  // Release barrier: ensure data write visible before head update
q->head = next_head;
```

**Consumer side (`try_remove`):**

```c
if (tail == q->head) return false;
__dmb();  // Acquire barrier: ensure we see data written before head was updated
*data = q->buffer[tail];
__dmb();  // Release barrier: ensure data read completes before tail update
q->tail = (tail + 1) & q->mask;
```

The producer writes data first, then issues a release barrier before publishing the new `head`. The consumer checks `head`, issues an acquire barrier to read the committed data, then updates `tail` after a second release barrier.

---

## Queue Instances

From `src/usb_rx.h` and `src/usb_tx.h`:

```c
extern spsc_queue_t rx_fifo;      // Terminal input: Core1 → Core0
extern spsc_queue_t bin_rx_fifo;  // Binary mode input: Core1 → Core0
// tx_fifo and bin_tx_fifo: Core0 → Core1
```

| Queue | Producer | Consumer | Purpose |
|-------|----------|----------|---------|
| `rx_fifo` | Core 1 (USB/UART input) | Core 0 (command processor) | Terminal input |
| `tx_fifo` | Core 0 (printf output) | Core 1 (USB transmit) | Terminal output |
| `bin_rx_fifo` | Core 1 (USB input) | Core 0 (binary mode) | Binary mode input |
| `bin_tx_fifo` | Core 0 (binary mode) | Core 1 (USB transmit) | Binary mode output |

---

## Data Flow

```
USB Host ↔ TinyUSB (Core 1) ↔ SPSC Queues ↔ Core 0 ↔ Protocol Engine
                                                      ↔ Command Processor
```

All bytes between the USB host and the command/protocol layer pass through SPSC queues. Core 1 never directly calls into the command processor or protocol engine; it only enqueues/dequeues bytes.

---

## Usage Example

```c
static uint8_t my_buffer[256];
static spsc_queue_t my_queue;
spsc_queue_init(&my_queue, my_buffer, sizeof(my_buffer));
```

Buffer size must be a power of 2. The queue is statically allocated — no `malloc`/`free` required.

---

## Design Rationale

- **Lock-free:** No spinlocks, no priority inversion risk
- **Static allocation:** No malloc/free needed on embedded target
- **Power-of-2 sizes:** Efficient modulo via bitmask
- **Stale reads are safe:** May falsely report empty/full, caller retries

---

## Related Documentation

- [usb_communication_guide.md](usb_communication_guide.md) — USB RX/TX APIs
- [testing_guide.md](testing_guide.md) — SPSC queue test
- Source: `src/spsc_queue.h`, `src/usb_rx.h`, `src/usb_tx.h`
