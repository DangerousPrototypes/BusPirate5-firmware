# txtest — TX Pipeline Saturation Test

A firmware command + host-side Python script for stress-testing the Bus Pirate's USB TX pipeline. Built to detect byte-drop bugs that cause display corruption in interactive tools like the hex editor.

## Background

The Bus Pirate's output path is:

```
Core 0                          Core 1
─────────────────────           ──────────────────────
printf / tx_fifo_put            tx_fifo_service()
        │                               │
        ▼                               ▼
   SPSC queue ──────────────►  tud_cdc_n_write()
   (1024 bytes)                 (64-byte USB packets)
```

- **Core 0** enqueues bytes into a 1024-byte single-producer single-consumer (SPSC) lock-free queue.
- **Core 1** drains the queue and pushes data to the USB host via TinyUSB CDC in 64-byte chunks.

When Core 0 writes faster than Core 1 can drain — or when escape-heavy VT100 output inflates the byte count — the pipeline can drop or corrupt bytes. The hex editor's original per-byte `\x1b[0m` reset pattern (320 resets/frame ≈ 1,280 extra bytes) was enough to trigger this under fast key-repeat.

The `txtest` command lets you blast known patterns at controlled sizes through specific layers of this pipeline, while the Python script captures and verifies every byte on the host side.

## Firmware Command

**Hidden command** — does not appear in `help` output.

### Syntax

```
txtest [-l <1-3>] [-n <64-32768>] [-p <0-2>]
txtest -h
```

### Flags

| Flag | Long | Default | Description |
|------|------|---------|-------------|
| `-l` | `--layer` | 1 | TX pipeline layer to test (see below) |
| `-n` | `--nbytes` | 4096 | Number of bytes to send (64–32768) |
| `-p` | `--pattern` | 0 | Test pattern (see below) |
| `-h` | | | Show help |

### Layers

| Layer | Path | What it tests |
|-------|------|---------------|
| **1** | `tx_fifo_put()` per byte | The exact `_putchar` → SPSC path that `printf` uses. Slowest — maximum per-byte overhead. |
| **2** | `tx_fifo_write(buf, len)` | Blocking bulk write into the SPSC queue. Tests the queue's throughput without per-byte function call overhead. |
| **3** | `tx_fifo_write()` + `tx_fifo_wait_drain()` per 1KB chunk | Simulates the hex editor's `charbuf_draw` flush pattern: write a frame's worth of data, wait for it to drain, repeat. |

### Patterns

| ID | Name | Content | Why |
|----|------|---------|-----|
| **0** | Counter | `0x00 0x01 … 0xFF` repeating | Easy to spot single-byte drops — every offset is unique within a 256-byte window. |
| **1** | VT100 Reset | `\x1b[0m` repeating | The exact 4-byte sequence that triggered the original hex editor corruption. If the `\x1b` byte is dropped, the terminal sees literal `[0m`. |
| **2** | VT100 Mixed | `\x1b[1;34mAA\x1b[0m` repeating | Simulates colored hex editor output — bold blue attribute, two data chars, reset. 13 bytes/cycle with two escape sequences. |

### Wire Protocol

```
TXTEST:START:<layer>:<nbytes>:<pattern>\r\n
<nbytes raw bytes of pattern data>
\r\nTXTEST:END:<CRC16>\r\n
```

- **CRC-16/MODBUS** (polynomial 0xA001, init 0xFFFF) computed over the raw pattern bytes.
- START and END markers are always sent via `printf` with `tx_fifo_wait_drain()` before each, ensuring clean framing.

### Memory

The pattern buffer is allocated from the system's 128KB **big_buffer** (`mem_alloc` / `mem_free`) — the same pool used by the text editor, hex editor, logic analyzer, and scope. Zero static RAM cost; the buffer is held only during the test and released immediately after.

If the big_buffer is already in use (e.g., another command has it), txtest prints an error and exits cleanly.

## Host Script

### Requirements

- Python 3.6+
- `pyserial` (`pip install pyserial`)

### Usage

```bash
# Single test — layer 1, 4KB, counter pattern
python txtest_capture.py /dev/ttyACM0

# Specific layer and size
python txtest_capture.py /dev/ttyACM0 -l 2 -n 32768

# VT100 reset pattern (the one that broke the hex editor)
python txtest_capture.py /dev/ttyACM0 -p 1

# Full sweep — 3 layers × 3 patterns × 5 sizes = 45 tests
python txtest_capture.py /dev/ttyACM0 --sweep

# Custom sweep sizes
python txtest_capture.py /dev/ttyACM0 --sweep --sizes 64 1024 8192 32768

# Verbose — show raw data on failure
python txtest_capture.py /dev/ttyACM0 --sweep -v
```

On Windows, use `COM34` (or whatever port) instead of `/dev/ttyACM0`.

### What the Script Does

1. Opens the serial port and waits for the Bus Pirate `>` prompt
2. Sends the `txtest` command (CR only — no LF; Bus Pirate linenoise expects `\r`)
3. Captures everything between `TXTEST:START:` and `TXTEST:END:` markers
4. Generates the identical pattern locally using the same algorithm
5. Compares byte-by-byte: reports offset, expected vs. actual for each mismatch
6. Verifies CRC-16 matches between firmware-computed and locally-computed values
7. Drains the serial buffer after each test so no garbage contaminates the next run

### Output

```
============================================================
  Layer 2 | 32768 bytes | Pattern 1
  Command: txtest -l 2 -n 32768 -p 1
============================================================
  FW reports: layer=2 nbytes=32768 pattern=1
  Captured: 32768 bytes (expected 32768)
  CRC:  firmware=0xA1B2  expected=0xA1B2  got=0xA1B2
  ✅ PASS — all bytes match
```

On failure:

```
  ❌ FAIL — mismatch detected:
  MISMATCH @ offset 1847: expected 0x1B got 0x5B
  MISMATCH @ offset 1848: expected 0x5B got 0x30
  DROPPED 1 bytes (expected 4096, got 4095)
```

## Methodology

### Why Three Layers

Each layer isolates a different part of the pipeline:

- **Layer 1** (putchar): If this fails but layer 2 passes, the per-byte `tx_fifo_put` path has a bug — likely a race between the SPSC producer and consumer.
- **Layer 2** (bulk write): If this fails, the SPSC queue itself or the Core 1 drain loop is dropping bytes under sustained load.
- **Layer 3** (chunked + drain): If this passes but layer 2 fails, the issue is that Core 1 can't keep up with continuous writes — the drain pauses between chunks give it time to catch up.

### Why Three Patterns

- **Pattern 0** (counter): Any single dropped byte shifts all subsequent offsets, making drops trivially detectable. Also tests the full 0x00–0xFF byte range including NUL, which some paths might mishandle.
- **Pattern 1** (VT100 reset): Directly reproduces the hex editor bug. The `\x1b` byte (ESC, 0x1B) is the critical one — if dropped, the terminal interprets `[0m` as literal text.
- **Pattern 2** (VT100 mixed): Tests the realistic color-change-per-byte pattern that the hex editor's original code produced. Multiple escape sequences per cycle.

### Regression Testing

Run `--sweep` after any change to:
- `usb_tx.c` (SPSC queue, `tx_fifo_service`, `tx_fifo_write`)
- `printf-4.0.0/printf.c` (`_putchar` implementation)
- Core 1 main loop or USB task scheduling
- Any VT100-heavy interactive tool (hex editor, scope, logic analyzer toolbar)

A clean sweep (45/45 pass) means the TX pipeline is solid at all throughput levels.

## Files

| File | Description |
|------|-------------|
| `tools/txtest/txtest_capture.py` | Host-side capture & verify script |
| `tools/txtest/README.md` | This document |
| `src/commands/global/txtest.c` | Firmware command implementation |
| `src/commands/global/txtest.h` | Firmware command header |
| `src/pirate/mem.h` | `BP_BIG_BUFFER_TXTEST` owner enum |
| `src/translation/base.h` | Translation key IDs (`T_HELP_TXTEST_*`) |
| `src/translation/en-us.h` | English translation strings |
