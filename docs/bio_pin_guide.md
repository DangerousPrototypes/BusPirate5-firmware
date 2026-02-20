+++
weight = 90411
title = 'Pin & BIO System'
+++

# Pin & BIO System

> Developer guide to the Bus Pirate buffered I/O pin subsystem — direction control, voltage reading, pin claiming, and platform mappings.

---

## BIO Pin Overview

Every Bus Pirate exposes eight general-purpose buffered I/O (BIO) pins. Each physical
BIO pin passes through a level-shifting buffer before reaching the target device.
Firmware controls both the RP2040/RP2350 GPIO and the buffer direction independently,
which means configuring a pin always requires two steps: set the MCU GPIO direction
**and** set the buffer direction.

The pin numbering is defined in a per-platform header as an enum:

```c
enum _bp_bio_pins {
    BIO0,
    BIO1,
    BIO2,
    BIO3,
    BIO4,
    BIO5,
    BIO6,
    BIO7,
    BIO_MAX_PINS
};
```

Source: [`src/platform/bpi5-rev10.h`](../src/platform/bpi5-rev10.h)

## Pin Direction API

All functions live in `src/pirate/bio.h` / `src/pirate/bio.c`.

| Function | Signature | Purpose |
|----------|-----------|---------|
| `bio_init` | `void bio_init(void)` | Initialize all BIO pins and buffers to safe input state |
| `bio_output` | `void bio_output(uint8_t bio)` | Configure pin and buffer for output |
| `bio_input` | `void bio_input(uint8_t bio)` | Configure pin and buffer for input |
| `bio_put` | `void bio_put(uint8_t bio, bool value)` | Set output value on a pin |
| `bio_get` | `bool bio_get(uint8_t bio)` | Read current value of a pin |
| `bio_buf_output` | `void bio_buf_output(uint8_t bio)` | Set buffer direction to output |
| `bio_buf_input` | `void bio_buf_input(uint8_t bio)` | Set buffer direction to input |
| `bio_set_function` | `void bio_set_function(uint8_t bio, uint8_t function)` | Set GPIO function (e.g. SPI, PWM) for pin |

### Buffer Direction Constants

```c
#define BUFDIR_INPUT  0
#define BUFDIR_OUTPUT 1
```

`bio_output()` and `bio_input()` handle both the MCU GPIO and buffer direction in
one call. Use `bio_buf_output()` / `bio_buf_input()` only when you need to change
the buffer independently of the MCU GPIO (rare).

## Platform Pin Mappings

Each board revision maps the eight BIO pins to specific MCU GPIOs for the buffer
data line and the buffer direction line. On **BP5 REV10** the mapping is:

```c
// Buffer direction pins: GPIO 0–7
#define BUFDIR0 0
#define BUFDIR1 1
#define BUFDIR2 2
#define BUFDIR3 3
#define BUFDIR4 4
#define BUFDIR5 5
#define BUFDIR6 6
#define BUFDIR7 7

// Buffer IO pins: GPIO 8–15
#define BUFIO0  8
#define BUFIO1  9
#define BUFIO2  10
#define BUFIO3  11
#define BUFIO4  12
#define BUFIO5  13
#define BUFIO6  14
#define BUFIO7  15
```

The lookup tables that `bio_*` functions use internally:

```c
extern const uint8_t bio2bufiopin[8];   // BIO pin → buffer IO GPIO
extern const uint8_t bio2bufdirpin[8];  // BIO pin → buffer direction GPIO
```

Source: [`src/platform/bpi5-rev10.h`](../src/platform/bpi5-rev10.h)

Other board revisions (BP5 REV8, BP6) define their own GPIO numbers but use the
same array names, so `bio_*` functions work unchanged across platforms.

## Pin Claiming

When a mode or subsystem takes ownership of a BIO pin it must **claim** the pin so
the UI can display the assignment and other subsystems know the pin is in use.

### Pin Function Enum

```c
enum bp_pin_func {
    BP_PIN_IO,      // default, unclaimed
    BP_PIN_MODE,    // claimed by a protocol mode
    BP_PIN_PWM,     // PWM output
    BP_PIN_FREQ,    // frequency measurement
    BP_PIN_VREF,    // voltage reference
    BP_PIN_VOUT,    // voltage output
    BP_PIN_GROUND,  // ground connection
    BP_PIN_DEBUG    // debug use
};
```

Source: [`src/system_config.h`](../src/system_config.h)

### Claim / Release Function

```c
void system_bio_update_purpose_and_label(
    bool enable,        // true to claim, false to release
    uint8_t bio_pin,    // BIO0–BIO7
    enum bp_pin_func func,
    const char* label   // up to 4 characters, shown on LCD/status bar
);
```

The global array `system_config.pin_func[HW_PINS]` tracks what each pin is used
for. Claimed pins (any value other than `BP_PIN_IO`) are blocked from being
reassigned by other subsystems, preventing conflicts.

## Usage Pattern

A complete example from `src/mode/dummy1.c` showing the correct claim → use → release
lifecycle.

### Pin Labels

```c
static const char pin_labels[][5] = { "OUT1", "OUT2", "OUT3", "IN1" };
```

Labels are a maximum of **4 characters** (plus null terminator). They appear on the
LCD and in the terminal status bar.

### Setup — Claim Pins

```c
uint32_t dummy1_setup_exc(void) {
    // 1. Configure hardware direction
    bio_output(BIO4);
    bio_output(BIO5);
    bio_output(BIO6);
    bio_input(BIO7);

    // 2. Register ownership and labels
    system_bio_update_purpose_and_label(true, BIO4, BP_PIN_MODE, pin_labels[0]);
    system_bio_update_purpose_and_label(true, BIO5, BP_PIN_MODE, pin_labels[1]);
    system_bio_update_purpose_and_label(true, BIO6, BP_PIN_MODE, pin_labels[2]);
    system_bio_update_purpose_and_label(true, BIO7, BP_PIN_MODE, pin_labels[3]);
    return 1;
}
```

### Cleanup — Release Pins

```c
void dummy1_cleanup(void) {
    // 1. Reset all BIO pins to safe input state
    bio_init();

    // 2. Release ownership
    system_bio_update_purpose_and_label(false, BIO4, BP_PIN_MODE, 0);
    system_bio_update_purpose_and_label(false, BIO5, BP_PIN_MODE, 0);
    system_bio_update_purpose_and_label(false, BIO6, BP_PIN_MODE, 0);
    system_bio_update_purpose_and_label(false, BIO7, BP_PIN_MODE, 0);
}
```

**Key rules:**

1. Always call `bio_init()` before releasing pins — this returns every BIO line to a
   safe high-impedance input state.
2. Pass `0` (NULL) as the label when releasing.
3. Every pin claimed in `setup_exc` must be released in `cleanup`.

## ADC Voltage Reading

BIO pins can also be read as analog voltages through the AMUX (analog multiplexer)
system:

```c
uint32_t amux_read_bio(uint8_t bio);
```

Returns a **12-bit ADC value** (0–4095). The pin must be in input mode to get a
meaningful reading. See `src/pirate/amux.h` for the full AMUX API.

## Related Documentation

- [new_mode_guide.md](new_mode_guide.md) — Step 5: Hardware Setup and Cleanup
- [board_abstraction_guide.md](board_abstraction_guide.md) — Platform-specific pin mappings
- [system_config_reference.md](system_config_reference.md) — Global configuration and pin tracking
- Source: [`src/pirate/bio.h`](../src/pirate/bio.h), [`src/pirate/bio.c`](../src/pirate/bio.c), [`src/platform/bpi5-rev10.h`](../src/platform/bpi5-rev10.h)
