#!/usr/bin/env python3
"""
txtest_capture.py — Host-side capture & verify for the Bus Pirate txtest command.

Usage:
    python3 txtest_capture.py /dev/ttyACM0
    python3 txtest_capture.py /dev/ttyACM0 -l 2 -n 16384 -p 1
    python3 txtest_capture.py /dev/ttyACM0 --sweep          # run all combos

The script:
  1. Opens the serial port
  2. Waits for the Bus Pirate prompt (">")
  3. Sends "txtest [-l N] [-n SIZE] [-p P]\\r"  (CR only — no LF!)
  4. Captures raw bytes between TXTEST:START and TXTEST:END markers
  5. Drains until the next prompt so no garbage is left in the buffer
  6. Generates the same pattern locally and compares byte-by-byte
"""

import argparse
import re
import sys
import time


# ---------------------------------------------------------------------------
# Helpers
# ---------------------------------------------------------------------------

def crc16(data: bytes) -> int:
    """CRC-16 matching the firmware implementation (CRC-16/MODBUS)."""
    crc = 0xFFFF
    for byte in data:
        crc ^= byte
        for _ in range(8):
            if crc & 1:
                crc = (crc >> 1) ^ 0xA001
            else:
                crc >>= 1
    return crc & 0xFFFF


def gen_pattern(pattern_id: int, length: int) -> bytes:
    """Generate the same pattern the firmware produces.

    The firmware fills the full buffer with: buf[i] = base_pat[i % len(base_pat)]
    so we just tile the base pattern to the requested length.
    """
    if pattern_id == 0:
        pat = bytes(range(256))
    elif pattern_id == 1:
        pat = b"\x1b[0m"
    elif pattern_id == 2:
        pat = b"\x1b[1;34mAA\x1b[0m"
    else:
        raise ValueError(f"Unknown pattern {pattern_id}")

    repeats = (length // len(pat)) + 1
    return (pat * repeats)[:length]


def open_serial(port: str, baud: int = 115200):
    """Open serial port with pyserial."""
    import serial
    return serial.Serial(port, baud, timeout=0.1)


def drain(ser, timeout: float = 0.3) -> bytes:
    """Read and discard everything available, return what was consumed."""
    buf = bytearray()
    deadline = time.monotonic() + timeout
    while time.monotonic() < deadline:
        chunk = ser.read(4096)
        if chunk:
            buf.extend(chunk)
            deadline = time.monotonic() + timeout  # reset on activity
        else:
            time.sleep(0.01)
    return bytes(buf)


def wait_for_prompt(ser, timeout: float = 5.0) -> bool:
    """Read until we see '>' (the Bus Pirate prompt). Returns True on success."""
    deadline = time.monotonic() + timeout
    buf = bytearray()
    while time.monotonic() < deadline:
        chunk = ser.read(256)
        if chunk:
            buf.extend(chunk)
            if b">" in buf:
                return True
        else:
            time.sleep(0.01)
    return False


def read_until(ser, marker: bytes, timeout: float = 30.0) -> bytes:
    """Read from serial until marker is found. Returns everything read."""
    buf = bytearray()
    deadline = time.monotonic() + timeout
    while time.monotonic() < deadline:
        chunk = ser.read(4096)
        if chunk:
            buf.extend(chunk)
            if marker in buf:
                return bytes(buf)
        else:
            time.sleep(0.01)
    return bytes(buf)


def diff_report(expected: bytes, got: bytes, max_errors: int = 50) -> int:
    """Byte-level diff report between expected and received data."""
    errors = 0
    min_len = min(len(expected), len(got))
    for i in range(min_len):
        if expected[i] != got[i]:
            errors += 1
            if errors <= max_errors:
                print(
                    f"  MISMATCH @ offset {i}: "
                    f"expected 0x{expected[i]:02X} got 0x{got[i]:02X}"
                )
    if len(expected) != len(got):
        diff = len(expected) - len(got)
        if diff > 0:
            print(f"  DROPPED {diff} bytes (expected {len(expected)}, got {len(got)})")
        else:
            print(f"  EXTRA {-diff} bytes (expected {len(expected)}, got {len(got)})")
        errors += abs(diff)

    if errors > max_errors:
        print(f"  ... and {errors - max_errors} more mismatches")
    return errors


# ---------------------------------------------------------------------------
# Core test runner
# ---------------------------------------------------------------------------

def run_test(ser, layer: int, nbytes: int, pattern: int,
             verbose: bool = False) -> bool:
    """Run a single txtest and verify the output. Returns True on success."""
    cmd_str = f"txtest -l {layer} -n {nbytes} -p {pattern}"
    print(f"\n{'='*60}")
    print(f"  Layer {layer} | {nbytes} bytes | Pattern {pattern}")
    print(f"  Command: {cmd_str}")
    print(f"{'='*60}")

    # ---- 1. Sync: wait for the prompt so we know the BP is ready ----------
    #      Send a bare CR to provoke a fresh prompt if one isn't pending.
    ser.reset_input_buffer()
    ser.write(b"\r")
    if not wait_for_prompt(ser, timeout=5.0):
        print("  ERROR: Bus Pirate not responding (no prompt)")
        return False

    # Flush anything extra that came with the prompt (toolbar, VT100, etc.)
    time.sleep(0.05)
    ser.reset_input_buffer()

    # ---- 2. Send command (CR only — Bus Pirate linenoise expects \r) ------
    ser.write(cmd_str.encode() + b"\r")

    # ---- 3. Wait for START marker -----------------------------------------
    start_data = read_until(ser, b"TXTEST:START:", timeout=10.0)
    if b"TXTEST:START:" not in start_data:
        print("  ERROR: Never received START marker")
        if verbose and start_data:
            print(f"  Raw received ({len(start_data)} bytes): "
                  f"{start_data[:200]!r}")
        drain(ser)
        return False

    # ---- 4. Parse START line ----------------------------------------------
    start_idx = start_data.index(b"TXTEST:START:")
    newline_idx = start_data.index(b"\r\n", start_idx)
    start_line = start_data[start_idx:newline_idx].decode("ascii",
                                                           errors="replace")
    parts = start_line.split(":")
    if len(parts) >= 5:
        fw_layer   = int(parts[2])
        fw_nbytes  = int(parts[3])
        fw_pattern = int(parts[4])
        print(f"  FW reports: layer={fw_layer} nbytes={fw_nbytes} "
              f"pattern={fw_pattern}")
    else:
        print(f"  WARNING: Could not parse START line: {start_line!r}")
        fw_nbytes  = nbytes
        fw_pattern = pattern

    # Everything after START's \r\n is the beginning of the payload
    payload = bytearray(start_data[newline_idx + 2:])

    # ---- 5. Read payload until END marker ---------------------------------
    # For small payloads the END marker may already be in start_data,
    # so check before doing another blocking read.
    if b"TXTEST:END:" not in payload:
        end_data = read_until(ser, b"TXTEST:END:", timeout=60.0)
        payload.extend(end_data)

    raw = bytes(payload)
    end_marker = b"\r\nTXTEST:END:"
    if end_marker not in raw:
        print(f"  ERROR: Never received END marker (got {len(raw)} bytes)")
        if verbose:
            print(f"  Last 100 bytes: {raw[-100:]!r}")
        drain(ser)
        return False

    end_idx  = raw.index(end_marker)
    captured = raw[:end_idx]

    # Parse CRC from END line
    end_rest  = raw[end_idx + len(end_marker):]
    crc_match = re.search(rb"([0-9A-Fa-f]{4})", end_rest)
    fw_crc    = int(crc_match.group(1), 16) if crc_match else None

    # ---- 6. Drain until next prompt so the buffer is clean ----------------
    drain(ser, timeout=0.5)

    # ---- 7. Compare -------------------------------------------------------
    expected     = gen_pattern(fw_pattern, fw_nbytes)
    expected_crc = crc16(expected)
    got_crc      = crc16(captured)

    print(f"  Captured: {len(captured)} bytes (expected {fw_nbytes})")
    if fw_crc is not None:
        print(f"  CRC:  firmware=0x{fw_crc:04X}  "
              f"expected=0x{expected_crc:04X}  got=0x{got_crc:04X}")

    if captured == expected:
        print("  ✅ PASS — all bytes match")
        return True
    else:
        print("  ❌ FAIL — mismatch detected:")
        diff_report(expected, captured)
        return False


# ---------------------------------------------------------------------------
# Main
# ---------------------------------------------------------------------------

def main():
    parser = argparse.ArgumentParser(
        description="Bus Pirate txtest capture & verify"
    )
    parser.add_argument("port", help="Serial port (e.g. /dev/ttyACM0)")
    parser.add_argument("-b", "--baud", type=int, default=115200,
                        help="Baud rate")
    parser.add_argument("-l", "--layer", type=int, default=1,
                        choices=[1, 2, 3])
    parser.add_argument("-n", "--nbytes", type=int, default=4096,
                        help="Bytes to send (64-32768)")
    parser.add_argument("-p", "--pattern", type=int, default=0,
                        choices=[0, 1, 2])
    parser.add_argument("--sweep", action="store_true",
                        help="Test all layer/pattern combos")
    parser.add_argument("--sizes", nargs="+", type=int,
                        default=[64, 512, 4096, 16384, 32768],
                        help="Sizes to test in sweep mode (max 32768)")
    parser.add_argument("-v", "--verbose", action="store_true")
    args = parser.parse_args()

    ser = open_serial(args.port, args.baud)
    print(f"Opened {args.port} @ {args.baud}")

    # Initial sync — send CR, wait for prompt, drain any toolbar/banner junk
    time.sleep(0.3)
    ser.write(b"\r")
    if not wait_for_prompt(ser, timeout=5.0):
        print("ERROR: Bus Pirate not responding on startup")
        ser.close()
        return 1
    drain(ser, timeout=0.3)
    print("Bus Pirate prompt detected — ready")

    results = []

    if args.sweep:
        for layer in [1, 2, 3]:
            for pat in [0, 1, 2]:
                for size in args.sizes:
                    ok = run_test(ser, layer, size, pat, args.verbose)
                    results.append((layer, size, pat, ok))
    else:
        ok = run_test(ser, args.layer, args.nbytes, args.pattern,
                      args.verbose)
        results.append((args.layer, args.nbytes, args.pattern, ok))

    # Summary
    print(f"\n{'='*60}")
    print("SUMMARY")
    print(f"{'='*60}")
    passed = sum(1 for r in results if r[3])
    failed = len(results) - passed
    for layer, size, pat, ok in results:
        status = "✅ PASS" if ok else "❌ FAIL"
        print(f"  L{layer} P{pat} {size:>6} bytes  {status}")
    print(f"\n  {passed}/{len(results)} passed, {failed} failed")

    ser.close()
    return 0 if failed == 0 else 1


if __name__ == "__main__":
    sys.exit(main())
