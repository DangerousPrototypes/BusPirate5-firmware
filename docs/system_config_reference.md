+++
weight = 90414
title = 'system_config Reference'
+++

# system_config Reference

> Global runtime configuration struct holding all Bus Pirate settings, state, and pin assignments.

---

## Overview

The `system_config` struct is a single global instance (`extern struct _system_config system_config`) defined in `src/system_config.h`. It is initialized by `system_init()` at startup and read/written throughout the firmware. Most `uint32_t` fields use that width to standardize JSON parsing for persisted settings.

```c
#include "system_config.h"

// Access anywhere in the firmware:
if (system_config.mode == 0) { /* HiZ */ }
```

---

## Mode & Protocol State

| Field | Type | Default | Purpose |
|-------|------|---------|---------|
| `mode` | `uint8_t` | `0` | Current active protocol mode enum |
| `hiz` | `uint8_t` | `1` | Currently in HiZ (high-impedance) mode |
| `display` | `uint8_t` | `0` | Current display mode enum |
| `subprotocol_name` | `const char*` | `NULL` | Optional sub-protocol name string |
| `mode_active` | `uint8_t` | `0` | Mode is fully initialized and running |
| `num_bits` | `uint8_t` | `8` | Number of data bits per transfer |
| `bit_order` | `uint8_t` | `0` | Bit order: 0 = MSB first, 1 = LSB first |
| `write_with_read` | `uint8_t` | `0` | Write-with-read enabled |
| `open_drain` | `uint8_t` | `0` | Open drain pin mode (1 = enabled) |
| `pullup_enabled` | `uint8_t` | `0` | Pull-up resistors enabled (0 = off, 1 = on) |
| `display_format` | `uint32_t` | `df_auto` | Number display format (decimal, hex, octal, binary) |

---

## Pin Tracking

| Field | Type | Default | Purpose |
|-------|------|---------|---------|
| `pin_labels[HW_PINS]` | `const char*[]` | Per-pin | Human-readable label for each header pin |
| `pin_func[HW_PINS]` | `enum bp_pin_func[]` | Per-pin | Function assigned to each pin (`BP_PIN_IO`, `BP_PIN_VREF`, etc.) |
| `pin_changed` | `uint32_t` | `0xFFFFFFFF` | Bitmask of pins whose labels/functions changed (triggers UI update) |
| `info_bar_changed` | `bool` | `false` | Status bar needs a full redraw |

Pin 0 is initialized to `BP_PIN_VREF` and pin 9 to `BP_PIN_GROUND`. All other pins default to `BP_PIN_IO`. Use the [helper functions](#helper-functions) rather than writing these fields directly.

---

## Error Handling

| Field | Type | Default | Purpose |
|-------|------|---------|---------|
| `error` | `uint8_t` | `0` | Error flag for command chaining |

Set `system_config.error = true` in error paths to signal the command dispatcher. The syntax engine uses this flag to evaluate chained operators (`;`, `||`, `&&`). See [error_handling_reference.md](error_handling_reference.md) for conventions.

---

## Terminal Settings

| Field | Type | Default | Purpose |
|-------|------|---------|---------|
| `terminal_language` | `uint32_t` | `0` | UI language selection index |
| `terminal_usb_enable` | `uint32_t` | `true` | Enable USB CDC terminal |
| `terminal_uart_enable` | `uint32_t` | `false` | Enable UART terminal on IO pins |
| `terminal_uart_number` | `uint32_t` | `1` | Which UART to use for terminal (0 or 1) |
| `terminal_ansi_rows` | `uint8_t` | `24` | Terminal height in rows |
| `terminal_ansi_columns` | `uint8_t` | `80` | Terminal width in columns |
| `terminal_ansi_color` | `uint32_t` | `UI_TERM_NO_COLOR` | ANSI color scheme |
| `terminal_ansi_statusbar` | `uint32_t` | `0` | Status bar display mode |
| `terminal_ansi_statusbar_update` | `bool` | `false` | Status bar needs update |
| `terminal_ansi_statusbar_pause` | `bool` | `false` | Status bar updates paused |
| `terminal_hide_cursor` | `bool` | `false` | Hide terminal cursor |
| `terminal_update` | `uint8_t` | `0` | Terminal update flags |

---

## LED & LCD Settings

| Field | Type | Default | Purpose |
|-------|------|---------|---------|
| `led_color` | `uint32_t` | `0xFF0000` | RGB color value (0xRRGGBB) |
| `led_brightness_divisor` | `uint32_t` | `10` | Brightness divisor (10 = 10%, 5 = 20%, 4 = 25%) |
| `led_effect` | `led_effect_t` | `LED_EFFECT_DISABLED` | Current LED animation effect |
| `led_effect_as_uint32` | `uint32_t` | — | Union alias of `led_effect` for JSON parsing |
| `lcd_screensaver_active` | `uint32_t` | `false` | LCD screensaver currently active |
| `lcd_timeout` | `uint32_t` | `0` | LCD screensaver timeout in seconds (0 = disabled) |

The `led_effect` / `led_effect_as_uint32` union allows the LED effect enum to be read/written as a `uint32_t` for JSON serialization.

---

## Storage

| Field | Type | Default | Purpose |
|-------|------|---------|---------|
| `storage_available` | `uint8_t` | `0` | Storage device detected and available |
| `storage_mount_error` | `uint8_t` | `3` | Storage mount error code |
| `storage_fat_type` | `uint8_t` | `5` | FAT filesystem type (12/16/32) |
| `storage_size` | `float` | `0` | Storage size in MB |

See [storage_guide.md](storage_guide.md) for details on storage initialization and error codes.

---

## Debug Settings

| Field | Type | Default | Purpose |
|-------|------|---------|---------|
| `debug_uart_enable` | `uint32_t` | `false` | Initialize a UART for developer debugging |
| `debug_uart_number` | `uint32_t` | `0` | Which UART to use for debug (0 or 1) |
| `bpio_debug_enable` | `uint32_t` | `false` | Enable debug output for BPIO binary protocol |

---

## Configuration & Hardware

| Field | Type | Default | Purpose |
|-------|------|---------|---------|
| `config_loaded_from_file` | `bool` | `false` | True if config was loaded from storage (`bpconfig.bp`) |
| `disable_unique_usb_serial_number` | `uint32_t` | `false` | Disable unique USB serial number (for manufacturing) |
| `hardware_revision` | `uint8_t` | `0` | Hardware revision number |

---

## PWM, Frequency & Auxiliary Pins

| Field | Type | Default | Purpose |
|-------|------|---------|---------|
| `pwm_active` | `uint8_t` | `0` | Bitmask of active PWM channels (one bit per pin) |
| `freq_config[BIO_MAX_PINS]` | `_pwm_config[]` | zeroed | PWM/frequency settings per pin (period + duty cycle) |
| `freq_active` | `uint8_t` | `0` | Bitmask of active frequency measurement channels |
| `aux_active` | `uint8_t` | `0` | Bitmask of user-controlled auxiliary output pins |

The `_pwm_config` struct holds `float period` (seconds) and `float dutycycle` (0.0–1.0).

---

## Binary Mode

| Field | Type | Default | Purpose |
|-------|------|---------|---------|
| `binmode_usb_rx_queue_enable` | `bool` | `true` | Enable binmode RX queue (false = direct TinyUSB access) |
| `binmode_usb_tx_queue_enable` | `bool` | `true` | Enable binmode TX queue (false = direct TinyUSB access) |
| `binmode_select` | `uint8_t` | `0` | Index of currently active binary mode |
| `binmode_lock_terminal` | `bool` | `false` | Lock (disable) terminal while in binary mode |

---

## Miscellaneous

| Field | Type | Default | Purpose |
|-------|------|---------|---------|
| `big_buffer_owner` | `uint32_t` | `BP_BIG_BUFFER_NONE` | Current owner of the shared big buffer |
| `rts` | `bool` | `false` | RTS flow control state |

---

## When to Read vs. Write

- **Read freely** — `system_config.mode`, `system_config.hiz`, pin state, storage status, display format
- **Write carefully** — `system_config.error` (set in error paths only), `pin_labels`/`pin_func` (in `setup_exc`/`cleanup` only, prefer helper functions)
- **Never write directly** — `system_config.mode` (changed by the mode selection system), `storage_*` fields (managed by the storage subsystem)

---

## Helper Functions

Defined in `src/system_config.h` and implemented in `src/system_config.c`:

```c
// Initialize all fields to defaults — call once at startup
void system_init(void);

// Update a header pin's function and label (by absolute pin index)
// Refuses to modify pins reserved for debug UART
void system_pin_update_purpose_and_label(bool enable, uint8_t pin, enum bp_pin_func func, const char* label);

// Update a BIO pin's function and label (by BIO pin index, offset +1 internally)
void system_bio_update_purpose_and_label(bool enable, uint8_t bio_pin, enum bp_pin_func func, const char* label);

// Track pin active state in a function register bitmask (pwm_active, freq_active, aux_active)
void system_set_active(bool active, uint8_t bio_pin, uint8_t* function_register);
```

**Usage example — claiming a pin for PWM:**

```c
system_bio_update_purpose_and_label(true, bio_pin, BP_PIN_PWM, "PWM");
system_set_active(true, bio_pin, &system_config.pwm_active);
```

**Usage example — releasing a pin:**

```c
system_bio_update_purpose_and_label(false, bio_pin, BP_PIN_IO, NULL);
system_set_active(false, bio_pin, &system_config.pwm_active);
```

---

## Related Documentation

- [bio_pin_guide.md](bio_pin_guide.md) — Pin claiming and BIO pin abstraction
- [error_handling_reference.md](error_handling_reference.md) — Error flag conventions and command chaining
- [storage_guide.md](storage_guide.md) — Storage subsystem and mount error codes
- Source: [`src/system_config.h`](../src/system_config.h), [`src/system_config.c`](../src/system_config.c)
