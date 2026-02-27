# Hardware-in-the-Loop Test Fixture Pipeline

## Overview

This document outlines a two-port test fixture that can **flash firmware** to a Bus Pirate and then **drive its terminal** to verify behavior — all from a single Python test harness. The key insight is that the Bus Pirate exposes two USB CDC interfaces simultaneously, and the BPIO2 FlatBuffers protocol already has `hardware_bootloader` and `hardware_reset` commands.

```
┌─────────────────────────────────────────────────────────────────┐
│  Test Host (PC / CI runner)                                     │
│                                                                 │
│  pytest test runner                                             │
│    │                                                            │
│    ├── BPIOClient (CDC 1 — /dev/ttyACM1)                        │
│    │     FlatBuffers + COBS → reset, bootloader, config, data   │
│    │                                                            │
│    ├── TerminalClient (CDC 0 — /dev/ttyACM0)                    │
│    │     Raw serial → send commands, capture output, assert     │
│    │                                                            │
│    └── FlashTool                                                │
│          picotool load / UF2 copy after bootloader entry        │
│                                                                 │
│  ┌───── USB ─────────────────────────────────────┐              │
│  │  CDC 0  "Bus Pirate CDC"  (terminal)          │              │
│  │  CDC 1  "Bus Pirate BIN"  (BPIO2 protocol)    │              │
│  │  MSC    "Bus Pirate MSC"  (flash storage)     │              │
│  └───────────────────────────────────────────────┘              │
│                        │                                        │
│                   Bus Pirate                                    │
└─────────────────────────────────────────────────────────────────┘
```

---

## 1. Pipeline Stages

### Stage 0 — Build Firmware

Standard CMake cross-compile. Produces a `.uf2` file.

```
cmake --build build/ --target bus_pirate5_rev10   # or whichever target
# Output: build/src/bus_pirate5_rev10.uf2
```

This can run in CI (Docker container with the Pico SDK) or locally. The pipeline only needs the `.uf2` artifact.

### Stage 1 — Enter Bootloader

Use the BPIO2 binary interface (CDC 1) to command the device into USB bootloader mode:

```python
# Send a ConfigurationRequest with hardware_bootloader=true
# via the FlatBuffers/COBS protocol on CDC 1
bpio_client.send_configuration(hardware_bootloader=True)
```

Under the hood this triggers `reset_usb_boot(0x00, 0x00)` on the RP2040/RP2350, which:
- Disconnects USB (both CDC ports disappear)
- Re-enumerates as a USB mass storage device (`RPI-RP2` / `RP2350` / `BP_BOOT`)

**Fallback**: If BPIO2 is unavailable (e.g., first flash of a blank board), enter bootloader by holding the BOOT button during power-on, or use `picotool reboot -f -u` if a previous firmware is running.

### Stage 2 — Flash Firmware

Two options:

| Method | Command | When to use |
|--------|---------|-------------|
| **picotool** | `picotool load build/src/firmware.uf2 -f` | Preferred — no mount needed, handles reboot |
| **UF2 copy** | `cp firmware.uf2 /media/RPI-RP2/` | Fallback — requires udev/automount |

`picotool` is better for CI because it doesn't depend on filesystem automounting and can also verify the flash contents:

```bash
picotool load firmware.uf2 -f        # flash and force-reboot
picotool verify firmware.uf2         # optional: verify flash contents
picotool reboot                      # reboot into application
```

### Stage 3 — Wait for Re-enumeration

After flashing, the device reboots into the new firmware. The test harness must wait for both CDC ports to reappear:

```python
def wait_for_device(vid=0x1209, pid=0x7331, timeout=15):
    """Block until the Bus Pirate re-enumerates with both CDC interfaces."""
    deadline = time.time() + timeout
    while time.time() < deadline:
        # Check /dev/ttyACM* or use pyudev/pyserial.tools.list_ports
        ports = serial.tools.list_ports.comports()
        bp_ports = [p for p in ports if (p.vid, p.pid) == (vid, pid)]
        if len(bp_ports) >= 2:  # CDC 0 + CDC 1
            return sorted(bp_ports, key=lambda p: p.device)
        time.sleep(0.5)
    raise TimeoutError("Bus Pirate did not re-enumerate after flash")
```

### Stage 4 — Device Ready Handshake

Before running tests, confirm the firmware is alive and responsive:

```python
# Option A: BPIO2 status query on CDC 1
status = bpio_client.get_status()
assert status.version_firmware_major >= expected_major
assert status.mode_current == "HiZ"

# Option B: Terminal prompt check on CDC 0
terminal.write(b"\r")
output = terminal.read_until_prompt()
assert "HiZ>" in output
```

### Stage 5 — Run Tests

Tests execute against the live device using two channels:

```python
class TestVT100Toolbar:
    """Test the toolbar system via the terminal interface."""
    
    def test_statusbar_appears(self, terminal, bpio):
        """Verify the statusbar renders on a terminal with enough rows."""
        # Tell the BP we have a 24-row terminal by answering the VT100 query
        terminal.set_terminal_size(rows=24, cols=80)
        terminal.send_command("")  # trigger prompt + statusbar redraw
        output = terminal.capture_raw()
        # Check for scroll region escape sequence
        assert "\x1b[1;" in output  # scroll region set
    
    def test_v_command_shows_pin_info(self, terminal):
        """Verify the 'v' command outputs voltage readings."""
        terminal.send_command("v")
        output = terminal.read_until_prompt()
        clean = strip_ansi(output)
        assert "V" in clean  # voltage reading present
    
    def test_mode_change_via_bpio(self, bpio, terminal):
        """Change mode via BPIO2, verify terminal reflects it."""
        bpio.send_configuration(mode="SPI")
        terminal.send_command("")
        output = terminal.read_until_prompt()
        assert "SPI>" in strip_ansi(output)
    
    def test_logic_bar_renders(self, terminal):
        """Verify the logic analyzer bar draws without corruption."""
        terminal.send_command("logic")
        output = terminal.capture_raw(timeout=2)
        # Should see cursor save, scroll region change, bar content
        assert "\x1b7" in output or "\x1b[s" in output
```

### Stage 6 — Reset Between Tests

Use the BPIO2 `hardware_reset` command to return to a known state:

```python
@pytest.fixture(autouse=True)
def reset_device(bpio, terminal):
    """Reset the Bus Pirate to HiZ mode before each test."""
    yield
    # After test: reset to clean state
    bpio.send_configuration(hardware_reset=True)
    wait_for_device()
    terminal.reconnect()
```

Or, for a lighter-weight reset that doesn't require re-enumeration, send `m 1` (HiZ mode) via the terminal:

```python
@pytest.fixture(autouse=True)
def soft_reset(terminal):
    yield
    terminal.send_command("m 1")  # return to HiZ
    terminal.read_until_prompt()
```

---

## 2. Core Python Components

### 2.1 BPIO2 Client (`bp_bpio_client.py`)

Wraps the FlatBuffers/COBS protocol for CDC 1:

```python
class BPIOClient:
    """Binary protocol client for Bus Pirate BPIO2 interface (CDC 1)."""
    
    COBS_DELIMITER = b'\x00'
    VERSION_MAJOR = 2
    VERSION_MINOR = 2
    
    def __init__(self, port: str, baudrate: int = 115200):
        self.ser = Serial(port, baudrate, timeout=2)
    
    def send_configuration(self, **kwargs) -> ConfigurationResponse:
        """Send a ConfigurationRequest. kwargs map to flatbuffer fields:
           mode, hardware_bootloader, hardware_reset, psu_enable, etc."""
        buf = self._build_config_request(**kwargs)
        self._send_packet(buf)
        return self._read_response(ConfigurationResponse)
    
    def get_status(self, queries=None) -> StatusResponse:
        """Send a StatusRequest, return parsed StatusResponse."""
        buf = self._build_status_request(queries)
        self._send_packet(buf)
        return self._read_response(StatusResponse)
    
    def data_transaction(self, write=None, read_count=0, 
                         start=False, stop=False) -> DataResponse:
        """Send a DataRequest for protocol I/O."""
        buf = self._build_data_request(write, read_count, start, stop)
        self._send_packet(buf)
        return self._read_response(DataResponse)
    
    def enter_bootloader(self):
        """Command the device into USB bootloader mode."""
        self.send_configuration(hardware_bootloader=True)
        self.ser.close()
    
    def reset(self):
        """Hard reset the device via watchdog."""
        self.send_configuration(hardware_reset=True)
        self.ser.close()
    
    def _send_packet(self, flatbuf_bytes: bytes):
        encoded = cobs_encode(flatbuf_bytes) + self.COBS_DELIMITER
        self.ser.write(encoded)
    
    def _read_response(self, response_type):
        raw = self.ser.read_until(self.COBS_DELIMITER)
        decoded = cobs_decode(raw.rstrip(self.COBS_DELIMITER))
        return parse_response(decoded, response_type)
```

**Dependencies**: `flatbuffers` (Python package, generated from `bpio.fbs`), `cobs` (or hand-rolled — it's ~20 lines), `pyserial`.

Generate the Python FlatBuffers bindings:

```bash
flatc --python bpio.fbs    # produces bpio/ Python package
```

### 2.2 Terminal Client (`bp_terminal_client.py`)

Wraps CDC 0 for human-readable terminal interaction. Adapted from the existing patterns in [tools/helpcollect.py](../tools/helpcollect.py):

```python
class TerminalClient:
    """Serial terminal client for Bus Pirate CDC 0."""
    
    PROMPT_MARKER = "\x03"  # Bus Pirate sends 0x03 to mark prompt
    VT100_SIZE_QUERY = "\x1b7\x1b[999;999H\x1b[6n\x1b8"
    
    def __init__(self, port: str, baudrate: int = 115200, 
                 rows: int = 24, cols: int = 80):
        self.ser = Serial(port, baudrate, timeout=1)
        self.rows = rows
        self.cols = cols
        self._prev_data = ""
    
    def send_command(self, cmd: str) -> str:
        """Send a command and wait for the prompt. Returns captured output."""
        self.ser.write((cmd + "\r").encode("utf-8"))
        return self.read_until_prompt()
    
    def read_until_prompt(self, timeout: float = 5.0) -> str:
        """Read serial data until the prompt marker (0x03) is seen."""
        buf = ""
        deadline = time.time() + timeout
        while time.time() < deadline:
            waiting = self.ser.in_waiting
            if waiting:
                chunk = self.ser.read(waiting).decode("utf-8", errors="ignore")
                buf += chunk
                self._handle_vt100_queries(chunk)
                if self.PROMPT_MARKER in chunk:
                    return buf
            else:
                time.sleep(0.02)
        return buf  # timeout — return what we have
    
    def capture_raw(self, timeout: float = 2.0) -> str:
        """Capture raw output including VT100 escape sequences."""
        buf = ""
        deadline = time.time() + timeout
        while time.time() < deadline:
            waiting = self.ser.in_waiting
            if waiting:
                chunk = self.ser.read(waiting).decode("utf-8", errors="ignore")
                buf += chunk
                self._handle_vt100_queries(chunk)
            else:
                time.sleep(0.02)
        return buf
    
    def set_terminal_size(self, rows: int, cols: int):
        """Set the size reported in VT100 cursor position replies."""
        self.rows = rows
        self.cols = cols
    
    def _handle_vt100_queries(self, chunk: str):
        """Auto-respond to VT100 cursor-position / device-status queries."""
        combined = self._prev_data + chunk
        if self.VT100_SIZE_QUERY in combined:
            reply = f"[{self.rows};{self.cols}R"
            self.ser.write(reply.encode("utf-8"))
            self._prev_data = ""
        else:
            self._prev_data = chunk[-50:]  # keep tail for split detection
    
    def reconnect(self, port: str = None):
        """Close and re-open the serial connection."""
        self.ser.close()
        time.sleep(0.5)
        self.ser = Serial(port or self.ser.port, self.ser.baudrate, timeout=1)
```

### 2.3 Flash Tool (`bp_flash.py`)

Wraps the build + flash sequence:

```python
class FlashTool:
    """Flash firmware to a Bus Pirate via picotool or UF2 copy."""
    
    @staticmethod
    def flash_picotool(uf2_path: str, force_reboot: bool = True) -> bool:
        """Flash using picotool. Device must be in bootloader mode."""
        cmd = ["picotool", "load", uf2_path]
        if force_reboot:
            cmd.append("-f")  # force reboot after load
        result = subprocess.run(cmd, capture_output=True, text=True, timeout=30)
        return result.returncode == 0
    
    @staticmethod
    def flash_uf2_copy(uf2_path: str, mount_point: str = "/media/RPI-RP2"):
        """Flash by copying UF2 to the bootloader mass storage device."""
        deadline = time.time() + 10
        while not os.path.isdir(mount_point) and time.time() < deadline:
            time.sleep(0.5)
        if not os.path.isdir(mount_point):
            raise FileNotFoundError(f"Bootloader volume not found at {mount_point}")
        shutil.copy2(uf2_path, mount_point)
    
    @staticmethod
    def build(target: str = "bus_pirate5_rev10", 
              build_dir: str = "build") -> str:
        """Run CMake build, return path to .uf2 file."""
        subprocess.run(
            ["cmake", "--build", build_dir, "--target", target],
            check=True, timeout=120
        )
        return f"{build_dir}/src/{target}.uf2"
```

---

## 3. pytest Fixtures (`conftest.py`)

```python
import pytest
import serial.tools.list_ports

BP_VID, BP_PID = 0x1209, 0x7331

def discover_bus_pirate():
    """Find CDC 0 (terminal) and CDC 1 (binary) port paths."""
    ports = serial.tools.list_ports.comports()
    bp_ports = sorted(
        [p for p in ports if (p.vid, p.pid) == (BP_VID, BP_PID)],
        key=lambda p: p.device
    )
    if len(bp_ports) < 2:
        pytest.skip("No Bus Pirate found — need 2 CDC ports")
    return bp_ports[0].device, bp_ports[1].device  # CDC 0, CDC 1


@pytest.fixture(scope="session")
def flash_firmware(request):
    """Session-scoped: build and flash firmware once per test run."""
    uf2 = request.config.getoption("--uf2", default=None)
    if uf2 is None:
        # Build from source
        uf2 = FlashTool.build()
    
    cdc0, cdc1 = discover_bus_pirate()
    
    # Enter bootloader via BPIO2
    client = BPIOClient(cdc1)
    client.enter_bootloader()
    time.sleep(2)
    
    # Flash
    FlashTool.flash_picotool(uf2)
    
    # Wait for re-enumeration
    time.sleep(5)
    return discover_bus_pirate()


@pytest.fixture(scope="session")
def bpio(flash_firmware):
    """Session-scoped BPIO2 client on CDC 1."""
    _, cdc1 = flash_firmware
    client = BPIOClient(cdc1)
    yield client
    client.ser.close()


@pytest.fixture(scope="session")
def terminal(flash_firmware):
    """Session-scoped terminal client on CDC 0."""
    cdc0, _ = flash_firmware
    client = TerminalClient(cdc0, rows=24, cols=80)
    # Consume any startup output
    client.read_until_prompt(timeout=3)
    yield client
    client.ser.close()


@pytest.fixture(autouse=True)
def soft_reset(terminal):
    """Reset to HiZ mode between tests (no reboot needed)."""
    yield
    terminal.send_command("m 1")
    terminal.read_until_prompt()
```

---

## 4. Full Pipeline Sequence Diagram

```
Test Host                          Bus Pirate                   USB Bus
    │                                  │                           │
    │  ┌── Stage 0: Build ──┐          │                           │
    │  │ cmake --build       │          │                           │
    │  │ → firmware.uf2      │          │                           │
    │  └─────────────────────┘          │                           │
    │                                   │                           │
    │── Stage 1: Enter Bootloader ─────→│                           │
    │  BPIO2 CDC1: ConfigurationRequest │                           │
    │  { hardware_bootloader: true }    │                           │
    │                                   │── reset_usb_boot() ──────→│
    │                                   │   USB disconnect          │
    │                                   │   Re-enum as mass storage │
    │                                   │                           │
    │── Stage 2: Flash ────────────────→│                           │
    │  picotool load firmware.uf2 -f    │                           │
    │                                   │── Reboot into FW ────────→│
    │                                   │   USB re-enumerate        │
    │                                   │   CDC0 + CDC1 appear      │
    │                                   │                           │
    │── Stage 3: Wait for enum ────────→│                           │
    │  poll /dev/ttyACM* for VID:PID    │                           │
    │  until 2 CDC ports found          │                           │
    │                                   │                           │
    │── Stage 4: Ready handshake ──────→│                           │
    │  CDC1: StatusRequest              │                           │
    │  ←── StatusResponse (version, mode)                           │
    │  CDC0: "\r"                       │                           │
    │  ←── "HiZ> " (prompt)            │                           │
    │                                   │                           │
    │── Stage 5: Run tests ────────────→│                           │
    │  CDC0: send command               │                           │
    │  ←── capture output               │                           │
    │  CDC1: send BPIO2 request         │                           │
    │  ←── parse BPIO2 response         │                           │
    │  assert results                   │                           │
    │                                   │                           │
    │── Stage 6: Reset between tests ──→│                           │
    │  CDC0: "m 1\r" (soft reset)       │                           │
    │  ←── "HiZ> "                      │                           │
    │                                   │                           │
    │  [repeat Stage 5-6 per test]      │                           │
```

---

## 5. VT100/Toolbar-Specific Test Strategies

Since this pipeline was motivated by the toolbar refactoring (see [vt100_toolbar_analysis.md](vt100_toolbar_analysis.md)), here are the specific testing strategies for that work:

### 5.1 Escape Sequence Validation

Capture raw output (don't strip ANSI) and parse the VT100 sequences programmatically:

```python
import re

CSI_RE = re.compile(r'\x1b\[([0-9;]*?)([A-Za-z])')

def parse_vt100(raw: str) -> list[tuple[str, str]]:
    """Extract all CSI sequences as (params, command) tuples."""
    return CSI_RE.findall(raw)

def test_scroll_region_set(terminal):
    """After startup with 24-row terminal, scroll region should be set."""
    terminal.set_terminal_size(rows=24, cols=80)
    terminal.send_command("")
    raw = terminal.capture_raw()
    sequences = parse_vt100(raw)
    # Look for scroll region: \033[1;20r  (top=1, bottom=rows-statusbar)
    scroll_regions = [(p, c) for p, c in sequences if c == 'r']
    assert len(scroll_regions) > 0
    params = scroll_regions[0][0]  # e.g. "1;20"
    top, bottom = params.split(';')
    assert int(top) == 1
    assert int(bottom) == 20  # 24 - 4 (statusbar height)
```

### 5.2 Screen Buffer Simulation

For more rigorous testing, build a virtual terminal that processes the VT100 stream and maintains a screen buffer:

```python
class VirtualScreen:
    """Minimal VT100 screen buffer for test assertions."""
    
    def __init__(self, rows=24, cols=80):
        self.rows = rows
        self.cols = cols
        self.buffer = [[' '] * cols for _ in range(rows)]
        self.cursor_row = 0
        self.cursor_col = 0
        self.scroll_top = 0
        self.scroll_bottom = rows - 1
    
    def feed(self, data: str):
        """Process a stream of characters + VT100 escape sequences."""
        # ... parse and apply cursor moves, erases, scroll regions, text
    
    def get_line(self, row: int) -> str:
        return ''.join(self.buffer[row]).rstrip()
    
    def get_region(self, top: int, bottom: int) -> list[str]:
        return [self.get_line(r) for r in range(top, bottom + 1)]

def test_statusbar_content(terminal):
    screen = VirtualScreen(24, 80)
    terminal.send_command("v")
    raw = terminal.capture_raw()
    screen.feed(raw)
    # Statusbar occupies rows 21-24 (bottom 4 lines)
    statusbar = screen.get_region(20, 23)
    assert any("V" in line for line in statusbar)  # voltage reading
```

### 5.3 Interleaving / Corruption Detection

Test the race condition described in the analysis by generating heavy output while the statusbar is active:

```python
def test_no_statusbar_corruption_under_load(terminal, bpio):
    """Flood the terminal with output and check statusbar doesn't corrupt."""
    terminal.set_terminal_size(rows=24, cols=80)
    
    # Trigger continuous output (e.g., SPI read loop)
    bpio.send_configuration(mode="SPI")
    terminal.send_command("m 6")  # enter SPI mode via terminal
    terminal.read_until_prompt()
    
    # Rapid-fire commands while statusbar is updating
    raw_output = ""
    for _ in range(20):
        terminal.send_command("[0xff r:16]")  # SPI write+read
        raw_output += terminal.capture_raw(timeout=0.5)
    
    # Check for corruption: orphaned escape sequences, broken UTF-8,
    # cursor positions outside the scroll region
    sequences = parse_vt100(raw_output)
    for params, cmd in sequences:
        if cmd == 'H':  # cursor position
            row, col = (params.split(';') + ['1', '1'])[:2]
            row, col = int(row or 1), int(col or 1)
            # Cursor should never be in the statusbar region during 
            # regular output (only during statusbar update)
            # This is a heuristic — not perfect, but catches gross errors
```

---

## 6. CI Integration

### GitHub Actions Workflow (self-hosted runner with USB device)

```yaml
name: Hardware-in-the-Loop Tests

on:
  push:
    branches: [main]
  pull_request:

jobs:
  hil-test:
    runs-on: [self-hosted, bus-pirate]   # runner with BP attached via USB
    steps:
      - uses: actions/checkout@v4
      
      - name: Build firmware
        run: |
          cmake -B build -DPICO_BOARD=bus_pirate5_rev10
          cmake --build build --target bus_pirate5_rev10
      
      - name: Install test dependencies
        run: pip install pytest pyserial flatbuffers cobs
      
      - name: Generate FlatBuffer bindings
        run: flatc --python bpio.fbs
      
      - name: Run HIL tests
        run: pytest tests/hil/ -v --uf2 build/src/bus_pirate5_rev10.uf2
        timeout-minutes: 10
```

### udev Rule (for consistent port names)

Use the existing udev rule from [hacks/88-buspirate.rules](../hacks/88-buspirate.rules), or create symlinks:

```udev
# /etc/udev/rules.d/99-buspirate-test.rules
SUBSYSTEM=="tty", ATTRS{idVendor}=="1209", ATTRS{idProduct}=="7331", \
    ENV{ID_USB_INTERFACE_NUM}=="00", SYMLINK+="buspirate_terminal"
SUBSYSTEM=="tty", ATTRS{idVendor}=="1209", ATTRS{idProduct}=="7331", \
    ENV{ID_USB_INTERFACE_NUM}=="02", SYMLINK+="buspirate_binary"
```

This gives stable paths `/dev/buspirate_terminal` and `/dev/buspirate_binary` regardless of ACM numbering.

---

## 7. Directory Structure

```
tests/
├── hil/                          # Hardware-in-the-loop tests
│   ├── conftest.py               # pytest fixtures (discover, flash, connect)
│   ├── bp_bpio_client.py         # BPIO2 FlatBuffers/COBS client
│   ├── bp_terminal_client.py     # Terminal serial client
│   ├── bp_flash.py               # Flash tool wrapper
│   ├── bp_vt100_parser.py        # VT100 escape sequence parser
│   ├── bp_virtual_screen.py      # Virtual terminal screen buffer
│   ├── test_basic_commands.py    # Smoke tests: prompt, help, version
│   ├── test_mode_switching.py    # Mode enter/exit via terminal + BPIO2
│   ├── test_statusbar.py         # Statusbar rendering, scroll regions
│   ├── test_toolbar_registry.py  # Toolbar enable/disable/layout
│   ├── test_logic_bar.py         # Logic analyzer bar rendering
│   ├── test_vt100_primitives.py  # Color, cursor, erase sequences
│   └── test_pin_display.py       # Pin names/labels/voltages ('v' command)
├── stubs/                        # Existing Pico SDK mocks
│   └── ...
├── test_spsc_queue.c             # Existing host-side unit test
└── run_tests.sh                  # Existing test runner
```

---

## 8. Implementation Priority

| Priority | Component | Effort | Value |
|----------|-----------|--------|-------|
| **1** | `bp_terminal_client.py` | Low | Unlocks all terminal tests; pattern exists in `helpcollect.py` |
| **2** | `conftest.py` (discovery + fixtures) | Low | Glue that makes pytest work |
| **3** | `test_basic_commands.py` | Low | Smoke tests — validate the fixture itself works |
| **4** | `bp_bpio_client.py` | Medium | Requires FlatBuffers + COBS; enables flash pipeline |
| **5** | `bp_flash.py` + bootloader stage | Medium | Full CI pipeline; needs picotool on runner |
| **6** | `bp_vt100_parser.py` | Medium | Enables toolbar/statusbar assertions |
| **7** | `bp_virtual_screen.py` | High | Full screen simulation; needed for corruption detection |
| **8** | Toolbar-specific test suites | Medium | The actual VT100 refactoring validation |

The terminal client and basic command tests can be written **immediately** — they need only `pyserial` and a plugged-in Bus Pirate. The BPIO2 client and flash pipeline can follow incrementally.
