+++
weight = 90409
title = 'Board Abstraction & Platform Porting'
+++

# Board Abstraction & Platform Porting

> How Bus Pirate firmware maps hardware variants through a header cascade, feature flags, and per-board pin definitions.

---

## Header Cascade

Every source file includes `src/pirate.h`, which selects the correct platform and board headers based on compile-time defines `BP_VER` and `BP_REV`:

```
pirate.h → platform header (bpiX-revN.h) → board header (buspirateN.h) → linker script (memmap_*.ld)
```

The cascade in `src/pirate.h`:

```c
#ifndef BP_REV
    #error "No /platform/ file included in pirate.h"
#else
    #if BP_VER == 5
        #if BP_REV == 8
            #include "platform/bpi5-rev8.h"
            #define BP_HW_STORAGE_TFCARD 1
            #define BP_HW_PSU_PWM_IO_BUG 1
        #elif BP_REV == 9
            #include "platform/bpi5-rev9.h"
            #define BP_HW_STORAGE_NAND 1
        #elif BP_REV == 10
            #include "platform/bpi5-rev10.h"
            #define BP_HW_STORAGE_NAND 1
        #else
            #error "Unknown platform version in pirate.h"
        #endif
        #define BP_HW_IOEXP_595 1
        #define RPI_PLATFORM RP2040
        #define BP_HW_PSU_PWM 1
    #elif BP_VER == XL5
        #include "platform/bpi5xl-rev0.h"
        #define RPI_PLATFORM RP2350
        #define BP_HW_STORAGE_NAND 1
        #define BP_HW_IOEXP_595 1
        #define BP_HW_PSU_PWM 1
    #elif BP_VER == 6
        #include "platform/bpi6-rev2.h"
        #define RPI_PLATFORM RP2350
        #define BP_HW_STORAGE_NAND 1
        #define BP_HW_IOEXP_NONE 1
        #define BP_HW_FALA_BUFFER 1
        #define BP_HW_PSU_PWM 1
        #define BP_HW_RP2350_E9_BUG 1
    #elif BP_VER == 7
        #include "platform/bpi7-rev0.h"
        #define RPI_PLATFORM RP2350
        #define BP_HW_STORAGE_NAND 1
        #define BP_HW_PULLX 1
        #define BP_HW_IOEXP_I2C 1
        #define BP_HW_FALA_BUFFER 1
        #define BP_HW_PSU_DAC 1
        #define BP_HW_RP2350_E9_BUG 1
    #else
        #error "Unknown platform version in pirate.h"
    #endif
#endif
```

`BP_VER` and `BP_REV` are set by the board header (e.g. `src/boards/buspirate5.h`), which is selected by the CMake build target.

---

## BP_HW_* Feature Flags

Feature flags enable or disable hardware-specific code paths at compile time. Each flag is set in the `pirate.h` cascade based on the active board variant.

| Flag | Description | Set By |
|------|-------------|--------|
| `BP_HW_STORAGE_NAND` | NAND flash storage | Rev 9+, XL5, 6, 7 |
| `BP_HW_STORAGE_TFCARD` | TF/microSD card storage | Rev 8 only |
| `BP_HW_PSU_PWM` | PWM-based power supply | Rev 5, XL5, 6 |
| `BP_HW_PSU_DAC` | DAC-based power supply | Rev 7 |
| `BP_HW_PSU_PWM_IO_BUG` | PSU PWM IO bug workaround | Rev 8 |
| `BP_HW_IOEXP_595` | 74HC595 shift register IO expander | Rev 5, XL5 |
| `BP_HW_IOEXP_I2C` | I2C IO expander | Rev 7 |
| `BP_HW_IOEXP_NONE` | No IO expander | Rev 6 |
| `BP_HW_FALA_BUFFER` | Follow-along logic analyzer buffer | Rev 6, 7 |
| `BP_HW_PULLX` | Pull-up/pull-down resistors | Rev 7 |
| `BP_HW_RP2350_E9_BUG` | RP2350 E9 errata workaround | Rev 6, 7 |

Use these flags with `#ifdef` guards in driver code to compile only the paths relevant to the target hardware:

```c
#ifdef BP_HW_STORAGE_NAND
    nand_init();
#elif defined(BP_HW_STORAGE_TFCARD)
    tfcard_init();
#endif
```

---

## Platform Headers

Platform headers live in `src/platform/` and define everything specific to a board revision's physical layout.

**Example: `src/platform/bpi5-rev10.h`** defines:

- **Hardware version strings**: `BP_HARDWARE_VERSION` (`"Bus Pirate 5 REV10"`), `BP_HARDWARE_MCU`, `BP_HARDWARE_VERSION_MAJOR`, `BP_HARDWARE_REVISION`
- **Buffer direction pins**: `BUFDIR0`–`BUFDIR7`
- **Buffer IO pins**: `BUFIO0`–`BUFIO7`
- **BIO pin enum**: `BIO0`–`BIO7` via `enum _bp_bio_pins`, with `BIO_MAX_PINS` (8)
- **Pin mapping arrays**: `bio2bufiopin[]`, `bio2bufdirpin[]` — translate logical BIO numbers to physical GPIO pins
- **SPI, display, PSU, and ADC pin assignments**
- **ADC mux channel mappings**: `enum adc_mux` with entries like `HW_ADC_MUX_BPIO7`, `HW_ADC_MUX_VREF_VOUT`, `HW_ADC_MUX_COUNT`, `HW_ADC_CURRENT_SENSE`
- **Color palette** for terminal and LCD themes
- **LCD dimensions**: `BP_LCD_WIDTH` (240), `BP_LCD_HEIGHT` (320)

Each platform header follows the same structure, but pin numbers and peripheral assignments differ per revision.

---

## Board Headers

Board headers live in `src/boards/` and configure the Pico SDK for the target microcontroller.

**Example: `src/boards/buspirate5.h`** defines:

- `PICO_PLATFORM` — set to `rp2040`
- `PICO_FLASH_SIZE_BYTES` — `(16 * 1024 * 1024)` (16 MB)
- Boot stage selection — `PICO_BOOT_STAGE2_CHOOSE_W25Q080`

### Board Headers Available

| File | Platform |
|------|----------|
| `src/boards/buspirate5.h` | RP2040 (BP5 Rev8/10) |
| `src/boards/buspirate5xl.h` | RP2350 (BP5 XL) |
| `src/boards/buspirate6.h` | RP2350 (BP6) |
| `src/boards/buspirate7.h` | RP2350 (BP7) |

---

## File Structure

| File | Purpose |
|------|---------|
| `src/pirate.h` | Master config, platform cascade, feature flags |
| `src/platform/bpiX-revN.h` | Pin maps, ADC mux, display config, hardware strings |
| `src/boards/buspirateN.h` | SDK platform, flash size, boot stage |
| `src/boards/memmap_default_rp2040.ld` | Linker script — RP2040 memory layout |
| `src/boards/memmap_default_rp2350.ld` | Linker script — RP2350 memory layout |
| `src/boards/memmap_psram_rp2350.ld` | Linker script — RP2350 with PSRAM |

---

## Adding a New Hardware Revision

1. **Create platform header** `src/platform/bpiX-revN.h` — define all pin mappings (`BUFDIR*`, `BUFIO*`, `BIO*`), ADC mux channels, display config, hardware version strings, and color palette.
2. **Create board header** `src/boards/buspirateX.h` — set `PICO_PLATFORM` (`rp2040` or `rp2350`), `PICO_FLASH_SIZE_BYTES`, and boot stage selection.
3. **Add linker script** if needed: `src/boards/memmap_*.ld` for custom memory layout.
4. **Add platform branch in `src/pirate.h`** — new `#elif` under the appropriate `BP_VER` block with the correct `BP_HW_*` flags for the revision's hardware capabilities.
5. **Add build target in `src/CMakeLists.txt`** — register the new board as a CMake target with the correct `BP_VER` and `BP_REV` defines.
6. **Implement platform-specific init code** if the hardware differs from existing revisions (new peripherals, different IO expander, etc.).

---

## Related Documentation

- [build_system_guide.md](build_system_guide.md) — Build targets and CMake config
- [bio_pin_guide.md](bio_pin_guide.md) — Pin mapping details
- Source: `src/pirate.h`, `src/platform/bpi5-rev10.h`, `src/boards/buspirate5.h`
