"""
bp_terminal_client.py — Bus Pirate terminal serial client (CDC 0).

Wraps pyserial communication with the Bus Pirate human terminal port.
Handles VT100 size queries, prompt detection, and output capture.
"""
# Copyright (c) 2024 Ian Lesnet
# Modified by contributors to the BusPirate5-firmware project

import re
import time

import serial


# ---------------------------------------------------------------------------
# ANSI / VT100 stripping (from tools/helpcollect.py)
# ---------------------------------------------------------------------------

_ANSI_RE = re.compile(
    r"""
      \x1b \[ [0-9;]* [A-Za-z]     # CSI sequences
    | \x1b \] [^\x07]* \x07         # OSC sequences
    | \x1b [()][AB012]              # Character set
    | \x1b [78]                     # Save/Restore cursor
    | \x1b \[ \? [0-9;]* [A-Za-z]  # Private mode sequences
    | \x07                          # BEL
    | \x08                          # Backspace
    """,
    re.VERBOSE,
)

# VT100 terminal-size query sent by the Bus Pirate
_VT100_QUERY = "\x1b7\x1b[999;999H\x1b[6n\x1b8"


def strip_ansi(text: str) -> str:
    """Remove all ANSI / VT100 escape sequences, leaving plain text."""
    return _ANSI_RE.sub("", text)


def clean_output(raw: str) -> str:
    """Convert raw captured serial output to tidy plain text."""
    text = raw.replace("\x03", "").replace("\r", "")
    text = strip_ansi(text)
    text = re.sub(r"\n{3,}", "\n\n", text)
    return text.lstrip("\n").rstrip() + "\n"


class TerminalClient:
    """Synchronous serial client for the Bus Pirate CDC 0 terminal port.

    Parameters
    ----------
    port:
        Serial device path (e.g. ``/dev/ttyACM0``).
    baudrate:
        Baud rate — 115200 matches firmware default.
    rows, cols:
        Terminal size reported in response to VT100 size queries.
    prompt_timeout:
        Seconds to wait for a prompt (``\\x03``) before giving up.
    debug:
        Print raw serial traffic to stdout when ``True``.
    """

    def __init__(
        self,
        port: str = "/dev/ttyACM0",
        baudrate: int = 115200,
        rows: int = 24,
        cols: int = 80,
        prompt_timeout: float = 5.0,
        debug: bool = False,
    ) -> None:
        self.port = port
        self.baudrate = baudrate
        self.rows = rows
        self.cols = cols
        self.prompt_timeout = prompt_timeout
        self.debug = debug
        self._ser: serial.Serial | None = None
        self._prev_chunk: str = ""  # for split VT100-query detection
        self.connect()

    # ------------------------------------------------------------------
    # Connection management
    # ------------------------------------------------------------------

    def connect(self) -> None:
        """Open the serial port and wait for the device to be ready."""
        self._ser = serial.Serial(self.port, self.baudrate, timeout=0.1)
        # Give the device ~1 second to settle after USB enumeration
        time.sleep(1)
        self._ser.reset_input_buffer()
        self._prev_chunk = ""

    def reconnect(self) -> None:
        """Close and re-open the serial port (use after reset/reflash)."""
        self.close()
        time.sleep(1)
        self.connect()

    def close(self) -> None:
        """Close the serial port."""
        if self._ser and self._ser.is_open:
            self._ser.close()
        self._ser = None

    # ------------------------------------------------------------------
    # VT100 query handling
    # ------------------------------------------------------------------

    def _handle_vt100_query(self, chunk: str) -> None:
        """Detect and respond to the Bus Pirate VT100 terminal-size query.

        The query may be split across two consecutive reads, so we check
        the concatenation of the previous chunk and the current chunk.
        """
        combined = self._prev_chunk + chunk
        if _VT100_QUERY in combined:
            # Reply has NO ESC prefix — this is what the firmware expects
            reply = f"[{self.rows};{self.cols}R"
            if self.debug:
                print(f"[VT100 query detected, replying: {reply!r}]")
            self._ser.write(reply.encode("utf-8"))
            self._prev_chunk = ""
        else:
            self._prev_chunk = chunk

    # ------------------------------------------------------------------
    # Send / receive
    # ------------------------------------------------------------------

    def send_command(self, cmd: str) -> None:
        """Write a command string to the terminal port (no waiting)."""
        self._ser.write(cmd.encode("utf-8"))

    def wait_for_prompt(self) -> str:
        """Block until the Bus Pirate prompt (``\\x03``) is seen.

        Returns the raw accumulated bytes (including escape sequences).
        Raises ``TimeoutError`` if neither the prompt nor any data arrives
        within the configured timeouts.
        """
        buf = ""
        deadline = time.time() + 30  # hard 30-second safety ceiling
        idle_since = time.time()

        while time.time() < deadline:
            waiting = self._ser.in_waiting
            if waiting:
                chunk = self._ser.read(waiting).decode("utf-8", errors="ignore")
                if self.debug:
                    print(chunk, end="", flush=True)
                buf += chunk
                idle_since = time.time()

                self._handle_vt100_query(chunk)

                if "\x03" in chunk:
                    return buf
            else:
                time.sleep(0.05)
                if time.time() - idle_since > self.prompt_timeout:
                    if self.debug:
                        print(f"[prompt timeout after {self.prompt_timeout}s of silence]")
                    return buf

        if self.debug:
            print("[hard 30s timeout reached]")
        return buf

    def run_command(self, cmd: str) -> tuple[str, str]:
        """Send *cmd* (should end with ``\\r``) and wait for the prompt.

        Returns a tuple of ``(raw_output, clean_output)`` where *raw_output*
        preserves all escape sequences and *clean_output* has them stripped.
        """
        self.send_command(cmd)
        raw = self.wait_for_prompt()
        return raw, clean_output(raw)

    def consume_startup(self) -> str:
        """Drain any pending startup banner output and return it."""
        self.send_command("\r")
        return self.wait_for_prompt()
