+++
weight = 90408
title = 'Build System & Targets'
+++

# Build System & Targets

> Developer guide to the Bus Pirate CMake build system, platform selection, and firmware build targets.

---

## Platform Selection

The top-level `CMakeLists.txt` selects the microcontroller platform based on the `BP_PICO_PLATFORM` variable:

```cmake
if(${BP_PICO_PLATFORM} MATCHES "rp2350")
    set(PICO_PLATFORM "rp2350")
    set(PICO_BOARD buspirate6)   
else()
    set(PICO_PLATFORM "rp2040")
    set(PICO_BOARD buspirate5)
endif()
```

When `BP_PICO_PLATFORM` is not set, the build defaults to RP2040. Setting it to `rp2350` switches to the RP2350 platform used by Bus Pirate 5XL, 6, and 7.

## Build Targets

Each target is defined in `src/CMakeLists.txt` with `add_executable()` and `target_compile_definitions()` that set `BP_VER`, `BP_REV`, and `BP_FIRMWARE_HASH`.

| Target | BP_VER | BP_REV | Platform | Board Header |
|--------|--------|--------|----------|--------------|
| `bus_pirate5_rev8` | 5 | 8 | RP2040 | `buspirate5.h` |
| `bus_pirate5_rev10` | 5 | 10 | RP2040 | `buspirate5.h` |
| `bus_pirate5_xl` | XL5 | 10 | RP2350 | `buspirate5xl.h` |
| `bus_pirate6` | 6 | 10 | RP2350 | `buspirate6.h` |

Board headers live in `src/boards/` and define pin assignments and hardware capabilities for each variant.

## Build Commands

```bash
# Configure for RP2040 (Bus Pirate 5)
cmake -S . -B build_rp2040 -DPICO_SDK_FETCH_FROM_GIT=TRUE

# Configure for RP2350 (Bus Pirate 5XL/6/7)
cmake -S . -B build_rp2350 -DPICO_SDK_FETCH_FROM_GIT=TRUE -DBP_PICO_PLATFORM=rp2350

# Build specific targets
cmake --build ./build_rp2040 --parallel --target bus_pirate5_rev10
cmake --build ./build_rp2350 --parallel --target bus_pirate6
```

RP2040 targets (`bus_pirate5_rev8`, `bus_pirate5_rev10`) must be built from an RP2040-configured build directory. RP2350 targets (`bus_pirate5_xl`, `bus_pirate6`) must be built from an RP2350-configured build directory.

## Output Files

Each target produces three firmware files in the build directory:

- **`.elf`** — Debug format for use with GDB/OpenOCD
- **`.uf2`** — Drag-and-drop USB flashing (hold BOOTSEL, plug in, copy file)
- **`.bin`** — Raw binary image

## BP_VER / BP_REV Compile Defines

The `BP_VER` and `BP_REV` values set per target drive a platform cascade in `src/pirate.h` that selects the correct platform header and hardware feature flags:

```c
#ifndef BP_REV
    #error "No /platform/ file included in pirate.h"
#else
    #if BP_VER == 5
        #if BP_REV == 8
            #include "platform/bpi5-rev8.h"
            #define BP_HW_STORAGE_TFCARD 1
        #elif BP_REV == 10
            #include "platform/bpi5-rev10.h"
            #define BP_HW_STORAGE_NAND 1
        #endif
        #define BP_HW_IOEXP_595 1
        #define RPI_PLATFORM RP2040
        #define BP_HW_PSU_PWM 1
    #elif BP_VER == XL5
        #include "platform/bpi5xl-rev0.h"
        #define RPI_PLATFORM RP2350
    #elif BP_VER == 6
        #include "platform/bpi6-rev2.h"  
        #define RPI_PLATFORM RP2350
    #elif BP_VER == 7
        #include "platform/bpi7-rev0.h"
        #define RPI_PLATFORM RP2350
    #endif
#endif
```

This cascade determines storage type (TF card vs NAND), IO expander type, PSU implementation, and which platform initialization file is compiled.

## Adding a New Build Target

1. Create a platform header in `src/platform/bpiX-revN.h` with pin definitions and hardware constants.
2. Create a board header in `src/boards/buspirateName.h` with the Pico SDK board configuration.
3. Add the target in `src/CMakeLists.txt` using `add_executable()` and `target_compile_definitions()` to set `BP_VER`, `BP_REV`, and `BP_FIRMWARE_HASH`.
4. Add a platform branch in `src/pirate.h` to include the new platform header and set feature flags.

## PIO Program Compilation

The build uses `pico_generate_pio_header()` to compile `.pio` assembly files into C headers. These are iterated over all targets in `src/CMakeLists.txt`:

```cmake
pico_generate_pio_header(${revision} ${CMAKE_CURRENT_LIST_DIR}/pirate/hw2wire.pio)
pico_generate_pio_header(${revision} ${CMAKE_CURRENT_LIST_DIR}/pirate/hwi2c.pio)
pico_generate_pio_header(${revision} ${CMAKE_CURRENT_LIST_DIR}/pirate/hwuart.pio)
pico_generate_pio_header(${revision} ${CMAKE_CURRENT_LIST_DIR}/pirate/hw1wire.pio)
pico_generate_pio_header(${revision} ${CMAKE_CURRENT_LIST_DIR}/binmode/logicanalyzer.pio)
```

PIO programs provide hardware-accelerated protocol implementations for 1-Wire, I2C, 2-Wire, 3-Wire, UART, SPI sniffing, WS2812 LEDs, IR, and the logic analyzer.

## Source File Organization

In `src/CMakeLists.txt`, the `buspirate_common` variable lists all shared source files compiled into every target. Target-specific source files (e.g., platform initialization) are added per target via separate `add_executable()` source lists.

## Docker Build Environment

The `docker-compose.yml` provides a containerized build environment:

```yaml
services:
  base:
    image: "wyatt3arp/buspirate_v5plus:latest"
    build: ./docker
    volumes:
      - .:/project
    working_dir: '/project'

  dev:
    extends:
      service: base
    environment:
      - MODE=development
```

```bash
docker compose build dev
docker compose run dev build-clean
```

The `dev-debug` service extends the base with USB device passthrough for on-device debugging.

## Requirements

- **CMake** 3.21+
- **Pico SDK** 2.1.0 — fetched via `PICO_SDK_FETCH_FROM_GIT=TRUE` or provided via `PICO_SDK_PATH`
- **ARM GCC toolchain** — `arm-none-eabi-gcc`
- **Python 3** — required by the Pico SDK build process

## Related Documentation

- [Board Abstraction Guide](board_abstraction_guide.md)
- [Testing Guide](testing_guide.md)
- Source: [`CMakeLists.txt`](../CMakeLists.txt), [`src/CMakeLists.txt`](../src/CMakeLists.txt), [`docker-compose.yml`](../docker-compose.yml)
