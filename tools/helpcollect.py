#!/usr/bin/env python3
"""
Collect all Bus Pirate help menus (-h output) and produce a single markdown file.

Uses a .txt command script (same format as autosnip.py) to drive a Bus Pirate
over serial. Lines beginning with ``# <heading>`` create new sections in the
markdown output; every other non-blank line is sent as a command.

VT100/ANSI escape sequences are stripped so the markdown contains only plain
text.

Usage
-----
    python helpcollect.py -p /dev/ttyACM0 -i helpref-snip.txt -o help_reference.md
"""

import argparse
import re
import time
from serial import Serial


# ---------------------------------------------------------------------------
# VT100 / ANSI stripping
# ---------------------------------------------------------------------------

# Matches all common ANSI/VT100 escape sequences
_ANSI_RE = re.compile(
    r"""
      \x1b \[ [0-9;]* [A-Za-z]     # CSI sequences  (e.g. \x1b[1;31m)
    | \x1b \] [^\x07]* \x07         # OSC sequences  (e.g. title set)
    | \x1b [()][AB012]              # Character set   (e.g. \x1b(B)
    | \x1b [78]                     # Save/Restore cursor
    | \x1b \[ \? [0-9;]* [A-Za-z]  # Private mode sequences
    | \x07                          # BEL
    | \x08                          # Backspace
    """,
    re.VERBOSE,
)


def strip_ansi(text: str) -> str:
    """Remove all ANSI / VT100 escape sequences, leaving plain text."""
    return _ANSI_RE.sub("", text)


def clean_output(raw: str) -> str:
    """Clean raw captured serial output into tidy plain text."""
    text = raw
    # Remove Bus Pirate prompt marker
    text = text.replace("\x03", "")
    # Remove carriage returns (keep newlines)
    text = text.replace("\r", "")
    # Strip ANSI / VT100 codes
    text = strip_ansi(text)
    # Collapse runs of 3+ blank lines into 2
    text = re.sub(r"\n{3,}", "\n\n", text)
    # Remove leading blank lines
    text = text.lstrip("\n")
    # Remove trailing whitespace
    text = text.rstrip() + "\n"
    return text


# ---------------------------------------------------------------------------
# Serial communication helpers (mirrors autosnip.py logic)
# ---------------------------------------------------------------------------

def handle_vt100_query(serial_conn, buf: str, prev: str) -> str:
    """Respond to VT100 device-status / cursor-position queries."""
    vt100_query = "\x1b7\x1b[999;999H\x1b[6n\x1b8"
    vt100_reply = "[24;80R"  # no ESC prefix — matches autosnip.py / BP expectation
    combined = prev + buf
    if vt100_query in combined:
        serial_conn.write(vt100_reply.encode("utf-8"))
        return ""  # reset prev
    return buf  # keep as prev for next iteration


# ---------------------------------------------------------------------------
# Main
# ---------------------------------------------------------------------------

def parse_arguments():
    p = argparse.ArgumentParser(
        description="Collect Bus Pirate help menus into a single markdown file."
    )
    p.add_argument("-p", "--port", default="/dev/ttyACM0",
                   help="Serial port (default: /dev/ttyACM0)")
    p.add_argument("-b", "--baudrate", type=int, default=115200,
                   help="Baud rate (default: 115200)")
    p.add_argument("-i", "--input", default="helpref-snip.txt",
                   help="Command script file (default: helpref-snip.txt)")
    p.add_argument("-o", "--output", default="help_reference.md",
                   help="Output markdown file (default: help_reference.md)")
    p.add_argument("-d", "--debug", action="store_true",
                   help="Print raw serial traffic to stdout")
    p.add_argument("-t", "--timeout", type=float, default=3.0,
                   help="Extra seconds to wait for slow commands (default: 3)")
    return p.parse_args()


def read_commands(path: str) -> list[str]:
    with open(path) as fh:
        return [line.rstrip("\n") for line in fh]


def main():
    args = parse_arguments()
    lines = read_commands(args.input)

    md_parts: list[str] = []          # collected markdown sections
    prev_data: str = ""               # for split VT100-query detection

    with Serial(args.port, args.baudrate, timeout=1) as ser:
        if args.debug:
            print(f"Connected to {args.port} at {args.baudrate} baud.")

        # Wait for initial prompt
        time.sleep(1)
        ser.reset_input_buffer()

        # Send a bare CR to provoke a prompt
        ser.write(b"\r")
        _wait_prompt(ser, args.debug, prev_data, timeout=args.timeout)

        for line in lines:
            stripped = line.strip()

            # ── blank lines: accept menu defaults silently ───────────
            if stripped == "":
                ser.write(b" \r")
                prev_data = _wait_prompt(ser, args.debug, prev_data,
                                         timeout=args.timeout)
                continue

            # ── comment / control lines ──────────────────────────────
            if stripped.startswith("#"):
                comment = stripped.lstrip("#").strip()
                if comment.lower() == "done":
                    break
                # all other comments are just annotations — skip
                if args.debug:
                    print(f"  # {comment}")
                continue

            # ── command lines ────────────────────────────────────────
            # strip inline comment (command # comment)
            if "#" in line:
                cmd = line.split("#")[0].strip()
            else:
                cmd = stripped

            if not cmd:
                continue

            is_help_cmd = cmd.rstrip().endswith("-h")

            if args.debug:
                tag = "[CAPTURE]" if is_help_cmd else "[setup]"
                print(f">>> {tag} Sending: {cmd!r}")

            ser.write((cmd + "\r").encode("utf-8"))
            prev_data = _wait_prompt(
                ser, args.debug, prev_data,
                timeout=args.timeout,
            )

            # Only capture output for -h commands
            if is_help_cmd:
                captured = _last_captured
                # Use the command (without -h) as the section heading
                heading = cmd.rstrip()[:-2].rstrip()
                md_parts.append(_format_section(heading, captured))

    # Write the markdown file
    header = (
        "# Bus Pirate Command Help Reference\n\n"
        "> Auto-generated by `helpcollect.py`. Do not edit manually.\n\n"
    )
    with open(args.output, "w") as fh:
        fh.write(header)
        fh.write("\n".join(md_parts))
        fh.write("\n")

    print(f"\nDone — wrote {args.output}  ({len(md_parts)} sections)")


# ---------------------------------------------------------------------------
# Prompt wait with capture
# ---------------------------------------------------------------------------

_last_captured: str = ""  # side-channel for captured output


def _wait_prompt(ser: Serial, debug: bool, prev_data: str, *,
                 timeout: float = 3.0,
                 capture_buf=None) -> str:
    """Block until the Bus Pirate prompt (0x03) is seen.

    Returns the updated ``prev_data`` for VT100-query tracking.
    Sets module-level ``_last_captured`` with everything received.
    """
    global _last_captured
    buf = ""
    deadline = time.time() + 30  # hard 30-second safety ceiling
    idle_since = time.time()

    while time.time() < deadline:
        waiting = ser.in_waiting
        if waiting:
            chunk = ser.read(waiting).decode("utf-8", errors="ignore")
            if debug:
                print(chunk, end="", flush=True)
            buf += chunk
            idle_since = time.time()

            prev_data = handle_vt100_query(ser, chunk, prev_data)

            if "\x03" in chunk:
                _last_captured = buf
                return prev_data
        else:
            # No data — short sleep
            time.sleep(0.05)
            # If we haven't seen anything for `timeout` seconds, give up
            if time.time() - idle_since > timeout:
                if debug:
                    print(f"[timeout — no prompt after {timeout}s of silence]")
                _last_captured = buf
                return prev_data

    # Hard deadline hit
    if debug:
        print("[hard timeout — 30s]")
    _last_captured = buf
    return prev_data


# ---------------------------------------------------------------------------
# Markdown formatting
# ---------------------------------------------------------------------------

def _format_section(heading: str, raw: str) -> str:
    """Turn a heading + raw VT100 capture into a markdown section."""
    clean = clean_output(raw)
    return f"## {heading}\n\n```\n{clean}```\n"


if __name__ == "__main__":
    main()
