   # Hardware-in-the-Loop (HIL) Test Rig

A Python-based test harness for verifying Bus Pirate 5 firmware on real hardware.
Communicates over USB serial, parses VT100 output, and can flash new firmware
before running tests.

## Quick Start

```bash
# Install dependencies
pip install -r tests/hil/requirements.txt

# Run all tests (auto-discovers Bus Pirate by VID:PID)
pytest tests/hil/ -v

# Specify ports explicitly
pytest tests/hil/ -v \
    --terminal-port /dev/ttyACM0 \
    --binary-port /dev/ttyACM1

# Flash new firmware first, then test
pytest tests/hil/ -v \
    --uf2 build/src/bus_pirate5_rev10.uf2

# Run just the hint/completion tests
pytest tests/hil/test_hint_completion.py -v

# Debug mode (prints raw serial traffic)
pytest tests/hil/ -v --debug-serial
```

## Architecture

The Bus Pirate 5 exposes **3 USB interfaces** (VID `0x1209`, PID `0x7331`):

| Interface | Port (typical) | Name | Purpose |
|-----------|---------------|------|---------|
| CDC 0 | `/dev/ttyACM0` | Bus Pirate CDC | Human terminal (command line) |
| CDC 1 | `/dev/ttyACM1` | Bus Pirate BIN | BPIO2 binary protocol |
| MSC | — | Bus Pirate MSC | Flash storage |

The test rig talks to both CDC interfaces:

```
┌────────────────┐     CDC 0 (terminal)      ┌──────────────┐
│                │◄──────────────────────────►│              │
│  pytest suite  │                            │  Bus Pirate  │
│                │     CDC 1 (binary/BPIO2)   │     5/6      │
│                │◄──────────────────────────►│              │
└────────────────┘                            └──────────────┘
     │                                              ▲
     │  picotool / UF2 copy                         │
     └──────────────────────────────────────────────┘
                    (firmware flash)
```

## Components

### `bp_terminal_client.py` — Terminal Client (CDC 0)

Serial client that talks to the human terminal interface.

**Key features:**
- **VT100 auto-response**: The Bus Pirate sends `\x1b7\x1b[999;999H\x1b[6n\x1b8`
  to ask "how big is your terminal?" — the client auto-replies with
  `[{rows};{cols}R` (no ESC prefix). This enables the statusbar, scroll regions,
  and colored output.
- **Prompt detection**: The Bus Pirate sends `\x03` (ETX) as a prompt marker.
  `read_until_prompt()` blocks until this byte arrives.
- **Two capture modes**:
  - `send_command_clean()` — strips ANSI codes, returns plain text
  - `send_command()` — preserves raw VT100 sequences for assertion
- **Hint/completion helpers**: `send_partial()` types characters without Enter
  (for testing linenoise hints), `send_tab()` triggers tab completion.

**Usage:**
```python
from bp_terminal_client import TerminalClient

client = TerminalClient("/dev/ttyACM0", rows=24, cols=80)

# Send a command and get clean output
output = client.send_command_clean("help")
assert "Bus Pirate" in output

# Test hints: type partial text, capture ghost-text
hint_output = client.send_partial("hel")  # should hint "p"
client.send_ctrl_c()  # cancel

# Test tab completion
client.send_partial("hel")
completion = client.send_tab()  # should complete to "help"
client.send_ctrl_c()

client.close()
```

### `bp_vt100_parser.py` — VT100 Parser

Parses raw terminal output into structured data for test assertions.

**Parsed sequence types:**
- `ScrollRegion(top, bottom)` — from `ESC[top;bottomr`
- `CursorPosition(row, col)` — from `ESC[row;colH`
- `EraseLine(mode)` — from `ESC[K`
- `EraseChars(count)` — from `ESC[nX`
- `SGR(params)` — from `ESC[...m` (color/style)
- `CursorSave`, `CursorRestore` — from `ESC7`, `ESC8`
- `CursorHide`, `CursorShow` — from `ESC[?25l`, `ESC[?25h`

**VirtualScreen:** A lightweight 2D character buffer that processes VT100 output.
Not a full terminal emulator — just enough to verify statusbar content ends up
in the right rows.

```python
from bp_vt100_parser import find_scroll_regions, VirtualScreen

regions = find_scroll_regions(raw_output)
assert regions[-1].bottom == 20  # 24-row terminal, 4-line statusbar

screen = VirtualScreen(24, 80)
screen.feed(raw_output)
assert "HiZ" in screen.get_line(23)  # statusbar bottom row
```

### `bp_bpio_client.py` — BPIO2 Binary Protocol Client (CDC 1)

FlatBuffers + COBS client for the binary protocol interface.

**Wire protocol:**
1. Build FlatBuffers `RequestPacket` → serialize
2. COBS-encode → append `0x00` delimiter → write to serial
3. Read until `0x00` → COBS-decode → parse as `ResponsePacket`

Max packet size: 640 bytes. Version: major=2, minor=0.

```python
from bp_bpio_client import BPIOClient

client = BPIOClient("/dev/ttyACM1")

# Query device status
status = client.get_status()
print(status["version_firmware_major"])
print(status["mode_current"])  # "HiZ"
print(status["modes_available"])  # ["HiZ", "UART", "SPI", ...]

# Enter bootloader for firmware flash
client.enter_bootloader()  # closes port, device re-enumerates
```

### `bp_flash.py` — Firmware Flash Tool

Flashes `.uf2` firmware via `picotool` or UF2 drag-and-drop.

**Flash pipeline:**
1. Send `ConfigurationRequest(hardware_bootloader=True)` via BPIO2
2. Device re-enumerates as RP2 Boot USB mass storage
3. Flash via `picotool load firmware.uf2 -f` (preferred) or copy to RP2 Boot volume
4. Wait for Bus Pirate to re-enumerate with VID:PID `1209:7331`

```bash
# Standalone usage
python tests/hil/bp_flash.py build/src/bus_pirate5_rev10.uf2 \
    --binary-port /dev/ttyACM1
```

### `conftest.py` — pytest Fixtures

| Fixture | Scope | Purpose |
|---------|-------|---------|
| `port_pair` | session | Auto-discover CDC 0 + CDC 1 ports by VID/PID |
| `terminal` | session | `TerminalClient` connected to CDC 0 |
| `bpio` | session | `BPIOClient` connected to CDC 1 |
| `soft_reset` | function (autouse) | Reset to HiZ mode after each test |
| `flash_if_requested` | session | Flash `--uf2` firmware before tests |

**CLI options:**

| Option | Default | Description |
|--------|---------|-------------|
| `--uf2` | None | Flash this firmware before testing |
| `--terminal-port` | auto | Override CDC 0 port |
| `--binary-port` | auto | Override CDC 1 port |
| `--terminal-rows` | 24 | Terminal rows to report |
| `--terminal-cols` | 80 | Terminal cols to report |
| `--debug-serial` | off | Print raw serial traffic |

## Test Suites

### `test_smoke.py` — Smoke Tests (10 tests)

Basic connectivity and sanity checks. These pass on any standard firmware.

| Test | What it checks |
|------|---------------|
| `test_device_responds_to_enter` | Prompt contains `>` |
| `test_help_command` | `help` output is reasonable |
| `test_info_command` | `i` shows firmware/hardware info |
| `test_voltage_command` | `v` shows voltage readings |
| `test_terminal_size_detection` | VT100 size reply → scroll region |
| `test_empty_command_returns_prompt` | Enter → prompt (no crash) |
| `test_bpio_status_query` | BPIO2 StatusRequest works |
| `test_bpio_mode_is_hiz` | Default mode is HiZ |
| `test_bpio_modes_available` | Mode list is non-empty |
| `test_bpio_adc_readings` | ADC data is present |

### `test_hint_completion.py` — Hint & Completion Tests (26 tests)

Tests for the `bp_cmd.c` linenoise hint/completion refactoring. These exercise
the ghost-text hints and TAB completion on a real device.

**Test classes:**

| Class | Tests | What it covers |
|-------|-------|---------------|
| `TestCommandHints` | 4 | Partial command name → hint rest (`hel` → `p`) |
| `TestVerbHints` | 2 | Action verb hints (`m u` → `art`) |
| `TestShortFlagHints` | 3 | Short flag hints (`-b` → `<baud>`) |
| `TestLongFlagHints` | 4 | Long flag hints, **including exact-match bug fix** |
| `TestTabCompletion` | 5 | TAB completion for commands, verbs, flags |
| `TestEdgeCases` | 8 | Empty input, unknown commands, Ctrl-C, truncation |

**Key regression test:** `test_exact_long_flag_shows_arg_hint` — validates that
typing `m uart --baud` (exact match) shows the `<baud>` arg hint. Before the
bug fix, exact-match long flags got no hint because the partial-match loop
required strict less-than on length.

### `test_vt100_basics.py` — VT100 Output Tests (7 tests)

VT100 escape sequence and toolbar rendering validation.

| Test | What it checks |
|------|---------------|
| `test_scroll_region_set_for_24_rows` | Scroll region ≈ row 20 for 24-row term |
| `test_scroll_region_adjusts_to_40_rows` | Scroll region increases for 40 rows |
| `test_cursor_hidden_during_update` | Cursor hide/show sequences present |
| `test_cursor_visible_after_command` | Cursor shown after command completes |
| `test_default_output_has_color` | SGR sequences in `help` output |
| `test_info_has_color` | SGR sequences in `i` output |
| `test_screen_processes_help_output` | VirtualScreen processes without crash |

## Generated Files

The `tests/hil/bpio/` directory contains FlatBuffers Python bindings generated
from `bpio.fbs`:

```bash
# Regenerate if schema changes
/home/ian/flatc/flatc --python -o tests/hil/ bpio.fbs
```

## Troubleshooting

**"No Bus Pirate device found"** — Device not connected or ports not visible.
Check `ls /dev/ttyACM*` or use `--terminal-port`/`--binary-port` overrides.
On WSL, you may need USB passthrough (`usbipd`).

**"picotool not found"** — The flash tool falls back to UF2 file copy if
picotool isn't installed. Install picotool from the Raspberry Pi Pico SDK,
or the flash tool will look for a mounted `RP2 Boot` volume.

**Tests time out** — The Bus Pirate needs ~1 second after USB connection before
it's ready. If you see timeouts, try increasing `--terminal-rows 24` or check
that the VT100 size reply is working (`test_terminal_size_detection` should pass).

**Hint tests show empty output** — The hint ghost-text is rendered as ANSI dim
text. Make sure `strip_ansi()` isn't stripping the hint itself. Check with
`--debug-serial` to see raw traffic.
