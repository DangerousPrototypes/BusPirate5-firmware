# AP33772S USB PD Sink Toolkit

CLI demo and reusable driver for the Diodes Inc. AP33772S USB Power Delivery 3.1 sink controller. The driver talks to the chip over I²C, parses source capabilities, issues power requests (fixed, PPS, AVS), and exposes helpers to monitor protection thresholds and live telemetry.

## Features
- Delegated bus abstraction (attach any I²C backend)
- PDO refresh, parsing, and rich introspection helpers
- Request fixed, PPS, AVS, or maximum-power profiles
- Raw register read/write utilities for scripting and debug
- Interactive CLI with history, status view, and command helpers

## Building

This toolkit can be built for various targets. For example, it has been tested on a Raspberry Pi setup.
Requirements:
- GNU `make`
- C11 compiler (tested with `gcc`)
- Linux headers with `linux/i2c-dev.h`

Compile the toolkit:

```sh
make
```

Generated artifacts:
- `ap33772s_demo` – interactive CLI/repl
- `ap33772s.o` – driver object (link into other applications)

Clean the build:

```sh
make clean
```

## Running the CLI

The default I²C device is `/dev/i2c-4`. Override with `--device`:

```sh
./ap33772s_demo --device /dev/i2c-1
```

Enter interactive mode without extra arguments. Helpful commands:

| Command | Description |
| --- | --- |
| `status` | Refresh STATUS, PDO list, voltage/current/temperature measurements |
| `set fixed <idx> <mA>` | Request fixed PDO with target current |
| `set pps <idx> <mV> <mA>` | Request PPS profile |
| `set avs <idx> <mV> <mA>` | Request AVS profile |
| `set max <idx>` | Request maximum power for PDO |
| `readreg <reg> <len>` | Raw register dump (uses driver delegate) |
| `dbg on|off` | Toggle I²C transaction logging |
| `monitor [ms]` | Continuous status polling (default 500 ms) |

Type `help` in the shell for the full command list.

## Embedding the Driver

Link against `ap33772s.c` and provide an `ap33772s_bus_delegate` implementation:

```c
int my_bus_read(void *ctx, uint8_t reg, uint8_t *data, size_t len);
int my_bus_write(void *ctx, uint8_t reg, const uint8_t *data, size_t len);
void my_delay_us(void *ctx, unsigned int usec);

struct ap33772s_bus_delegate delegate = {
    .read = my_bus_read,
    .write = my_bus_write,
    .delay_us = my_delay_us,
    .ctx = my_context_pointer,
};

ap33772s_ref dev = ap33772s_init(&delegate);
```

The `delay_us` delegate should block for at least the requested duration; the driver uses it between NTC calibration writes and while waiting for PDO updates during reset.

After initialisation you can refresh capabilities, request PDOs, and query telemetry:

```c
ap33772s_get_power_capabilities(dev);
ap33772s_request_fixed_pdo(dev, 2, 3000);  // 9 V @ 3 A (Depends on device caps)
int voltage_mv;
ap33772s_read_vreq(dev, &voltage_mv);
```

When finished call
```c
ap33772s_destroy(dev);
```

## License

This project is released under the Apache License 2.0. SPDX identifiers in the sources point to the full terms; include the required NOTICE and license text when redistributing.

## Board Notes

- Ensure the AP33772S is powered and connected to the chosen `/dev/i2c-*` bus.
- The CLI toggles debug prints through the driver delegate; default is enabled.
- Overcurrent/temperature/voltage protections ship enabled via datasheet defaults.
