# System Monitor & Power Supply

> Voltage/current monitoring, ADC multiplexer, and PSU control for Bus Pirate hardware.

---

## System Monitor API

Defined in `src/system_monitor.h` — periodic monitoring of voltages and currents.

```c
bool monitor(void);
void monitor_init(void);
void monitor_reset(void);
void monitor_force_update(void);
```

| Function | Returns | Purpose |
|----------|---------|---------|
| `monitor()` | `bool` | Execute monitor update, return true if performed |
| `monitor_init()` | `void` | Initialize system monitor |
| `monitor_reset()` | `void` | Reset monitor state |
| `monitor_force_update()` | `void` | Force immediate monitor update |

---

## Voltage & Current Reading

```c
bool monitor_get_voltage_char(uint8_t pin, uint8_t digit, char* c);
bool monitor_get_voltage_ptr(uint8_t pin, char** c);
bool monitor_get_current_ptr(char** c);
bool monitor_get_current_char(uint8_t digit, char* c);
void monitor_clear_voltage(void);
void monitor_clear_current(void);
bool monitor_voltage_changed(void);
bool monitor_current_changed(void);
```

Character-level change detection: `monitor_voltage_changed()` and `monitor_current_changed()` enable efficient display updates — only redraw when values change.

---

## AMUX — Analog Multiplexer

Defined in `src/pirate/amux.h` — routes analog signals to the ADC.

```c
void amux_init(void);
bool amux_select_input(uint16_t channel);
bool amux_select_bio(uint8_t bio);
uint32_t amux_read(uint8_t channel);
uint32_t amux_read_present_channel(void);
uint32_t amux_read_bio(uint8_t bio);
uint32_t amux_read_current(void);
void amux_sweep(void);
void adc_busy_wait(bool enable);
```

| Function | Returns | Purpose |
|----------|---------|---------|
| `amux_read_bio(bio)` | `uint32_t` | Read voltage from BIO pin (12-bit ADC) |
| `amux_read_current()` | `uint32_t` | Read current sense (12-bit ADC) |
| `amux_sweep()` | `void` | Read all channels, store in `hw_adc_raw[]` and `hw_adc_voltage[]` |
| `amux_select_bio(bio)` | `bool` | Select BIO pin for measurement |

### AMUX Channel Map (BP5 REV10)

```c
enum adc_mux{
    HW_ADC_MUX_BPIO7,
    HW_ADC_MUX_BPIO6,
    HW_ADC_MUX_BPIO5,
    HW_ADC_MUX_BPIO4,
    HW_ADC_MUX_BPIO3,
    HW_ADC_MUX_BPIO2,
    HW_ADC_MUX_BPIO1,
    HW_ADC_MUX_BPIO0,
    HW_ADC_MUX_VUSB,
    HW_ADC_MUX_CURRENT_DETECT,
    HW_ADC_MUX_VREG_OUT,
    HW_ADC_MUX_VREF_VOUT,
    HW_ADC_MUX_COUNT
};
```

---

## ADC Averaging

```c
#define ADC_AVG_TIMES 64

inline uint32_t get_adc_average(uint32_t avgsum) {
    return ((avgsum + (ADC_AVG_TIMES / 2)) / ADC_AVG_TIMES);
}

extern bool reset_adc_average;
```

- 64-sample averaging with proper rounding
- Set `reset_adc_average = true` to restart averaging after step changes

---

## ADC Voltage Conversion

From `src/platform/bpi5-rev10.h`:

```c
// Pins with /2 resistor divider (MUX inputs)
#define hw_adc_to_volts_x2(X) ((6600*hw_adc_raw[X])/4096);
// Pins with no divider (current sense)
#define hw_adc_to_volts_x1(X) ((3300*hw_adc_raw[X])/4096);
```

---

## PSU Control

PSU hardware varies by platform:

| Platform | PSU Type | Control |
|----------|----------|---------|
| BP5, BP5XL, BP6 | `BP_HW_PSU_PWM` | PWM-based voltage regulation |
| BP7 | `BP_HW_PSU_DAC` | DAC-based voltage regulation |

PSU API (from `src/pirate/psu.h`):

- PSU enable/disable
- Voltage setting
- Current limiting
- Over-current protection

---

## Related Documentation

- [bio_pin_guide.md](bio_pin_guide.md) — Pin voltage reading
- [board_abstraction_guide.md](board_abstraction_guide.md) — Platform-specific ADC config
- [system_config_reference.md](system_config_reference.md) — PSU state fields
- Source: `src/system_monitor.h`, `src/system_monitor.c`, `src/pirate/amux.h`, `src/pirate/psu.h`
